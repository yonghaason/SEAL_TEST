"""실험 7 용 exec/mnist_data.h 를 만든다 (numpy 만 필요).

MNIST 의 0, 1 만 골라 로지스틱 회귀를 학습하고, 가중치와 테스트 이미지 몇 장을 C++ 헤더로 내보낸다.
로짓이 대략 [-5, 5] 안에 들어오도록 L2 규제를 세게 건다 (3차 시그모이드 근사가 이 범위에서만 맞는다).

사용법:
    python3 tools/make_mnist_data.py <MNIST idx.gz 파일 4개가 있는 폴더>
"""
import gzip
import sys
from pathlib import Path

import numpy as np

LAMBDA = 0.2       # L2 규제 세기
N_IMAGES = 8       # 헤더에 넣을 테스트 이미지 수 (암호문 하나에 8장)
OUT = Path(__file__).resolve().parent.parent / "exec" / "mnist_data.h"


def load(folder, prefix):
    with gzip.open(folder / f"{prefix}-images-idx3-ubyte.gz") as f:
        x = np.frombuffer(f.read(), np.uint8, offset=16).reshape(-1, 784)
    with gzip.open(folder / f"{prefix}-labels-idx1-ubyte.gz") as f:
        y = np.frombuffer(f.read(), np.uint8, offset=8)
    keep = y <= 1
    return x[keep], y[keep].astype(np.float64)


def sigmoid(z):
    return 1.0 / (1.0 + np.exp(-z))


def train(x, y, lam, steps=3000, lr=0.5):
    w = np.zeros(x.shape[1])
    b = 0.0
    for _ in range(steps):
        p = sigmoid(x @ w + b)
        w -= lr * (x.T @ (p - y) / len(y) + lam * w)
        b -= lr * np.mean(p - y)
    return w, b


def main():
    folder = Path(sys.argv[1])
    x_train_u8, y_train = load(folder, "train")
    x_test_u8, y_test = load(folder, "t10k")
    x_train, x_test = x_train_u8 / 255.0, x_test_u8 / 255.0

    lam = float(sys.argv[2]) if len(sys.argv) > 2 else LAMBDA
    w, b = train(x_train, y_train, lam)
    z = x_test @ w + b
    cubic = 0.5 + 0.197 * z - 0.004 * z**3
    print(f"lambda = {lam}, test images = {len(z)}")
    print(f"logit range: [{z.min():.2f}, {z.max():.2f}],  |z| > 5: {np.sum(np.abs(z) > 5)}")
    print(f"accuracy (sigmoid): {np.mean((z > 0) == (y_test == 1)):.4f}")
    print(f"accuracy (cubic)  : {np.mean((cubic > 0.5) == (y_test == 1)):.4f}")
    print(f"max |cubic - sigmoid|: {np.max(np.abs(cubic - sigmoid(z))):.4f}")

    # 테스트 세트 앞에서부터 0 과 1 을 번갈아 고른다.
    idx, want = [], 0
    for i in range(len(y_test)):
        if int(y_test[i]) == want:
            idx.append(i)
            want = 1 - want
        if len(idx) == N_IMAGES:
            break
    print("chosen logits:", np.round(z[idx], 2))

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// tools/make_mnist_data.py 가 만든 파일. 직접 고치지 않는다.\n")
        f.write("// MNIST 숫자 0 과 1 을 구분하는 로지스틱 회귀 모델과 테스트 이미지.\n")
        f.write("//   로짓 z = <w, x> + b,  확률 P(숫자가 1) = sigmoid(z).  x 는 픽셀값 / 255 (0 ~ 1).\n")
        f.write("#pragma once\n\n#include <vector>\n\n")
        f.write("inline constexpr int MNIST_DIM = 784;   // 28 x 28 픽셀\n")
        f.write(f"inline constexpr int MNIST_IMAGES = {N_IMAGES}; // 테스트 이미지 수\n\n")
        f.write(f"inline const double mnist_b = {b:.17g};\n\n")
        f.write("inline const std::vector<double> mnist_w = {\n")
        for r in range(0, 784, 7):
            f.write("    " + ", ".join(f"{v:.10g}" for v in w[r:r + 7]) + ",\n")
        f.write("};\n\n")
        f.write("// 정답 라벨\n")
        f.write("inline const int mnist_label[MNIST_IMAGES] = { "
                + ", ".join(str(int(y_test[i])) for i in idx) + " };\n\n")
        f.write("// 픽셀값 0 ~ 255 (행 우선). 쓸 때는 255 로 나눈다.\n")
        f.write("inline const unsigned char mnist_pixel[MNIST_IMAGES][MNIST_DIM] = {\n")
        for i in idx:
            f.write("    {\n")
            for r in range(28):
                f.write("        " + ",".join(str(v) for v in x_test_u8[i, r * 28:(r + 1) * 28]) + ",\n")
            f.write("    },\n")
        f.write("};\n")
    print("saved:", OUT)


if __name__ == "__main__":
    main()
