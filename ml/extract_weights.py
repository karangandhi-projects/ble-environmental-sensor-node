#!/usr/bin/env python3
"""ml/extract_weights.py — regenerate the classifier weights inside
firmware/components/tinyml_inference/include/ml_weights.h from the Keras
SavedModel at ml/models/saved_model.

This is the script train_classifier.py and the ml_weights.h header point at.
Without it, the deployed classifier weights cannot be reproduced from
committed code.

Scope:
    UPDATES from the SavedModel:
        ML_W1, ML_b1, ML_W2, ML_b2, ML_W3, ML_b3
        (the 245-weight 3 -> 16 -> 8 -> 5 MLP).
    PRESERVES verbatim (read from the existing ml_weights.h):
        - The top file doc-comment.
        - The #defines (ML_INPUT_SIZE, ML_LAYER1_SIZE, ML_LAYER2_SIZE,
          ML_OUTPUT_SIZE, ML_AE_HIDDEN_SIZE, ML_ANOMALY_THRESHOLD).
        - The autoencoder block (ML_AE_We, ML_AE_be, ML_AE_Wd, ML_AE_bd).
          DD-019 replaced AE-based anomaly detection with confidence
          thresholding, so these arrays are dead at runtime. They are kept
          here for now; see docs/REVIEW_FINDINGS.md item B5 for the planned
          removal.
    UPDATES (only when --accuracy is provided):
        The "/* Classifier: 3-16-8-5 MLP, accuracy X.XXXX */" header line.

Usage:
    cd ml
    source .venv/bin/activate              # any env with tensorflow>=2.13
    python3 train_classifier.py            # writes models/saved_model
    python3 extract_weights.py [--accuracy 0.9883]
    cd ../firmware && idf.py build         # picks up the new weights

The script edits an existing ml_weights.h in place — it does not generate a
file from scratch. The skeleton (header doc-comment, #defines, AE block)
must already exist. This is intentional: those parts are stable across
retrainings, and a regex-replace is safer than a full template render
when the only thing actually changing is the classifier numbers.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEFAULT_SAVED_MODEL = ROOT / "models" / "saved_model"
DEFAULT_OUT_PATH = (
    ROOT.parent
    / "firmware"
    / "components"
    / "tinyml_inference"
    / "include"
    / "ml_weights.h"
)

# (kernel_var_name, bias_var_name) per layer, in input -> output order.
LAYER_VAR_NAMES = [
    ("sequential/dense/kernel:0", "sequential/dense/bias:0"),
    ("sequential/dense_1/kernel:0", "sequential/dense_1/bias:0"),
    ("sequential/dense_2/kernel:0", "sequential/dense_2/bias:0"),
]
ARRAY_NAMES = [("ML_W1", "ML_b1"), ("ML_W2", "ML_b2"), ("ML_W3", "ML_b3")]

CLASSIFIER_BLOCK_RE = re.compile(
    r"/\* --- Classifier weights --- \*/\n.*?(?=/\* --- Autoencoder weights)",
    re.DOTALL,
)
ACCURACY_LINE_RE = re.compile(
    r"^/\* Classifier: 3-16-8-5 MLP, accuracy [0-9.]+ \*/$", re.MULTILINE
)


def fmt_array(name: str, values: list[float]) -> str:
    """Render one `static const float NAME[N] = { ... };` block.

    The final entry has no trailing comma — matches the hand-written style
    of the existing ml_weights.h.
    """
    rows = [f"    {v:.8f}f" for v in values]
    body = ",\n".join(rows)
    return f"static const float {name}[{len(values)}] = {{\n{body}\n}};\n"


def render_classifier_block(
    weights: list[tuple[str, list[float]]],
) -> str:
    """Render the entire `/* --- Classifier weights --- */` section,
    ending with a blank line so the following AE marker sits flush."""
    arrays = [fmt_array(name, vals) for name, vals in weights]
    return "/* --- Classifier weights --- */\n" + "\n".join(arrays) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--accuracy",
        type=float,
        default=None,
        help="Override the classifier accuracy in the file header line.",
    )
    parser.add_argument(
        "--saved-model",
        default=str(DEFAULT_SAVED_MODEL),
        help=f"Path to the Keras SavedModel (default: {DEFAULT_SAVED_MODEL}).",
    )
    parser.add_argument(
        "--out",
        default=str(DEFAULT_OUT_PATH),
        help=f"Output header path (default: {DEFAULT_OUT_PATH}).",
    )
    args = parser.parse_args()

    saved_model_path = Path(args.saved_model)
    out_path = Path(args.out)

    if not saved_model_path.exists():
        sys.exit(
            f"SavedModel not found at {saved_model_path}. "
            "Run train_classifier.py first."
        )
    if not out_path.exists():
        sys.exit(
            f"{out_path} does not exist. This script edits in place; the "
            "skeleton (header, #defines, AE block) is preserved across runs."
        )

    # Lazy TF import — keeps `--help` fast and avoids paying the TF startup
    # cost when only validating args.
    import tensorflow as tf

    loaded = tf.saved_model.load(str(saved_model_path))
    var_by_name = {v.name: v.numpy() for v in loaded.variables}

    missing = [n for pair in LAYER_VAR_NAMES for n in pair if n not in var_by_name]
    if missing:
        sys.exit(
            "SavedModel is missing expected variables: "
            + ", ".join(missing)
            + ". Did train_classifier.py change the architecture or layer names?"
        )

    flats: list[tuple[str, list[float]]] = []
    for (k_name, b_name), (w_arr, b_arr) in zip(LAYER_VAR_NAMES, ARRAY_NAMES):
        kernel = var_by_name[k_name]  # shape (in, out)
        bias = var_by_name[b_name]  # shape (out,)
        # Keras stores Dense kernel as (in, out). tinyml_inference.c iterates
        # W[i * in_size + j] over (out, in) row-major, so transpose first.
        kernel_rm = kernel.T.reshape(-1)
        flats.append((w_arr, kernel_rm.tolist()))
        flats.append((b_arr, bias.tolist()))

    text = out_path.read_text()
    new_block = render_classifier_block(flats)
    if not CLASSIFIER_BLOCK_RE.search(text):
        sys.exit(
            "Could not locate the '/* --- Classifier weights --- */' ... "
            "'/* --- Autoencoder weights' markers in the existing file. "
            "Restore the markers and re-run."
        )
    new_text = CLASSIFIER_BLOCK_RE.sub(new_block, text, count=1)

    if args.accuracy is not None:
        if not ACCURACY_LINE_RE.search(new_text):
            sys.exit(
                "Could not locate the accuracy header line "
                "'/* Classifier: 3-16-8-5 MLP, accuracy X.XXXX */'."
            )
        new_text = ACCURACY_LINE_RE.sub(
            f"/* Classifier: 3-16-8-5 MLP, accuracy {args.accuracy:.4f} */",
            new_text,
            count=1,
        )

    out_path.write_text(new_text)
    total = sum(len(v) for _, v in flats)
    print(f"Wrote {out_path} ({total} classifier weights regenerated)")
    if args.accuracy is not None:
        print(f"  Updated accuracy header to {args.accuracy:.4f}")
    print(
        "  Autoencoder block preserved (REVIEW_FINDINGS.md B5 will delete it)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
