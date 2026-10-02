#!/usr/bin/env python3
"""Create a BPS patch (no Flips needed): python make_bps.py clean.gba new.gba out.bps

Commands used: SourceRead (bytes unchanged at the same offset), SourceCopy (data that moved; found through an index
of 32-byte source blocks), TargetCopy (runs of a repeated byte, e.g. 0xFF padding) and TargetRead (new bytes).
Check the result with bpscheck.py."""
import sys
import zlib

BLOCK = 32


def vlq(n):
    out = bytearray()
    while True:
        x = n & 0x7F
        n >>= 7
        if n == 0:
            out.append(0x80 | x)
            return out
        out.append(x)
        n -= 1


def signed(n):
    return vlq((abs(n) << 1) | (1 if n < 0 else 0))


def main(src_path, tgt_path, out_path):
    src = open(src_path, 'rb').read()
    tgt = open(tgt_path, 'rb').read()
    ls, lt = len(src), len(tgt)

    index = {}
    for i in range(0, ls - BLOCK + 1, BLOCK):
        key = src[i:i + BLOCK]
        if key.count(key[0]) == BLOCK:          # skip uniform filler blocks
            continue
        index.setdefault(key, i)

    out = bytearray(b'BPS1')
    out += vlq(ls) + vlq(lt) + vlq(0)
    literal_start = None
    src_rel = 0                                 # SourceCopy cursor
    tgt_rel = 0                                 # TargetCopy cursor

    def flush(t):
        nonlocal literal_start
        if literal_start is not None and t > literal_start:
            out.extend(vlq(((t - literal_start - 1) << 2) | 1))
            out.extend(tgt[literal_start:t])
        literal_start = None

    t = 0
    while t < lt:
        # 1. unchanged at the same offset
        if t < ls and src[t] == tgt[t]:
            e = t
            while e + 64 <= min(ls, lt) and src[e:e + 64] == tgt[e:e + 64]:
                e += 64
            while e < min(ls, lt) and src[e] == tgt[e]:
                e += 1
            if e - t >= 8:
                flush(t)
                out.extend(vlq(((e - t - 1) << 2) | 0))
                t = e
                continue
        # 2. a run of one repeated byte
        if t > 0 and tgt[t] == tgt[t - 1]:
            e = t
            b = tgt[t - 1]
            chunk = bytes([b]) * 64
            while e + 64 <= lt and tgt[e:e + 64] == chunk:
                e += 64
            while e < lt and tgt[e] == b:
                e += 1
            if e - t >= 16:
                flush(t)
                out.extend(vlq(((e - t - 1) << 2) | 3))
                out.extend(signed((t - 1) - tgt_rel))
                tgt_rel = t - 1 + (e - t)
                t = e
                continue
        # 3. data that moved: some source block matches here
        s = index.get(tgt[t:t + BLOCK]) if t + BLOCK <= lt else None
        if s is not None:
            e, se = t + BLOCK, s + BLOCK
            while e + 64 <= lt and se + 64 <= ls and src[se:se + 64] == tgt[e:e + 64]:
                e += 64
                se += 64
            while e < lt and se < ls and src[se] == tgt[e]:
                e += 1
                se += 1
            flush(t)
            out.extend(vlq(((e - t - 1) << 2) | 2))
            out.extend(signed(s - src_rel))
            src_rel = se
            t = e
            continue
        # 4. new byte
        if literal_start is None:
            literal_start = t
        t += 1
    flush(lt)

    out += zlib.crc32(src).to_bytes(4, 'little')
    out += zlib.crc32(tgt).to_bytes(4, 'little')
    out += zlib.crc32(out).to_bytes(4, 'little')
    open(out_path, 'wb').write(out)
    print('wrote', out_path, len(out), 'bytes')


if __name__ == '__main__':
    main(*sys.argv[1:4])
