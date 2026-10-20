#!/usr/bin/env python3
"""Make a test pack for the HD 2D override out of a dump the port itself wrote.

This invents no artwork. It takes the files `vf3run --tile-dump` (or `vf3 --tile-dump`) wrote, enlarges them
by the scale with no smoothing, and swaps their red and blue channels. The result is unmistakable on screen --
anything the pack replaced comes out with its colours exchanged, and anything it did not is untouched -- which
is what makes it a proof that the override fired, and not a drawing exercise.

The file name is the key and is never changed: that is the whole convention (docs/fv2-hd-2d.md).

  py -3 tools/hd-pack-test.py <dump folder> <pack folder> --scale 4
  py -3 tools/hd-pack-test.py <dump> <pack> --scale 4 --layers L2_835ee1da3ab98e02
  py -3 tools/hd-pack-test.py <dump> <pack> --scale 4 --tiles-only

Needs Pillow and numpy.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image


def convert(src, dst, scale):
    im = Image.open(src).convert("RGBA")
    a = np.asarray(im)
    # red and blue exchanged, alpha left alone (alpha 0 means "nothing drawn here" and must stay so)
    swapped = a[:, :, [2, 1, 0, 3]]
    big = np.repeat(np.repeat(swapped, scale, axis=0), scale, axis=1)
    Image.fromarray(big, "RGBA").save(dst)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("dump", help="the folder --tile-dump wrote (it holds tiles/ and layers/)")
    p.add_argument("pack", help="where to write the pack (the same shape: tiles/ and layers/)")
    p.add_argument("--scale", type=int, default=4, help="the tile scale the pack is for (must match --tile-scale)")
    p.add_argument("--layers", nargs="*", default=None,
                   help="only these layer images, by name without .png (default: all of them)")
    p.add_argument("--tiles-only", action="store_true", help="no layer images at all")
    p.add_argument("--layers-only", action="store_true", help="no tile images at all")
    args = p.parse_args()

    if args.scale < 1 or args.scale > 8:
        sys.exit("the scale is 1 to 8")

    n_t = n_l = 0
    if not args.layers_only:
        os.makedirs(os.path.join(args.pack, "tiles"), exist_ok=True)
        d = os.path.join(args.dump, "tiles")
        for name in sorted(os.listdir(d)) if os.path.isdir(d) else []:
            if not name.lower().endswith(".png"):
                continue
            convert(os.path.join(d, name), os.path.join(args.pack, "tiles", name), args.scale)
            n_t += 1
    if not args.tiles_only:
        os.makedirs(os.path.join(args.pack, "layers"), exist_ok=True)
        d = os.path.join(args.dump, "layers")
        want = set(args.layers) if args.layers else None
        for name in sorted(os.listdir(d)) if os.path.isdir(d) else []:
            if not name.lower().endswith(".png"):
                continue
            if want is not None and name[:-4] not in want:
                continue
            convert(os.path.join(d, name), os.path.join(args.pack, "layers", name), args.scale)
            n_l += 1
    print("%d tiles and %d layers written to %s at scale %d" % (n_t, n_l, args.pack, args.scale))


if __name__ == "__main__":
    main()
