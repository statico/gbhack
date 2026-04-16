#!/usr/bin/env python3
"""
Convert Kenney micro-roguelike monochrome 8x8 PNG tiles to GB 2bpp hex data.
Pure stdlib — no PIL needed. Handles indexed-color PNGs (type 3).
"""

import struct
import zlib
import sys
import os


def read_png_grayscale(filepath):
    """Read a PNG and return (width, height, [grayscale_values])."""
    with open(filepath, 'rb') as f:
        data = f.read()

    assert data[:8] == b'\x89PNG\r\n\x1a\n', "Not a PNG"

    pos = 8
    ihdr = None
    palette = None
    trns = None
    idat_data = b''

    while pos < len(data):
        length = struct.unpack('>I', data[pos:pos+4])[0]
        ctype = data[pos+4:pos+8]
        cdata = data[pos+8:pos+8+length]
        pos += 12 + length

        if ctype == b'IHDR':
            w, h = struct.unpack('>II', cdata[:8])
            bit_depth = cdata[8]
            color_type = cdata[9]
            ihdr = (w, h, bit_depth, color_type)
        elif ctype == b'PLTE':
            palette = [(cdata[i], cdata[i+1], cdata[i+2]) for i in range(0, len(cdata), 3)]
        elif ctype == b'tRNS':
            trns = list(cdata)
        elif ctype == b'IDAT':
            idat_data += cdata
        elif ctype == b'IEND':
            break

    w, h, bit_depth, color_type = ihdr
    raw = zlib.decompress(idat_data)

    # Reconstruct scanlines with filter
    if color_type == 3:  # Indexed
        if bit_depth == 8:
            stride = w
        elif bit_depth == 4:
            stride = (w + 1) // 2
        elif bit_depth == 2:
            stride = (w + 3) // 4
        elif bit_depth == 1:
            stride = (w + 7) // 8
        else:
            raise ValueError(f"Unsupported bit depth {bit_depth}")

        scanlines = []
        idx = 0
        prev_row = [0] * stride
        for y in range(h):
            filt = raw[idx]
            idx += 1
            row = list(raw[idx:idx+stride])
            idx += stride

            # Apply PNG reconstruction filters
            if filt == 0:  # None
                pass
            elif filt == 1:  # Sub
                for i in range(1, len(row)):
                    row[i] = (row[i] + row[i-1]) & 0xFF
            elif filt == 2:  # Up
                for i in range(len(row)):
                    row[i] = (row[i] + prev_row[i]) & 0xFF
            elif filt == 3:  # Average
                for i in range(len(row)):
                    a = row[i-1] if i > 0 else 0
                    b = prev_row[i]
                    row[i] = (row[i] + (a + b) // 2) & 0xFF
            elif filt == 4:  # Paeth
                for i in range(len(row)):
                    a = row[i-1] if i > 0 else 0
                    b = prev_row[i]
                    c = prev_row[i-1] if i > 0 else 0
                    p = a + b - c
                    pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
                    if pa <= pb and pa <= pc:
                        pr = a
                    elif pb <= pc:
                        pr = b
                    else:
                        pr = c
                    row[i] = (row[i] + pr) & 0xFF

            scanlines.append(row)
            prev_row = row

        # Extract pixel indices
        pixels = []
        for y in range(h):
            row = scanlines[y]
            for x in range(w):
                if bit_depth == 8:
                    pi = row[x]
                elif bit_depth == 4:
                    byte_val = row[x // 2]
                    if x % 2 == 0:
                        pi = (byte_val >> 4) & 0x0F
                    else:
                        pi = byte_val & 0x0F
                elif bit_depth == 2:
                    byte_val = row[x // 4]
                    shift = 6 - (x % 4) * 2
                    pi = (byte_val >> shift) & 0x03
                elif bit_depth == 1:
                    byte_val = row[x // 8]
                    shift = 7 - (x % 8)
                    pi = (byte_val >> shift) & 0x01
                pixels.append(pi)

        # Convert palette indices to grayscale
        gray = []
        for pi in pixels:
            if pi < len(palette):
                r, g, b = palette[pi]
                # Check transparency
                is_transparent = (trns is not None and pi < len(trns) and trns[pi] == 0)
                if is_transparent:
                    gray.append(0)  # transparent = black/unlit
                else:
                    gray.append((r + g + b) // 3)
            else:
                gray.append(0)

        return w, h, gray

    elif color_type == 0:  # Grayscale
        stride = w if bit_depth == 8 else (w + 7) // 8
        scanlines = []
        idx = 0
        prev_row = [0] * stride
        for y in range(h):
            filt = raw[idx]
            idx += 1
            row = list(raw[idx:idx+stride])
            idx += stride
            if filt == 1:
                for i in range(1, len(row)):
                    row[i] = (row[i] + row[i-1]) & 0xFF
            elif filt == 2:
                for i in range(len(row)):
                    row[i] = (row[i] + prev_row[i]) & 0xFF
            scanlines.append(row)
            prev_row = row

        gray = []
        for y in range(h):
            for x in range(w):
                if bit_depth == 8:
                    gray.append(scanlines[y][x])
                elif bit_depth == 1:
                    byte_val = scanlines[y][x // 8]
                    shift = 7 - (x % 8)
                    bit = (byte_val >> shift) & 1
                    gray.append(255 if bit else 0)
        return w, h, gray

    elif color_type == 2:  # RGB
        stride = w * 3
        scanlines = []
        idx = 0
        prev_row = [0] * stride
        for y in range(h):
            filt = raw[idx]
            idx += 1
            row = list(raw[idx:idx+stride])
            idx += stride
            if filt == 1:
                for i in range(3, len(row)):
                    row[i] = (row[i] + row[i-3]) & 0xFF
            elif filt == 2:
                for i in range(len(row)):
                    row[i] = (row[i] + prev_row[i]) & 0xFF
            scanlines.append(row)
            prev_row = row

        gray = []
        for y in range(h):
            for x in range(w):
                r = scanlines[y][x*3]
                g = scanlines[y][x*3+1]
                b = scanlines[y][x*3+2]
                gray.append((r + g + b) // 3)
        return w, h, gray

    else:
        raise ValueError(f"Unsupported color type: {color_type}")


def tile_to_gb_hex(gray, w, h, threshold=128):
    """Convert 8x8 grayscale to GB 2bpp. White pixels = lit (color 3)."""
    assert w == 8 and h == 8
    rows = []
    for y in range(8):
        byte_val = 0
        for x in range(8):
            if gray[y * 8 + x] >= threshold:
                byte_val |= (1 << (7 - x))
        rows.append((byte_val, byte_val))
    return rows


def format_tile_c(rows, name=""):
    parts = []
    if name:
        parts.append(f"    // {name}")
    hex_pairs = []
    for lo, hi in rows:
        hex_pairs.append(f"0x{lo:02X},0x{hi:02X}")
    # Print in rows of 4 pairs for compactness
    for i in range(0, len(hex_pairs), 4):
        parts.append("    " + ", ".join(hex_pairs[i:i+4]) + ",")
    return "\n".join(parts)


def print_tile_visual(gray, w=8, h=8, threshold=128):
    """Print a visual representation of the tile."""
    for y in range(h):
        row = ""
        for x in range(w):
            row += "#" if gray[y * w + x] >= threshold else "."
        print(f"    // {row}")


def main():
    tile_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            '..', '..', 'art', 'kenney_micro-roguelike', 'Tiles', 'Monochrome')

    if not os.path.isdir(tile_dir):
        print(f"Tile directory not found: {tile_dir}", file=sys.stderr)
        sys.exit(1)

    print("// Kenney micro-roguelike monochrome tiles -> GB 2bpp hex")
    print("// CC0 license - credit: Kenney (www.kenney.nl)")
    print()

    for i in range(160):
        filepath = os.path.join(tile_dir, f"tile_{i:04d}.png")
        if not os.path.exists(filepath):
            continue

        try:
            w, h, gray = read_png_grayscale(filepath)
            rows = tile_to_gb_hex(gray, w, h)
            print(format_tile_c(rows, f"Kenney tile {i}"))
            print_tile_visual(gray, w, h)
            print()
        except Exception as e:
            print(f"    // Tile {i}: ERROR - {e}")
            print()


if __name__ == '__main__':
    main()
