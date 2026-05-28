"""
Generate synthetic training data for the BLE_ENV_NODE environmental classifier.

Produces 1500 samples (300 per class) with uniform random values within each
class's characteristic environmental range. The data is shuffled and saved to
data/synthetic_train.csv for use by train_classifier.py.

Why synthetic data?
  Real sensor data is limited by what can be physically simulated with the
  BLE sensor override. Synthetic data ensures full coverage of each class
  region, including boundary areas that are hard to reach physically.

Usage:
    mkdir -p data
    python3 collect_synthetic.py

Class ranges (match CLASS_ORDER in train_classifier.py and app_config.h):
    comfortable: 18–25°C, 30–60% RH, 1000–1025 hPa
    warm:        28–45°C, 20–50% RH,  990–1020 hPa
    cold:        -5–15°C, 20–50% RH,  985–1015 hPa
    humid:       18–28°C, 75–99% RH,  995–1020 hPa
    danger:      46–60°C,  0–20% RH,  970–995 hPa

Note: ranges are deliberately designed to overlap slightly (e.g. comfortable
and warm both include values near 25°C) to force the model to learn from
multiple features rather than temperature alone. Boundary overlaps also
train the anomaly detector naturally via the classifier confidence threshold.
"""
import pandas as pd
import numpy as np

# Fixed seed for reproducibility — same seed produces the same synthetic dataset.
rng = np.random.default_rng(42)


def samples(n, temp_range, hum_range, press_range, label):
    """Generate n random samples uniformly distributed within the given ranges.

    Args:
        n:           Number of samples to generate.
        temp_range:  (min, max) temperature in °C.
        hum_range:   (min, max) relative humidity in %.
        press_range: (min, max) pressure in hPa.
        label:       Class label string (e.g. 'comfortable').

    Returns:
        DataFrame with columns: timestamp_ms, temp_c, humidity_pct, pressure_hpa, label.
    """
    return pd.DataFrame({
        'timestamp_ms': np.arange(n) * 2000,
        'temp_c':       rng.uniform(*temp_range, n).round(2),
        'humidity_pct': rng.uniform(*hum_range, n).round(2),
        'pressure_hpa': rng.uniform(*press_range, n).round(2),
        'label':        label
    })


df = pd.concat([
    samples(300, (18, 25), (30, 60), (1000, 1025), 'comfortable'),
    samples(300, (28, 45), (20, 50), (990, 1020),  'warm'),
    samples(300, (-5, 15), (20, 50), (985, 1015),  'cold'),
    samples(300, (18, 28), (75, 99), (995, 1020),  'humid'),
    samples(300, (46, 60), (0,  20), (970, 995),   'danger'),
], ignore_index=True).sample(frac=1, random_state=42)

df.to_csv('data/synthetic_train.csv', index=False)
print(f"Generated {len(df)} samples → data/synthetic_train.csv")
print(df['label'].value_counts())
