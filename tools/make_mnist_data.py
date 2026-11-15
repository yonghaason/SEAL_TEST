"""실험 7 용 exec/mnist_data.h 를 만든다 (numpy 만 필요).

MNIST 숫자 0 ~ 9 를 구분하는 10-클래스 로지스틱 회귀 (softmax 회귀) 를 학습하고,
가중치 행렬 W (10 x 784), 상수 b (10), 테스트 이미지 11 장을 C++ 헤더로 내보낸다.

    점수 z = W x + b   (x 는 픽셀값 / 255, 길이 784)
    예측  = z 가 가장 큰 클래스

사용법:
    python3 tools/make_mnist_data.py <MNIST idx.gz 파일 4개가 있는 폴더>
"""
import gzip
import sys
from pathlib import Path

import numpy as np

LAMBDA = 1e-4   # L2 규제 세기
STEPS = 2000    # 경사하강법 반복 횟수
LR = 0.5        # 학습률
OUT = Path(__file__).resolve().parent.parent / "exec" / "mnist_data.h"


def load(folder, prefix):
    with gzip.open(folder / f"{prefix}-images-idx3-ubyte.gz") as f:
        x = np.frombuffer(f.read(), np.uint8, offset=16).reshape(-1, 784)
    with gzip.open(folder / f"{prefix}-labels-idx1-ubyte.gz") as f:
        y = np.frombuffer(f.read(), np.uint8, offset=8)
    return x, y.astype(np.int64)


def softmax(z):
    z = z - z.max(axis=1, keepdims=True)
    p = np.exp(z)
    return p / p.sum(axis=1, keepdims=True)


def train(x, y):
    """전체 데이터로 경사하강법 (모멘텀 0.9). W 는 (10, 784), b 는 (10,)."""
    n = len(y)
    onehot = np.zeros((n, 10))
    onehot[np.arange(n), y] = 1.0
    w = np.zeros((10, 784))
    b = np.zeros(10)
    vw = np.zeros_like(w)
    vb = np.zeros_like(b)
    for _ in range(STEPS):
        g = (softmax(x @ w.T + b) - onehot) / n
        vw = 0.9 * vw - LR * (g.T @ x + LAMBDA * w)
        vb = 0.9 * vb - LR * g.sum(axis=0)
        w += vw
        b += vb
    return w, b


def main():
    folder = Path(sys.argv[1])
    x_train_u8, y_train = load(folder, "train")
    x_test_u8, y_test = load(folder, "t10k")
    x_train, x_test = x_train_u8 / 255.0, x_test_u8 / 255.0

    w, b = train(x_train, y_train)
    z = x_test @ w.T + b
    pred = z.argmax(axis=1)
    print(f"train accuracy: {np.mean((x_train @ w.T + b).argmax(axis=1) == y_train):.4f}")
    print(f"test accuracy : {np.mean(pred == y_test):.4f}  ({len(y_test)} images)")
    print(f"score range: [{z.min():.1f}, {z.max():.1f}],  max |W| = {np.abs(w).max():.2f}")

    # 테스트 세트에서 숫자 0 ~ 9 마다 모델이 맞히는 첫 이미지를 하나씩 고르고,
    # 마지막에 모델이 틀리는 첫 이미지를 하나 덧붙인다 (암호문으로 계산해도 똑같이 틀리는지 보는 용도).
    idx = [int(np.where((y_test == d) & (pred == d))[0][0]) for d in range(10)]
    idx.append(int(np.where(pred != y_test)[0][0]))
    print("chosen test indices:", idx)
    print("labels             :", y_test[idx].tolist())
    print("their predictions  :", pred[idx].tolist())

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// tools/make_mnist_data.py 가 만든 파일. 직접 고치지 않는다.\n")
        f.write("// MNIST 숫자 0 ~ 9 를 구분하는 10-클래스 로지스틱 회귀 모델과 테스트 이미지.\n")
        f.write("//   점수 z = W x + b  (W 는 10 x 784 행렬, x 는 픽셀값 / 255),  예측 = z 가 가장 큰 클래스\n")
        f.write("#pragma once\n\n")
        f.write("inline constexpr int MNIST_DIM = 784;     // 28 x 28 픽셀\n")
        f.write("inline constexpr int MNIST_CLASSES = 10;  // 숫자 0 ~ 9\n")
        f.write("inline constexpr int MNIST_IMAGES = 11;   // 테스트 이미지 수\n\n")
        f.write("// 정답 라벨. 0 ~ 9 번 이미지는 숫자 0 ~ 9 (모델이 맞히는 것), 10 번은 모델이 틀리는 이미지\n")
        f.write("inline const int mnist_label[MNIST_IMAGES] = { "
                + ", ".join(str(int(y_test[i])) for i in idx) + " };\n\n")
        f.write("// 상수 b: 클래스마다 하나\n")
        f.write("inline const double mnist_b[MNIST_CLASSES] = { " + ", ".join(f"{v:.10g}" for v in b) + " };\n\n")
        f.write("// 가중치 행렬 W: mnist_w[k] 가 클래스 k 의 가중치 (길이 784)\n")
        f.write("inline const double mnist_w[MNIST_CLASSES][MNIST_DIM] = {\n")
        for k in range(10):
            f.write("    {\n")
            for r in range(0, 784, 7):
                f.write("        " + ", ".join(f"{v:.8g}" for v in w[k, r:r + 7]) + ",\n")
            f.write("    },\n")
        f.write("};\n\n")
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
