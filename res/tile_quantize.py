#!/usr/bin/env python3
"""
Tile-aware color quantization for Game Boy Color.

Reads a 160x144 raw RGB file, enforces:
  - Max 4 colors per 8x8 tile
  - Max 8 palettes of 4 colors each
  - Snaps colors to GBC 5-bit-per-channel (RGB555)

Outputs a new PNG (using only stdlib: struct + zlib).
"""

import struct
import zlib
import sys
import os
from collections import Counter

W, H = 160, 144
TILE_W, TILE_H = 8, 8
TILES_X = W // TILE_W   # 20
TILES_Y = H // TILE_H   # 18
MAX_PAL_COLORS = 4
MAX_PALETTES = 8

def snap_to_gbc(r, g, b):
    """Snap RGB888 to GBC RGB555 (5 bits per channel, mapped back to 8-bit)."""
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return (r5 * 255 // 31, g5 * 255 // 31, b5 * 255 // 31)

def color_distance_sq(c1, c2):
    return (c1[0]-c2[0])**2 + (c1[1]-c2[1])**2 + (c1[2]-c2[2])**2

def find_nearest(color, palette):
    best = 0
    best_dist = color_distance_sq(color, palette[0])
    for i in range(1, len(palette)):
        d = color_distance_sq(color, palette[i])
        if d < best_dist:
            best_dist = d
            best = i
    return best

def median_cut(colors_with_counts, max_colors):
    """Simple median cut to reduce a set of (color, count) to max_colors."""
    if len(colors_with_counts) <= max_colors:
        return [c for c, _ in colors_with_counts]

    buckets = [colors_with_counts]

    while len(buckets) < max_colors:
        # Find bucket with most colors to split
        best_idx = 0
        best_range = -1
        for i, bucket in enumerate(buckets):
            if len(bucket) <= 1:
                continue
            for ch in range(3):
                vals = [c[ch] for c, _ in bucket]
                r = max(vals) - min(vals)
                if r > best_range:
                    best_range = r
                    best_idx = i
                    best_ch = ch

        if best_range <= 0:
            break

        bucket = buckets.pop(best_idx)
        bucket.sort(key=lambda x: x[0][best_ch])
        mid = len(bucket) // 2
        buckets.append(bucket[:mid])
        buckets.append(bucket[mid:])

    # Average each bucket weighted by count
    result = []
    for bucket in buckets:
        if not bucket:
            continue
        total_w = sum(cnt for _, cnt in bucket)
        if total_w == 0:
            continue
        avg = tuple(
            int(sum(c[ch] * cnt for c, cnt in bucket) / total_w)
            for ch in range(3)
        )
        result.append(avg)

    return result

def palette_distance(pal1_colors, pal2_colors):
    """Distance between two palettes = sum of min-distances."""
    total = 0
    for c in pal1_colors:
        mind = min(color_distance_sq(c, p) for p in pal2_colors)
        total += mind
    for c in pal2_colors:
        mind = min(color_distance_sq(c, p) for p in pal1_colors)
        total += mind
    return total

def write_png(filename, pixels, w, h):
    """Write an RGB PNG using only struct and zlib."""
    def chunk(chunk_type, data):
        c = chunk_type + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)

    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)  # 8-bit RGB
    raw = b''
    for y in range(h):
        raw += b'\x00'  # filter: none
        for x in range(w):
            r, g, b = pixels[y * w + x]
            raw += struct.pack('BBB', r, g, b)

    with open(filename, 'wb') as f:
        f.write(sig)
        f.write(chunk(b'IHDR', ihdr))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))

def main():
    input_path = os.path.join(os.path.dirname(__file__), 'title_bg_raw.rgb')
    output_path = os.path.join(os.path.dirname(__file__), 'title_bg.png')

    with open(input_path, 'rb') as f:
        raw = f.read()

    assert len(raw) == W * H * 3, f"Expected {W*H*3} bytes, got {len(raw)}"

    # Parse pixels and snap to GBC color space
    pixels = []
    for i in range(W * H):
        r, g, b = raw[i*3], raw[i*3+1], raw[i*3+2]
        pixels.append(snap_to_gbc(r, g, b))

    # Step 1: Extract unique colors per tile and find best 4-color palette for each
    tile_palettes = []  # list of palette (list of up to 4 colors) per tile
    tile_pixels = []    # [tile_idx] -> list of (local_x, local_y, pixel_idx)

    for ty in range(TILES_Y):
        for tx in range(TILES_X):
            tile_colors = Counter()
            tile_pix = []
            for ly in range(TILE_H):
                for lx in range(TILE_W):
                    px = (ty * TILE_H + ly) * W + (tx * TILE_W + lx)
                    c = pixels[px]
                    tile_colors[c] += 1
                    tile_pix.append(px)

            tile_pixels.append(tile_pix)

            # Reduce to 4 colors via median cut
            colors_with_counts = list(tile_colors.items())
            pal = median_cut(colors_with_counts, MAX_PAL_COLORS)
            tile_palettes.append(pal)

    # Step 2: Merge tile palettes into MAX_PALETTES global palettes
    # Start by collecting all unique tile palettes, then greedily merge closest pairs
    # Each "group" is a set of tile indices sharing a palette
    groups = []
    for i in range(len(tile_palettes)):
        groups.append({
            'tiles': [i],
            'colors': Counter()
        })
        for px_idx in tile_pixels[i]:
            groups[-1]['colors'][pixels[px_idx]] += 1

    # Greedy merge: repeatedly merge the two closest groups until we have MAX_PALETTES
    while len(groups) > MAX_PALETTES:
        best_dist = float('inf')
        best_i, best_j = 0, 1

        # For speed, compute palette representatives for each group
        group_pals = []
        for g in groups:
            pal = median_cut(list(g['colors'].items()), MAX_PAL_COLORS)
            group_pals.append(pal)

        for i in range(len(groups)):
            for j in range(i + 1, len(groups)):
                # Check if merged group would need > 4 colors
                merged_colors = groups[i]['colors'] + groups[j]['colors']
                unique_in_merged = len(merged_colors)

                d = palette_distance(group_pals[i], group_pals[j])

                # Penalize merges that would lose more color info
                if unique_in_merged > MAX_PAL_COLORS:
                    d += unique_in_merged * 100

                if d < best_dist:
                    best_dist = d
                    best_i, best_j = i, j

        # Merge best_j into best_i
        groups[best_i]['tiles'].extend(groups[best_j]['tiles'])
        groups[best_i]['colors'] += groups[best_j]['colors']
        groups.pop(best_j)

        remaining = len(groups)
        if remaining % 50 == 0 or remaining <= 20:
            print(f"  Merging... {remaining} groups remaining")

    # Step 3: Compute final 4-color palette for each group
    final_palettes = []
    tile_to_palette = [0] * (TILES_X * TILES_Y)

    for pal_idx, g in enumerate(groups):
        pal = median_cut(list(g['colors'].items()), MAX_PAL_COLORS)
        # Pad to exactly 4 colors if needed
        while len(pal) < MAX_PAL_COLORS:
            pal.append(pal[-1] if pal else (0, 0, 0))
        final_palettes.append(pal)
        for tile_idx in g['tiles']:
            tile_to_palette[tile_idx] = pal_idx

    print(f"Final palettes: {len(final_palettes)}")
    for i, pal in enumerate(final_palettes):
        print(f"  Palette {i}: {pal}")

    # Step 4: Remap all pixels to their tile's assigned palette
    output_pixels = list(pixels)  # copy
    for tile_idx in range(TILES_X * TILES_Y):
        pal = final_palettes[tile_to_palette[tile_idx]]
        for px_idx in tile_pixels[tile_idx]:
            c = pixels[px_idx]
            nearest = find_nearest(c, pal)
            output_pixels[px_idx] = pal[nearest]

    # Step 5: Verify constraints
    for ty in range(TILES_Y):
        for tx in range(TILES_X):
            tile_colors = set()
            for ly in range(TILE_H):
                for lx in range(TILE_W):
                    px = (ty * TILE_H + ly) * W + (tx * TILE_W + lx)
                    tile_colors.add(output_pixels[px])
            assert len(tile_colors) <= 4, f"Tile ({tx},{ty}) has {len(tile_colors)} colors!"

    all_colors = set(output_pixels)
    print(f"Total unique colors in output: {len(all_colors)}")

    # Step 6: Write output PNG
    write_png(output_path, output_pixels, W, H)
    print(f"Written to {output_path}")

if __name__ == '__main__':
    main()
