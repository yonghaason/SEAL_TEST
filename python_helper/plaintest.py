import json
import sys
from pathlib import Path

import numpy as np

from image_converter import image_to_mnist_vector

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_MODEL_PATH = SCRIPT_DIR / "mnist_logi_reg_models.json"


def load_raw_input_model(json_path: str):
    with open(json_path, "r", encoding="utf-8") as f:
        obj = json.load(f)

    feature_dim = obj["feature_dim"]

    if "model_raw_input" in obj:
        W_raw = np.array(obj["model_raw_input"]["W_raw"], dtype=np.float64)
        b_raw = np.array(obj["model_raw_input"]["b_raw"], dtype=np.float64)
    else:
        W = np.array(obj["model"]["W"], dtype=np.float64)
        b = np.array(obj["model"]["b"], dtype=np.float64)
        mean = np.array(obj["preprocessing"]["standard_scaler_mean"], dtype=np.float64)
        std = np.array(obj["preprocessing"]["standard_scaler_scale"], dtype=np.float64)

        W_raw = W / std[None, :]
        b_raw = b - (W * (mean / std)[None, :]).sum(axis=1)

    assert W_raw.shape == (10, feature_dim)
    assert b_raw.shape == (10,)

    return W_raw, b_raw


def plain_inference_raw(x_raw: np.ndarray, W_raw: np.ndarray, b_raw: np.ndarray):
    x_raw = np.asarray(x_raw, dtype=np.float64).reshape(-1)

    if x_raw.shape[0] != W_raw.shape[1]:
        raise ValueError(
            f"x_raw length mismatch: got {x_raw.shape[0]}, expected {W_raw.shape[1]}"
        )

    logits = W_raw @ x_raw + b_raw
    pred = int(np.argmax(logits))

    return logits, pred


def main():
    json_path = DEFAULT_MODEL_PATH

    if len(sys.argv) < 2:
        print("Usage: python python_helper/plaintest.py <image_path>")
        print("Example: python python_helper/plaintest.py my_digit.png")
        sys.exit(1)

    image_path = sys.argv[1]

    if not Path(image_path).exists():
        print(f"Error: image file not found: {image_path}")
        sys.exit(1)

    x_raw = image_to_mnist_vector(image_path)
    W_raw, b_raw = load_raw_input_model(json_path)
    scores, pred = plain_inference_raw(x_raw, W_raw, b_raw)

    print("Scores:")
    for k, score in enumerate(scores):
        print(f"{k}: {score:.6f}")

    print(f"\nPrediction: {pred}")


if __name__ == "__main__":
    main()
