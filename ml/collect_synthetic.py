import pandas as pd
import numpy as np

rng = np.random.default_rng(42)

def samples(n, temp_range, hum_range, press_range, label):
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
