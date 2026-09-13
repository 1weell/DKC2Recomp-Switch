#!/usr/bin/env python3
"""Compare a flat and a CRT-television capture of the same frame.

Both inputs are binary PPMs written by DKC2_DESKTOP_SCREENSHOT from the same
DKC2_DESKTOP_TEST_LOADSTATE, one with DKC2_DISPLAY=flat and one with
DKC2_DISPLAY=crt. The checks are the plan's acceptance criteria:

  energy   the tube conserves light: the mean linear luminance of the CRT
           capture is within a tolerance of the flat capture's;
  beating  the only periodic structure the beam adds is the scanline pitch:
           in linear light the CRT row profile, smoothed over four pitches,
           follows the flat capture's within a slow scene-following trend,
           with no periodic residual (moire) between the two;
  pitch    the strongest short period in the CRT row profile is the
           expected pixels-per-line pitch.

A crop can be written side by side (flat above CRT) for viewing. No
third-party modules; the images are a few megapixels and are reduced to
per-row and per-value statistics with bytes operations.
"""
import argparse
import sys

SOURCE_LINES = 224
DEFAULT_ENERGY_TOLERANCE = 0.03
DEFAULT_ENVELOPE_TOLERANCE = 0.02


def read_ppm(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if not data.startswith(b"P6"):
        raise SystemExit(f"{path}: not a binary PPM")
    fields = []
    position = 2
    while len(fields) < 3:
        while data[position:position + 1].isspace():
            position += 1
        if data[position:position + 1] == b"#":
            while data[position:position + 1] not in (b"\n", b""):
                position += 1
            continue
        start = position
        while not data[position:position + 1].isspace():
            position += 1
        fields.append(int(data[start:position]))
    position += 1
    width, height, maximum = fields
    if maximum != 255:
        raise SystemExit(f"{path}: only 8-bit PPMs are supported")
    pixels = data[position:position + width * height * 3]
    if len(pixels) != width * height * 3:
        raise SystemExit(f"{path}: truncated pixel data")
    return width, height, pixels


def srgb_to_linear(value):
    c = value / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


LINEAR = [srgb_to_linear(v) for v in range(256)]
LUMA_WEIGHTS = (0.2126, 0.7152, 0.0722)


def mean_linear_luminance(pixels, width, x0, y0, x1, y1):
    """Mean linear luminance over a rectangle, from per-channel histograms."""
    total = 0.0
    count = 0
    for channel, weight in enumerate(LUMA_WEIGHTS):
        histogram = [0] * 256
        for y in range(y0, y1):
            row = pixels[(y * width + x0) * 3 + channel:
                         (y * width + x1) * 3:3]
            for value in set(row):
                histogram[value] += row.count(value)
        total += weight * sum(LINEAR[v] * n for v, n in enumerate(histogram))
        count = sum(histogram)
    return total / count if count else 0.0


def row_profile(pixels, width, height, x0, x1):
    """Mean gray (gamma space) of each row across the columns x0..x1."""
    profile = []
    span = x1 - x0
    for y in range(height):
        row = pixels[(y * width + x0) * 3:(y * width + x1) * 3]
        profile.append(sum(row) / (3.0 * span) if span else 0.0)
    return profile


def linear_row_profile(pixels, width, y0, y1, x0, x1):
    """Mean linear light (channels unweighted) of each row y0..y1 across
    the columns x0..x1, for judging energy along the tube."""
    profile = []
    span = x1 - x0
    lookup = LINEAR.__getitem__
    for y in range(y0, y1):
        row = pixels[(y * width + x0) * 3:(y * width + x1) * 3]
        profile.append(sum(map(lookup, row)) / (3.0 * span) if span else 0.0)
    return profile


def content_rows(profile, threshold=1.0):
    rows = [y for y, value in enumerate(profile) if value > threshold]
    return (rows[0], rows[-1] + 1) if rows else (0, 0)


def gaussian_smooth(values, sigma):
    """Gaussian smoothing (kernel truncated at three sigma, renormalised
    at the ends). Unlike a box the size of a few pitches, a Gaussian with
    sigma above one pitch passes none of the scanline ripple through."""
    if sigma <= 0.0:
        return list(values)
    radius = int(3.0 * sigma + 0.5)
    kernel = [pow(2.718281828, -0.5 * (k / sigma) ** 2)
              for k in range(-radius, radius + 1)]
    out = []
    for i in range(len(values)):
        lo = max(0, i - radius)
        hi = min(len(values), i + radius + 1)
        weights = kernel[lo - i + radius:hi - i + radius]
        total = sum(weights)
        out.append(sum(v * w for v, w in zip(values[lo:hi], weights)) / total)
    return out


def moving_average(values, window):
    if window < 1:
        return list(values)
    out = []
    half = window // 2
    for i in range(len(values)):
        lo = max(0, i - half)
        hi = min(len(values), i + window - half)
        out.append(sum(values[lo:hi]) / (hi - lo))
    return out


def dominant_period(values, minimum, maximum):
    """Fundamental period of the detrended profile: the first local maximum
    of its autocorrelation that reaches half the overall maximum (a
    fractional pitch scores higher at its second multiple, which lands
    nearer an integer lag, so the global maximum alone misleads)."""
    if len(values) < maximum * 3:
        return None, 0.0
    mean = sum(values) / len(values)
    centered = [v - mean for v in values]
    energy = sum(v * v for v in centered) or 1.0
    scores = {}
    for lag in range(minimum, maximum + 1):
        acc = sum(centered[i] * centered[i + lag]
                  for i in range(len(centered) - lag))
        scores[lag] = acc / energy
    peak = max(scores.values())
    for lag in range(minimum + 1, maximum):
        if (scores[lag] >= scores[lag - 1] and scores[lag] >= scores[lag + 1]
                and scores[lag] >= 0.5 * peak):
            return lag, scores[lag]
    best = max(scores, key=scores.get)
    return best, scores[best]


def secondary_period(values, minimum, maximum):
    """The lag with the highest local maximum of the autocorrelation in
    minimum..maximum, and its score (1 = perfectly periodic), or (None, 0)
    when the autocorrelation only decays."""
    if len(values) < maximum + 3 or minimum < 2:
        return None, 0.0
    mean = sum(values) / len(values)
    centered = [v - mean for v in values]
    energy = sum(v * v for v in centered) or 1.0
    scores = {}
    for lag in range(minimum - 1, maximum + 2):
        acc = sum(centered[i] * centered[i + lag]
                  for i in range(len(centered) - lag))
        scores[lag] = acc / energy
    best_lag, best = None, 0.0
    for lag in range(minimum, maximum + 1):
        if (scores[lag] > scores[lag - 1] and scores[lag] >= scores[lag + 1]
                and scores[lag] > best):
            best_lag, best = lag, scores[lag]
    return best_lag, best


def write_crop(path, flat, crt, width, x0, y0, x1, y1):
    crop_width = x1 - x0
    crop_height = y1 - y0
    with open(path, "wb") as out:
        out.write(f"P6\n{crop_width} {crop_height * 2}\n255\n".encode())
        for pixels in (flat, crt):
            for y in range(y0, y1):
                out.write(pixels[(y * width + x0) * 3:(y * width + x1) * 3])


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("flat", help="capture with DKC2_DISPLAY=flat")
    parser.add_argument("crt", help="capture with DKC2_DISPLAY=crt")
    parser.add_argument("--lines", type=int, default=SOURCE_LINES,
                        help="source lines in the frame (default 224)")
    parser.add_argument("--energy-tolerance", type=float,
                        default=DEFAULT_ENERGY_TOLERANCE)
    parser.add_argument("--envelope-tolerance", type=float,
                        default=DEFAULT_ENVELOPE_TOLERANCE)
    parser.add_argument("--crop", metavar="X,Y,W,H",
                        help="write a side-by-side crop (flat over CRT)")
    parser.add_argument("--crop-out", default="crt_compare_crop.ppm")
    args = parser.parse_args()

    width, height, flat = read_ppm(args.flat)
    crt_width, crt_height, crt = read_ppm(args.crt)
    if (crt_width, crt_height) != (width, height):
        raise SystemExit("captures differ in size")

    # The viewport is the vertical extent with content in the flat capture
    # (letterbox rows are black); widescreen frames fill the width.
    flat_rows = row_profile(flat, width, height, 0, width)
    y0, y1 = content_rows(flat_rows)
    if y1 - y0 < args.lines:
        raise SystemExit("no content rows found in the flat capture")
    pitch = (y1 - y0) / args.lines
    # Judge inside the tube face: skip 4% at each edge so curvature,
    # corners, and vignette do not enter the statistics.
    inset_y = int((y1 - y0) * 0.04)
    inset_x = int(width * 0.04)
    jy0, jy1 = y0 + inset_y, y1 - inset_y
    jx0, jx1 = inset_x, width - inset_x

    flat_mean = mean_linear_luminance(flat, width, jx0, jy0, jx1, jy1)
    crt_mean = mean_linear_luminance(crt, width, jx0, jy0, jx1, jy1)
    ratio = crt_mean / flat_mean if flat_mean else 0.0

    # Beating: in linear light, the ratio of the two row profiles smoothed
    # with a Gaussian of 1.5 pitches (the beam legitimately spreads a line's
    # light half a pitch into its neighbours, and a Gaussian that wide
    # passes none of the ripple itself) is divided by its own twelve-pitch
    # trend, which follows the scene: the knee compresses bright rows and
    # the glow lifts dark ones. What remains is anything periodic between
    # the pitch and the trend, which is where a moire beat between the
    # fractional pitch and the pixel grid would show. Rows darker than
    # about 25/255 are left out: there the dither's half code value is a
    # large fraction of the light and the ratio is noise.
    pitch_window = max(1, int(round(pitch)))
    flat_linear = linear_row_profile(flat, width, jy0, jy1, jx0, jx1)
    crt_linear = linear_row_profile(crt, width, jy0, jy1, jx0, jx1)
    flat_smooth = gaussian_smooth(flat_linear, 1.5 * pitch)
    crt_smooth = gaussian_smooth(crt_linear, 1.5 * pitch)
    ratio_rows = [c / f if f > 0.002 else 1.0
                  for c, f in zip(crt_smooth, flat_smooth)]
    trend = gaussian_smooth(ratio_rows, 12.0 * pitch)
    residual = [(r / t - 1.0) if (t > 0.0 and f > 0.01) else 0.0
                for r, t, f in zip(ratio_rows, trend, flat_smooth)]
    trim = len(residual) // 50
    core = residual[trim:len(residual) - trim] or residual
    envelope = max(abs(v) for v in core) if core else 0.0
    # A scene edge leaves one smooth bump in the residual, whose
    # autocorrelation only decays; a beat repeats, and its autocorrelation
    # has a second maximum at the beat period. Report the strongest such
    # maximum between two and forty pitches.
    periodic_lag, periodic_score = secondary_period(
        core, int(2 * pitch), int(40 * pitch))
    sorted_ratios = sorted(ratio_rows)
    median_ratio = sorted_ratios[len(sorted_ratios) // 2] if sorted_ratios else 1.0

    crt_inner = row_profile(crt, width, height, jx0, jx1)[jy0:jy1]
    detrended = [c - s for c, s in
                 zip(crt_inner, moving_average(crt_inner, pitch_window))]
    lag, score = dominant_period(detrended, 2, max(4, int(pitch * 2.5)))

    energy_ok = abs(ratio - 1.0) <= args.energy_tolerance
    # Beating fails only when the residual is both large and periodic.
    envelope_ok = (envelope <= args.envelope_tolerance or
                   periodic_score < 0.5)
    pitch_ok = lag is not None and abs(lag - pitch) <= 0.6
    print(f"viewport rows {y0}..{y1} ({y1 - y0} px), pitch {pitch:.3f} px/line")
    print(f"energy   flat {flat_mean:.4f} crt {crt_mean:.4f} ratio {ratio:.4f} "
          f"{'ok' if energy_ok else 'FAIL'} (tolerance {args.energy_tolerance})")
    print(f"beating  linear row ratio median {median_ratio:.4f}, residual "
          f"{envelope * 100:.2f}% with periodicity {periodic_score:.2f}"
          f"{f' at {periodic_lag} px' if periodic_lag else ''} "
          f"{'ok' if envelope_ok else 'FAIL'} (fails above "
          f"{args.envelope_tolerance * 100:.1f}% when periodicity >= 0.5)")
    print(f"pitch    strongest period {lag} px (score {score:.3f}) "
          f"{'ok' if pitch_ok else 'FAIL'} (expected {pitch:.2f})")

    if args.crop:
        x, y, w, h = (int(v) for v in args.crop.split(","))
        write_crop(args.crop_out, flat, crt, width, x, y, x + w, y + h)
        print(f"crop     {args.crop_out}")
    return 0 if energy_ok and envelope_ok and pitch_ok else 1


if __name__ == "__main__":
    sys.exit(main())
