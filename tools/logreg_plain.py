"""실험 6 의 로지스틱 회귀를 평문으로 계산한다 (암호화 없이 개념 보기용).

모델 W (10 x 784), b (10) 는 exec/mnist_data.h 에서 읽는다 (실험 6 이 쓰는 것과 같은 값).
    점수  z_k = <w_k, x> + b_k        (k = 0 ~ 9)
    확률  softmax(z)                    (학습할 때 쓰는 값. 답을 고를 때는 z 가 가장 큰 k 만 보면 된다)

사용법 (저장소 맨 위에서):
    python3 tools/logreg_plain.py images/builtin_7.png   # MNIST 형식 그림 (검은 배경, 흰 글씨, 28 의 배수 크기)
    python3 tools/logreg_plain.py my.txt                 # image_to_vector.py 로 만든 파일 (직접 쓴 숫자)
    python3 tools/logreg_plain.py 7                      # 내장 이미지 7 번 (0 ~ 10)

필요한 패키지: numpy (png 를 읽을 때는 pillow 도)
"""
import argparse
import re
from pathlib import Path

import numpy as np

HEADER = Path(__file__).resolve().parent.parent / "exec" / "mnist_data.h"
ZOOM = 10   # export_mnist_png.py 가 28 x 28 을 키우는 배율


def read_array(text, name):
    m = re.search(rf"\b{name}\[[^=]*=\s*\{{(.*?)\}};", text, re.S)
    if m is None:
        raise SystemExit(f"[오류] {HEADER} 에서 {name} 을 찾지 못했습니다.")
    return np.array([float(v) for v in re.findall(r"-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?", m.group(1))])


def load_model():
    text = HEADER.read_text(encoding="utf-8")
    w = read_array(text, "mnist_w").reshape(10, 784)
    b = read_array(text, "mnist_b")
    pixels = read_array(text, "mnist_pixel").reshape(-1, 784)
    labels = read_array(text, "mnist_label").astype(int)
    return w, b, pixels, labels


def read_png(path):
    """MNIST 형식 그림 (검은 배경, 흰 글씨) 을 28 x 28 로 줄여 픽셀값 / 255 로 돌려준다."""
    from PIL import Image

    img = np.asarray(Image.open(path).convert("L"), dtype=np.float64)
    h, w = img.shape
    if h != w or h % 28:
        raise SystemExit(f"[오류] '{path}' 는 {w} x {h} 입니다. 28 의 배수인 정사각형 MNIST 형식 그림만 바로 읽습니다.\n"
                         "       직접 쓴 숫자는 image_to_vector.py 로 먼저 바꾸세요.")
    k = h // 28
    return img.reshape(28, k, 28, k).mean(axis=(1, 3)).reshape(784) / 255.0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image", nargs="?", default="0", help="png 그림, 784 개 값이 든 파일, 또는 내장 이미지 번호")
    args = parser.parse_args()

    w, b, pixels, labels = load_model()
    if args.image.isdigit() and int(args.image) < len(pixels):
        x = pixels[int(args.image)] / 255.0
    elif args.image.lower().endswith(".png"):
        x = read_png(args.image)
    else:
        x = np.loadtxt(args.image)
        if x.size != 784:
            raise SystemExit(f"[오류] '{args.image}' 에서 값 784 개를 읽지 못했습니다 (읽은 개수: {x.size}).")

    z = w @ x + b                      # 행렬 W (10 x 784) 와 벡터 x (784) 의 곱 + b
    p = np.exp(z - z.max())
    p /= p.sum()
    pred = int(np.argmax(z))

    print(f"입력: {args.image}\n")
    print(" 숫자 k    <w_k, x>       b_k     점수 z_k    softmax")
    for k in range(10):
        mark = "  <- 예측" if k == pred else ""
        print(f"   {k}    {w[k] @ x:9.4f} {b[k]:9.4f}   {z[k]:9.4f}    {p[k]:7.4f}{mark}")
    print(f"\n예측: {pred}")


if __name__ == "__main__":
    main()
