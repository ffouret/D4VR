"""Read complete, bounded float records emitted through VKD3D's debug ring."""
from __future__ import annotations
import math
from pathlib import Path
import re

MESSAGE = re.compile(r"Shader: ([0-9a-fA-F]{1,16}): Instance (\d+), ID \((\d+), (\d+), (\d+)\):\s*(.*)$")


def read_records(log: Path, row_counts: dict[int, int]) -> dict:
    if log.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("Diagnostic log exceeds 64 MiB")
    groups, rejected, segment = {}, 0, 0
    with log.open(encoding='utf-8', errors='replace') as stream:
        for line in stream:
            if 'Enabling shader debug ring of size:' in line:
                segment += 1
            match = MESSAGE.search(line)
            if not match or int(match[3]) not in row_counts:
                continue
            shader, invocation, marker, sample, row, text = match.groups()
            marker, sample, row = int(marker), int(sample), int(row)
            if int(invocation) != sample or sample >= 1024 or row >= row_counts[marker]:
                rejected += 1
                continue
            try:
                values = [float(v.strip()) for v in text.split(',')]
            except ValueError:
                values = []
            if len(values) != 4 or not all(math.isfinite(v) and abs(v) < 1e30 for v in values):
                rejected += 1
                continue
            rows = groups.setdefault((segment, shader.lower().zfill(16), sample, marker), {})
            if row in rows:
                rows['duplicate'] = True
                rejected += 1
            else:
                rows[row] = values
    records = []
    for key, rows in sorted(groups.items()):
        count = row_counts[key[3]]
        if set(rows) == set(range(count)):
            records.append({'segment': key[0], 'shader_hash': key[1], 'sample_id': key[2],
                            'marker': key[3], 'rows': [rows[i] for i in range(count)]})
    return {'records': records, 'incomplete_or_duplicate_groups': len(groups) - len(records), 'rejected_rows': rejected}
