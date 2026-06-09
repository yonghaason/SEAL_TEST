import json
import ssl
import math
import certifi
import numpy as np

from pathlib import Path

from sklearn.datasets import fetch_openml
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import accuracy_score


# ------------------------------------------------------------
# SSL certificate fix for fetch_openml
# ------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent

ssl._create_default_https_context = lambda: ssl.create_default_context(
    cafile=certifi.where()
)


# ------------------------------------------------------------
# 1. Load full MNIST: 70000 x 784
# ------------------------------------------------------------
mnist = fetch_openml("mnist_784", version=1, as_frame=False)

X = mnist.data.astype(np.float64) / 255.0
y = mnist.target.astype(np.int64)

print("X:", X.shape)
print("y:", y.shape)


# ------------------------------------------------------------
# 2. StandardScaler over all MNIST data
# ------------------------------------------------------------
scaler = StandardScaler()
X_s = scaler.fit_transform(X)

print("Scaler mean shape :", scaler.mean_.shape)
print("Scaler scale shape:", scaler.scale_.shape)


# ------------------------------------------------------------
# 3. Train 10-class logistic regression on all data
# ------------------------------------------------------------
clf = LogisticRegression(
    penalty="l2",
    C=2.0,
    solver="lbfgs",
    max_iter=5000,
    random_state=0,
    n_jobs=-1,
    verbose=1,
)

clf.fit(X_s, y)

W = clf.coef_.astype(np.float64)        # shape: (10, 784)
b = clf.intercept_.astype(np.float64)   # shape: (10,)

print("W shape:", W.shape)
print("b shape:", b.shape)


# ------------------------------------------------------------
# 4. Training accuracy sanity check
#    전체 데이터를 학습에 썼으므로 이건 test accuracy가 아니라 sanity check.
# ------------------------------------------------------------
logits_s = X_s @ W.T + b
pred_s = np.argmax(logits_s, axis=1)
train_acc = accuracy_score(y, pred_s)

print("Training accuracy:", train_acc)


# ------------------------------------------------------------
# 5. Absorb StandardScaler into model
#
# Original:
#   x_scaled[i] = (x_raw[i] - mean[i]) / std[i]
#   z_k = sum_i W[k,i] * x_scaled[i] + b[k]
#
# Raw-input model:
#   z_k = sum_i W_raw[k,i] * x_raw[i] + b_raw[k]
# where
#   W_raw[k,i] = W[k,i] / std[i]
#   b_raw[k]   = b[k] - sum_i W[k,i] * mean[i] / std[i]
# ------------------------------------------------------------
mean = scaler.mean_.astype(np.float64)
std = scaler.scale_.astype(np.float64)

W_raw = W / std[None, :]
b_raw = b - (W * (mean / std)[None, :]).sum(axis=1)

print("W_raw shape:", W_raw.shape)
print("b_raw shape:", b_raw.shape)


# ------------------------------------------------------------
# 6. Verify equivalence:
#    standardized model and raw-input model must give same logits.
# ------------------------------------------------------------
idx = 0
z1 = W @ X_s[idx] + b
z2 = W_raw @ X[idx] + b_raw

print("equivalence max error:", np.max(np.abs(z1 - z2)))
print("sample true label:", y[idx])
print("sample pred standardized:", int(np.argmax(z1)))
print("sample pred raw-input   :", int(np.argmax(z2)))


# ------------------------------------------------------------
# 7. Export JSON for C++/SEAL
# ------------------------------------------------------------
poly_modulus_degree = 8192
slot_count = poly_modulus_degree // 2
feature_dim = 784

export = {
    "dataset": "MNIST",
    "task": "10-class logistic regression on full MNIST 70000 samples",
    "class_names": [str(i) for i in range(10)],
    "feature_dim": feature_dim,

    "ckks": {
        "poly_modulus_degree_suggested": poly_modulus_degree,
        "slot_count": slot_count,
        "ciphertexts_needed_for_one_image": int(math.ceil(feature_dim / slot_count)),
    },

    "preprocessing": {
        "input_image_shape": [28, 28],
        "flatten_order": "row-major",
        "pixel_scale": "x / 255.0",
        "standard_scaler_mean": mean.tolist(),
        "standard_scaler_scale": std.tolist(),
    },

    "model_standardized_input": {
        "description": "Use with x_scaled[i] = (x_raw[i] - mean[i]) / scale[i].",
        "W_shape": list(W.shape),
        "W": W.tolist(),
        "b": b.tolist(),
    },

    "model_raw_input": {
        "description": "Use directly with x_raw in [0,1]^784. No StandardScaler needed in C++.",
        "formula": "z_k = inner_product(W_raw[k], x_raw) + b_raw[k]",
        "W_raw_shape": list(W_raw.shape),
        "W_raw": W_raw.tolist(),
        "b_raw": b_raw.tolist(),
    },

    "training": {
        "used_all_70000_samples_for_training": True,
        "training_accuracy": float(train_acc),
        "regularization_C": 2.0,
        "solver": "lbfgs",
        "max_iter": 5000,
    },

    "sanity_check": {
        "sample_index": int(idx),
        "true_label": int(y[idx]),
        "logits_standardized": z1.tolist(),
        "logits_raw_input": z2.tolist(),
        "max_abs_difference": float(np.max(np.abs(z1 - z2))),
        "prediction": int(np.argmax(z2)),
    },
}

out_path = SCRIPT_DIR / "mnist_logi_reg_models.json"
with open(out_path, "w", encoding="utf-8") as f:
    json.dump(export, f, indent=2)

print("Saved:", out_path)
