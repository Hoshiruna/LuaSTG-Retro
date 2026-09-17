"""Compare the deterministic core 2D fixture's PNG captures, using only the standard library."""

import argparse
from pathlib import Path
import struct
import sys
import zlib


def read_png(path, expect_size=None):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: expected a PNG capture")
    position = 8
    compressed = bytearray()
    width = height = channels = 0
    while position < len(data):
        length, kind = struct.unpack_from(">I4s", data, position)
        payload = data[position + 8:position + 8 + length]
        if len(payload) != length:
            raise ValueError(f"{path}: truncated PNG chunk")
        if kind == b"IHDR":
            width, height, bits, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            if bits != 8 or color not in (2, 6) or compression or filtering or interlace:
                raise ValueError(f"{path}: expected non-interlaced RGB8 or RGBA8 PNG")
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
        position += length + 12
    if expect_size is not None and (width, height) != expect_size:
        raise ValueError(
            f"{path}: expected a {expect_size[0]} x {expect_size[1]} logical-canvas capture;"
            f" got {width} x {height}. Pass --mode exact to compare other sizes,"
            " such as the nested render target."
        )
    stride = width * channels
    raw = zlib.decompress(compressed)
    if len(raw) != height * (stride + 1):
        raise ValueError(f"{path}: unexpected decompressed image length")
    previous = bytearray(stride)
    pixels = []
    for y in range(height):
        offset = y * (stride + 1)
        method = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        for index in range(stride):
            left = row[index - channels] if index >= channels else 0
            up = previous[index]
            corner = previous[index - channels] if index >= channels else 0
            if method == 0:
                predictor = 0
            elif method == 1:
                predictor = left
            elif method == 2:
                predictor = up
            elif method == 3:
                predictor = (left + up) // 2
            elif method == 4:
                estimate = left + up - corner
                distances = (abs(estimate - left), abs(estimate - up), abs(estimate - corner))
                predictor = (left, up, corner)[distances.index(min(distances))]
            else:
                raise ValueError(f"{path}: unknown PNG filter {method}")
            row[index] = (row[index] + predictor) & 255
        for x in range(width):
            rgba = tuple(row[x * channels:(x + 1) * channels])
            pixels.append(rgba if channels == 4 else rgba + (255,))
        previous = row
    return pixels, width, height


def regions():
    yield "background", (610, 20, 620, 30)
    for name, x, y in (("red", 32, 32), ("green", 80, 32), ("blue", 32, 80), ("white", 80, 80)):
        yield name, (x, y, x + 8, y + 8)
    for row in range(8):
        for blend in range(11):
            x, y = 16 + blend * 56, 128 + row * 27
            yield f"alpha={row // 4},vertex={row % 4},blend={blend}", (x + 12, y + 8, x + 36, y + 15)
    yield "nested target", (28, 372, 36, 380)
    yield "depth occlusion", (46, 378, 56, 388)
    yield "scissor inside", (110, 378, 130, 390)
    yield "scissor outside", (96, 360, 102, 370)
    yield "viewport", (370, 370, 398, 398)
    for fog in range(3):
        x = 160 + fog * 64
        yield f"fog={fog + 1}", (x + 8, 368, x + 40, 400)


def compare_canvas(reference, candidate):
    """Check the fixture's known markers, then diff each interior region of the 640 x 480 canvas."""
    expected = {
        (36, 36): (255, 0, 0, 255),
        (84, 36): (0, 255, 0, 255),
        (36, 84): (0, 0, 255, 255),
        (84, 84): (255, 255, 255, 255),
        (615, 25): (32, 48, 64, 255),
        (50, 382): (0, 0, 255, 255),
        (120, 384): (255, 255, 0, 255),
        (380, 380): (0, 255, 255, 255),
    }
    for name, pixels in (("reference", reference), ("candidate", candidate)):
        for (x, y), rgba in expected.items():
            actual = pixels[y * 640 + x]
            if max(abs(a - b) for a, b in zip(actual, rgba)) > 2:
                raise ValueError(f"{name}: marker ({x}, {y}) is {actual}, expected {rgba}; check orientation, channels, depth, and capture timing")
    failures = []
    compared = worst = 0
    for name, (left, top, right, bottom) in regions():
        region_worst = 0
        for y in range(top, bottom):
            for x in range(left, right):
                index = y * 640 + x
                difference = max(abs(a - b) for a, b in zip(reference[index], candidate[index]))
                region_worst = max(region_worst, difference)
                compared += 1
        worst = max(worst, region_worst)
        if region_worst > 2:
            failures.append(f"{name}: maximum channel difference {region_worst}")
    return failures, compared, worst


def compare_exact(reference, candidate, width, height):
    """Diff every pixel of two equally sized captures, with no fixture-specific knowledge."""
    failures = []
    worst = worst_x = worst_y = 0
    for index, (left, right) in enumerate(zip(reference, candidate)):
        difference = max(abs(a - b) for a, b in zip(left, right))
        if difference > worst:
            worst, worst_x, worst_y = difference, index % width, index // width
    if worst > 2:
        failures.append(
            f"whole image: maximum channel difference {worst} at ({worst_x}, {worst_y});"
            f" reference {reference[worst_y * width + worst_x]},"
            f" candidate {candidate[worst_y * width + worst_x]}"
        )
    return failures, width * height, worst


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", help="D3D11 PNG captured with this fixture")
    parser.add_argument("candidate", help="SDL GPU PNG captured with the same font and dimensions")
    parser.add_argument(
        "--mode",
        choices=("canvas", "exact"),
        default="canvas",
        help="canvas: check the fixture's markers and interior regions of the 640 x 480 capture (default)."
             " exact: diff every pixel of two equally sized captures, for the nested render target.",
    )
    args = parser.parse_args()
    expect_size = (640, 480) if args.mode == "canvas" else None
    reference, width, height = read_png(args.reference, expect_size)
    candidate, candidate_width, candidate_height = read_png(args.candidate, expect_size)
    if (width, height) != (candidate_width, candidate_height):
        raise ValueError(
            f"size mismatch: reference is {width} x {height},"
            f" candidate is {candidate_width} x {candidate_height}"
        )
    if args.mode == "canvas":
        failures, compared, worst = compare_canvas(reference, candidate)
        scope = "interior"
    else:
        failures, compared, worst = compare_exact(reference, candidate, width, height)
        scope = f"{width} x {height}"
    for failure in failures:
        print(f"FAIL {failure}")
    print(f"Compared {compared} {scope} pixels; maximum difference {worst}/255; tolerance 2/255.")
    if args.mode == "canvas":
        print("Review linear-filtered edges, vertex gradients, and FreeType text separately.")
    return 1 if failures else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, struct.error, zlib.error) as error:
        print(f"Capture comparison failed: {error}", file=sys.stderr)
        sys.exit(1)
