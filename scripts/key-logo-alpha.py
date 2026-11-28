#!/usr/bin/env python3
"""Key the IDW logo's black padding to transparency so the editor shows no plate.

CMake embeds Resources/idw_logo.png as binary data (juce_add_binary_data
IDWAssets) and the editor draws it in the header strip. The artwork ships on a
solid black background, so an opaque copy would render as a dark rectangle
against the editor. This script turns that padding into transparency with a
luminance ramp, which keeps the symbol's smoke glow fading out cleanly instead
of ending in a hard edge.

Run it on any replacement artwork before committing it:

    python scripts/key-logo-alpha.py path/to/new-artwork.png

Without a path it re-keys Resources/idw_logo.png in place. The input's alpha
channel is ignored, so running it again on an already keyed asset is a no-op:
the RGB channels are untouched by keying, only alpha is written.

CI runs it with --check, which writes nothing and exits non-zero unless the
checked-in logo is exactly what the ramp produces and its four corner alphas
are 0, so an opaque asset replaced without keying fails the build instead of
shipping the dark plate. --check also writes an opaque copy of the logo to a
temporary file and fails if that copy is accepted, so a check that quietly
passes everything cannot go green.

Requires Pillow (python -m pip install pillow).
"""
import tempfile
from argparse import ArgumentParser
from pathlib import Path

from PIL import Image, ImageChops

# max(r, g, b) at or below this is pure padding, at or above it is untouched.
# Between the two the alpha ramp fades out, matching the panel colour exactly.
TRANSPARENT_AT_OR_BELOW = 24
OPAQUE_AT_OR_ABOVE = 60

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LOGO = ROOT / 'Resources' / 'idw_logo.png'


def key_alpha(image, low=TRANSPARENT_AT_OR_BELOW, high=OPAQUE_AT_OR_ABOVE):
    """Return the image as RGBA with alpha ramped from its luminance."""
    if not 0 <= low < high <= 255:
        raise ValueError('expected 0 <= low < high <= 255')
    opaque = image.convert('RGB')  # any existing alpha is discarded, never re-applied
    luminance = ImageChops.lighter(
        ImageChops.lighter(opaque.getchannel('R'), opaque.getchannel('G')),
        opaque.getchannel('B'))
    span = high - low
    ramp = lambda value: min(1.0, max(0.0, (value - low) / span))
    keyed = opaque.copy()
    keyed.putalpha(luminance.point(lambda value: round(ramp(value) * 255)))
    return keyed


def corner_alphas(keyed):
    """Alpha at the four corners of the keyed tile, where its padding sits."""
    alpha = keyed.getchannel('A')
    last_x, last_y = keyed.width - 1, keyed.height - 1
    return [alpha.getpixel(point)
            for point in ((0, 0), (last_x, 0), (0, last_y), (last_x, last_y))]


def keying_problem(source, low=TRANSPARENT_AT_OR_BELOW, high=OPAQUE_AT_OR_ABOVE):
    """Return why source is not keyed the way the ramp produces, or None if it is."""
    with Image.open(source) as artwork:
        artwork.load()
        current = artwork.convert('RGBA')
    keyed = key_alpha(current, low, high)
    if current.size != keyed.size or current.tobytes() != keyed.tobytes():
        return (f'{source} is not keyed the way this ramp produces; '
                f'run: python scripts/key-logo-alpha.py {source}')
    corners = corner_alphas(keyed)
    if any(corners):
        return (f'{source} matches the ramp but its corner alpha is {corners}, not 0; '
                f'the padding is not black, so the editor would still draw a plate')
    return None


def reject_unkeyed_copy(source, low=TRANSPARENT_AT_OR_BELOW, high=OPAQUE_AT_OR_ABOVE):
    """Fail if --check would accept an opaque copy, so a blind check cannot pass.

    The copy is the logo with its alpha dropped, which is exactly the dark plate
    this script exists to remove. It is written to a temporary directory, so
    nothing beside the repository is touched.
    """
    with tempfile.TemporaryDirectory() as scratch:
        unkeyed = Path(scratch) / 'un-keyed.png'
        with Image.open(source) as artwork:
            artwork.load()
            artwork.convert('RGB').save(unkeyed)
        if keying_problem(unkeyed, low, high) is None:
            raise SystemExit(f'self-test failed: --check accepted {unkeyed.name}, '
                             f'an un-keyed copy of {Path(source).name}')


def main():
    parser = ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('source', nargs='?', default=DEFAULT_LOGO,
                        help='opaque artwork on black (default: Resources/idw_logo.png)')
    parser.add_argument('-o', '--output', default=None,
                        help='output PNG (default: overwrite the source)')
    parser.add_argument('--low', type=int, default=TRANSPARENT_AT_OR_BELOW,
                        help=f'luminance at or below which alpha is 0 (default: {TRANSPARENT_AT_OR_BELOW})')
    parser.add_argument('--high', type=int, default=OPAQUE_AT_OR_ABOVE,
                        help=f'luminance at or above which alpha is 255 (default: {OPAQUE_AT_OR_ABOVE})')
    parser.add_argument('--check', action='store_true',
                        help='write nothing; fail unless the file already holds this keying')
    args = parser.parse_args()

    source = Path(args.source)
    output = Path(args.output) if args.output else source

    if args.check:
        problem = keying_problem(source, args.low, args.high)
        if problem:
            raise SystemExit(problem)
        reject_unkeyed_copy(source, args.low, args.high)
        with Image.open(source) as artwork:
            size = f'{artwork.width}x{artwork.height}'
        print(f'{source} is keyed ({size}, alpha 0 at the tile corners); '
              f'an opaque copy of it is rejected')
        return

    with Image.open(source) as artwork:
        artwork.load()
        if artwork.width != artwork.height:
            print(f'warning: {source.name} is {artwork.width}x{artwork.height}, not square')
        keyed = key_alpha(artwork, args.low, args.high)

    alpha = keyed.getchannel('A')
    counts = alpha.histogram()
    transparent, opaque = counts[0], counts[255]
    output.parent.mkdir(parents=True, exist_ok=True)
    keyed.save(output)
    print(f'{source} -> {output} ({keyed.width}x{keyed.height} RGBA, '
          f'{transparent} transparent, {opaque} opaque, '
          f'{keyed.width * keyed.height - transparent - opaque} ramped)')
    if not transparent:
        print('warning: no fully transparent pixels; check the artwork really sits on black')
    # The tile's corners sit in the padding: they must end up fully clear or the
    # editor draws a plate again.
    corners = corner_alphas(keyed)
    if any(corners):
        print(f'warning: corner alpha is {corners}, expected 0; the padding is not black')


if __name__ == '__main__':
    main()
