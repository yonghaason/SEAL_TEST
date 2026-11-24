"""exec/mnist_data.h 의 내장 이미지 11 장을 png 파일로 내보낸다 (numpy, pillow 필요).

    python3 tools/export_mnist_png.py          # images/builtin_0.png ~ builtin_10.png

MNIST 원본 그대로 검은 배경에 흰 글씨다. 28 x 28 은 너무 작아서 한 픽셀을 10 x 10 으로 키워 280 x 280 으로 저장한다.
(픽셀을 섞지 않고 그대로 키우므로 logreg_plain.py 가 다시 28 x 28 로 줄이면 원래 값이 그대로 나온다)
"""
from pathlib import Path

import numpy as np
from PIL import Image

from logreg_plain import ZOOM, load_model

OUT = Path(__file__).resolve().parent.parent / "images"


def main():
    _, _, pixels, labels = load_model()
    OUT.mkdir(exist_ok=True)
    for i, p in enumerate(pixels):
        img = Image.fromarray(p.reshape(28, 28).astype(np.uint8), mode="L")
        img = img.resize((28 * ZOOM, 28 * ZOOM), Image.NEAREST)
        path = OUT / f"builtin_{i}.png"
        img.save(path)
        print(f"{path}  (정답 {labels[i]})")


if __name__ == "__main__":
    main()
