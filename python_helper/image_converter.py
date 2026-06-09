import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from PIL import Image, ImageFilter, ImageOps


def image_to_mnist_vector(
    image_path,
    invert=True,
    threshold=30,
    target_digit_size=20,
    center_by_mass=True,
    contrast_normalize=True,
    contrast_percentile=99.0,
    thicken=False,
    blur_radius=0.0,
    save_debug=False,
    debug_dir="mnist_debug",
    prefix=None,
    show=False,
):
    """
    Convert a hand-drawn image to an MNIST-like 28x28 vector.

    Output:
        x_raw: shape (784,), values in [0,1]
    """

    image_path = Path(image_path)

    if prefix is None:
        prefix = image_path.stem

    debug_dir = Path(debug_dir)
    if save_debug:
        debug_dir.mkdir(parents=True, exist_ok=True)

    img_gray = Image.open(image_path).convert("L")

    if save_debug:
        img_gray.save(debug_dir / f"{prefix}_01_gray.png")

    if invert:
        img_inv = ImageOps.invert(img_gray)
    else:
        img_inv = img_gray.copy()

    if save_debug:
        img_inv.save(debug_dir / f"{prefix}_02_inverted.png")

    arr = np.asarray(img_inv).astype(np.float64)
    arr[arr < threshold] = 0
    img_thresh = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), mode="L")

    if save_debug:
        img_thresh.save(debug_dir / f"{prefix}_03_thresholded.png")

    ys, xs = np.where(arr > 0)

    if len(xs) == 0 or len(ys) == 0:
        raise ValueError("No digit pixels found. Check invert/threshold.")

    left, right = xs.min(), xs.max()
    top, bottom = ys.min(), ys.max()

    cropped = arr[top:bottom + 1, left:right + 1]
    img_crop = Image.fromarray(np.clip(cropped, 0, 255).astype(np.uint8), mode="L")

    if save_debug:
        img_crop.save(debug_dir / f"{prefix}_04_cropped.png")

    w, h = img_crop.size
    if w > h:
        new_w = target_digit_size
        new_h = max(1, int(round(h * target_digit_size / w)))
    else:
        new_h = target_digit_size
        new_w = max(1, int(round(w * target_digit_size / h)))

    img_resized = img_crop.resize((new_w, new_h), Image.Resampling.LANCZOS)

    if save_debug:
        img_resized.save(debug_dir / f"{prefix}_05_resized.png")

    canvas = Image.new("L", (28, 28), color=0)
    paste_left = (28 - new_w) // 2
    paste_top = (28 - new_h) // 2
    canvas.paste(img_resized, (paste_left, paste_top))

    if center_by_mass:
        canvas_arr = np.asarray(canvas).astype(np.float64)
        total = canvas_arr.sum()

        if total > 0:
            yy, xx = np.indices(canvas_arr.shape)
            center_y = float((yy * canvas_arr).sum() / total)
            center_x = float((xx * canvas_arr).sum() / total)

            shift_y = int(round(13.5 - center_y))
            shift_x = int(round(13.5 - center_x))

            shifted = np.zeros_like(canvas_arr)

            src_y0 = max(0, -shift_y)
            src_y1 = min(28, 28 - shift_y)
            dst_y0 = max(0, shift_y)
            dst_y1 = min(28, 28 + shift_y)

            src_x0 = max(0, -shift_x)
            src_x1 = min(28, 28 - shift_x)
            dst_x0 = max(0, shift_x)
            dst_x1 = min(28, 28 + shift_x)

            shifted[dst_y0:dst_y1, dst_x0:dst_x1] = canvas_arr[src_y0:src_y1, src_x0:src_x1]
            canvas = Image.fromarray(np.clip(shifted, 0, 255).astype(np.uint8), mode="L")

    if save_debug:
        canvas.save(debug_dir / f"{prefix}_06_centered_28x28.png")

    img_final = canvas.copy()

    if thicken:
        img_final = img_final.filter(ImageFilter.MaxFilter(3))
        if save_debug:
            img_final.save(debug_dir / f"{prefix}_07_thickened.png")

    if blur_radius is not None and blur_radius > 0:
        img_final = img_final.filter(ImageFilter.GaussianBlur(radius=blur_radius))
        if save_debug:
            img_final.save(debug_dir / f"{prefix}_08_blurred_final.png")

    final_arr = np.asarray(img_final).astype(np.float64)

    if contrast_normalize:
        foreground = final_arr[final_arr > 0]

        if foreground.size > 0:
            scale = np.percentile(foreground, contrast_percentile)
            if scale > 0:
                final_arr = np.clip(final_arr * (255.0 / scale), 0, 255)
                img_final = Image.fromarray(final_arr.astype(np.uint8), mode="L")

    x_raw_2d = final_arr / 255.0
    x_raw = x_raw_2d.reshape(-1)

    if save_debug:
        img_final.save(debug_dir / f"{prefix}_final_mnist_28x28.png")

    if show:
        plt.figure(figsize=(3, 3))
        plt.imshow(x_raw_2d, cmap="gray", vmin=0, vmax=1)
        plt.title("final MNIST-like 28x28")
        plt.axis("off")
        plt.show()

    return x_raw


def write_vector(path, x_raw):
    with open(path, "w", encoding="utf-8") as f:
        for value in x_raw:
            f.write(f"{float(value):.17g}\n")


def main():
    parser = argparse.ArgumentParser(description="Convert an image to a 784-value MNIST vector.")
    parser.add_argument("image_path")
    parser.add_argument("-o", "--output", default=None)
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--show", action="store_true")
    args = parser.parse_args()

    x_raw = image_to_mnist_vector(
        args.image_path,
        save_debug=args.debug,
        show=args.show,
    )

    if args.output:
        write_vector(args.output, x_raw)
    else:
        for value in x_raw:
            print(f"{float(value):.17g}")


if __name__ == "__main__":
    main()
