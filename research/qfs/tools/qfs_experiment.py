# This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
# optimizes the DBPF loading.
#
# Copyright (C) 2026 Casper Van Gheluwe
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation, under
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <https://www.gnu.org/licenses/>.

# /// script
# requires-python = ">=3.10"
# dependencies = ["tqdm>=4.66"]
# ///

"""Measure QFS/RefPack entry decoding in a real SC4 Plugins directory.

The decoder follows the SC4/RefPack control-byte layout documented by the
SC4 community.  It intentionally uses byte-at-a-time overlapping copies, the
safe implementation strategy also suitable for a native C implementation.
"""
from __future__ import annotations

import argparse
import csv
import struct
import statistics
import time
from collections import Counter, defaultdict
from pathlib import Path

from tqdm import tqdm

TYPE_NAMES = {
    0x6534284A: "Exemplar/Cohort",
    0x7AB50E44: "FSH",
    0x5AD0E817: "S3D",
    0xBD: "SC4Lot?",
    0xE86B1EEF: "DIR",
}


def qfs_decode(data: bytes) -> bytes:
    """Decode one DBPF QFS payload, including its 9-byte QFS header."""
    if len(data) < 9:
        raise ValueError("short QFS payload")
    p = 0
    # DBPF's QFS header is compressed-size (LE), 10 FB, decompressed-size (BE).
    compressed_size = struct.unpack_from("<I", data, p)[0]
    p += 4
    if data[p + 1] != 0xFB or data[p] not in (0x10, 0x50):
        raise ValueError("not QFS")
    flags = data[p]
    p += 2
    # Native SC4 cRZFastCompression3::decoderef uses bit 0 of the QFS
    # control/header byte to select the extended five-byte size form.
    if flags & 1:
        out_size = int.from_bytes(data[p:p + 4], "big")
        p += 4
    else:
        out_size = int.from_bytes(data[p:p + 3], "big")
        p += 3
    out = bytearray()
    limit = min(len(data), 4 + compressed_size) if compressed_size else len(data)
    while p < limit and len(out) < out_size:
        code = data[p]
        if code <= 0x7F:
            if p + 1 >= limit:
                raise ValueError("truncated short control")
            b1 = data[p + 1]
            literals = code & 3
            length = ((code & 0x1C) >> 2) + 3
            distance = ((code & 0x60) << 3) + b1 + 1
            p += 2
        elif code <= 0xBF:
            if p + 2 >= limit:
                raise ValueError("truncated medium control")
            b1, b2 = data[p + 1], data[p + 2]
            literals = (b1 >> 6) & 3
            length = (code & 0x3F) + 4
            distance = ((b1 & 0x3F) << 8) + b2 + 1
            p += 3
        elif code <= 0xDF:
            if p + 3 >= limit:
                raise ValueError("truncated long control")
            b1, b2, b3 = data[p + 1], data[p + 2], data[p + 3]
            literals = code & 3
            length = ((code & 0x0C) << 6) + b3 + 5
            distance = ((code & 0x10) << 12) + (b1 << 8) + b2 + 1
            p += 4
        elif code <= 0xFB:
            literals = (code & 0x1F) * 4 + 4
            length = 0
            distance = 0
            p += 1
        else:
            literals = code & 3
            length = 0
            distance = 0
            p += 1
            out.extend(data[p:p + literals])
            break
        out.extend(data[p:p + literals])
        p += literals
        if length:
            if distance > len(out):
                raise ValueError("invalid QFS distance")
            source = len(out) - distance
            for _ in range(length):
                out.append(out[source])
                source += 1
    if len(out) != out_size:
        raise ValueError(f"size mismatch: got {len(out)}, expected {out_size}")
    return bytes(out)


def entries(path: Path):
    with path.open("rb") as f:
        header = f.read(0x60)
        if header[:4] != b"DBPF":
            return
        count, index_offset, index_size = struct.unpack_from("<III", header, 0x24)
        if count == 0 or index_size < count * 20:
            return
        f.seek(index_offset)
        index = f.read(index_size)
        stride = index_size // count
        # SC4 uses 20-byte records; tolerate a padded index by taking its first 20.
        for i in range(count):
            record = index[i * stride:i * stride + 20]
            if len(record) < 20:
                continue
            type_id, group_id, instance_id, offset, size = struct.unpack("<IIIII", record)
            f.seek(offset)
            payload = f.read(size)
            yield type_id, group_id, instance_id, payload


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("plugins", type=Path, nargs="+")
    ap.add_argument("--csv", type=Path)
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--top-level", action="store_true", help="only scan .dat files directly in plugins")
    ap.add_argument("--sample-every", type=int, default=1,
                    help="decode only every Nth QFS entry while still inventorying the archive")
    args = ap.parse_args()
    rows = []
    errors = Counter()
    qfs_seen = 0
    csv_handle = args.csv.open("w", newline="", encoding="utf-8") if args.csv else None
    csv_writer = None
    if all(p.is_file() for p in args.plugins):
        files = sorted(args.plugins)
    else:
        roots = [p for p in args.plugins if p.is_dir()]
        files = sorted({f for root in roots for f in (root.glob("*.dat") if args.top_level else root.rglob("*.dat"))})
    file_progress = tqdm(files, desc="DBPF archives", unit="file")
    for path in file_progress:
        try:
            for type_id, group_id, instance_id, payload in entries(path):
                if len(payload) < 6 or not (payload[4] in (0x10, 0x50) and payload[5] == 0xFB):
                    continue
                qfs_seen += 1
                file_progress.set_postfix(qfs=qfs_seen, sampled=len(rows), errors=sum(errors.values()))
                if qfs_seen % max(1, args.sample_every) != 0:
                    continue
                # Warm once, then time repeated native-shaped decode loops.
                try:
                    decoded = qfs_decode(payload)
                except Exception as exc:  # keep inventory useful if a variant appears
                    errors[type_id] += 1
                    continue
                times = []
                for _ in range(max(1, args.repeats)):
                    start = time.perf_counter_ns()
                    qfs_decode(payload)
                    times.append((time.perf_counter_ns() - start) / 1e3)
                rows.append({
                    "file": str(path), "type": f"0x{type_id:08X}",
                    "type_name": TYPE_NAMES.get(type_id, "other"),
                    "compressed": len(payload), "uncompressed": len(decoded),
                    "ratio": len(decoded) / len(payload),
                    "decode_us": statistics.median(times),
                })
                if csv_handle is not None:
                    if csv_writer is None:
                        csv_writer = csv.DictWriter(csv_handle, fieldnames=rows[-1].keys())
                        csv_writer.writeheader()
                    csv_writer.writerow(rows[-1])
                    csv_handle.flush()
        except OSError as exc:
            errors[f"file:{exc}"] += 1
    if csv_handle is not None:
        csv_handle.close()
    by_type = defaultdict(list)
    for row in rows:
        by_type[row["type_name"]].append(row)
    print(f"files={len(files)} qfs_entries={len(rows)} errors={sum(errors.values())}")
    for name, group in sorted(by_type.items(), key=lambda kv: -len(kv[1])):
        us = sorted(r["decode_us"] for r in group)
        sizes = [r["uncompressed"] for r in group]
        def pct(p): return us[min(len(us)-1, int(len(us) * p))]
        print(f"{name:18} n={len(group):6} size_med={statistics.median(sizes):9.0f}B "
              f"decode_us med={statistics.median(us):8.2f} p90={pct(.90):8.2f} "
              f"us/KB={statistics.median(r['decode_us']*1024/r['uncompressed'] for r in group):.3f}")
    if errors:
        print("errors_by_type:", errors.most_common(10))


if __name__ == "__main__":
    main()
