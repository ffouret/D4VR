# Exact backing metadata control

The owned Linux CPU control establishes five local relationships between an already open, pinned regular file and the exact VMA returned by its original `mmap` call. It also records the filesystem type. It does **not** establish the optional `map_files` target-metadata witness: all five target `stat` calls returned `EPERM`.

The result preserves the difference between the device fields reported in `maps` and by `fstat`. It admits no device-number translation rule and no new gate policy. The preceding OFF2 evidence remains unchanged.

## Contract

[`exact_backing_control.cpp`](../native/tests/exact_backing_control.cpp) accepts exactly five privately compiled role/path/hash/stat pins: fixture, D3D12, D3D12 core, DXGI and observer. It accepts no runtime file-path arguments. Ordinary path components are opened without following symlinks; the nonblocking leaf FD must identify the expected regular file. SHA256 is calculated from ordinary buffers filled through that held FD, using the existing hash helper.

For each occurrence, the collector creates one page at offset zero with `prot=PROT_NONE` and `flags=MAP_PRIVATE`. It never dereferences the mapping or changes its protection. Before and after snapshots must contain a unique VMA with the exact returned span, offset, permissions, path and inode. A merged or different VMA does not qualify.

The additional backing-metadata facet requires the exact `/proc/<own-pid>/map_files/<start>-<end>` link, matching before/after link text, and successful **target** metadata equal to the still-held FD. Following that one procfs magic link is a specific exception. Metadata of the symlink itself, a pathname-only match or a fallback `stat` of an ordinary path cannot substitute for it. Permission denial leaves this facet unknown.

[`exact_backing_validate.py`](../tools/exact_backing_validate.py) audits supplied snapshots with `audit_bytes(..., expected_pins=..., run_id=...)` or `analyze(...)`. The model performs no mapping, process or file I/O and does not attest the truth or completeness of its inputs. Reports retain their values after later input mutation and FD or address reuse.

Each observation has entry and return ordinals. These delimit collector operations: a hash or maps operation can perform several bounded reads. Guard-generated errors must not all be described as errors returned by individual kernel syscalls. Cleanup results remain separate; failed unmapping inhibits later controlled mappings, and exception cleanup cannot manufacture a successful receipt.

## Bounds

| Item | Bound |
| --- | --- |
| Fixed roles | 5 |
| Simultaneous explicitly created file mappings | 1 |
| Mapping | One actual system page, offset 0 |
| Ordinary file hash bytes | 64 MiB per operation |
| Each maps snapshot | 1 MiB / 8,192 lines |
| Link target | 4,096 bytes, with a 4,097-byte readlink buffer |
| Snapshot/output | 24 MiB |
| Operation envelopes | 14 per role / 140 entry-return transitions |
| Mapped payload bytes read or written | 0 |

The current producer restricts files to at least one page and conservatively refuses an exact hash-byte cap when it has no budget left to establish EOF. These are collection restrictions, not general Linux API requirements. The mapping limit concerns this control's explicit mappings; the process also has its ordinary loader and runtime VMAs.

## Recorded result

The parent ran one owned CPU collection. It returned code 0 after **0.10164681299647782 host seconds**, producing 54,448 bytes of JSON. Its process start cookie was stable. The kernel reported `7.2.9-ogc3.1.fc44.x86_64`; the page size was 4,096 bytes.

| Observation | Result |
| --- | --- |
| Exact local FD → original mmap → VMA relationships | 5 known |
| Operation transitions | 140 |
| `maps` device field | `0:36` in all five occurrences |
| FD `fstat` device field | `0:60` in all five occurrences |
| Filesystem type from each held FD | `0x9123683e` — Btrfs |
| Before/after exact link targets | 10 successful observations |
| `map_files` target metadata | 5 failures: `EPERM` / errno 1 |
| Recorded unmaps / closes | 5 / 5 successful |

The selected VMA rows were identical before and after each occurrence. The five pinned files, collector sources and execution artifacts retained their raw hashes. The before/after hash operations read 23,710,448 ordinary file bytes through held FDs and read no mapped payloads. No Wine, Vulkan, GPU, renderer, game or headset work formed part of this collection.

The measured filesystem type and unequal device fields are recorded facts. This control does not establish that Btrfs caused the earlier mismatch, nor provide a universal mapping between the two device domains. Successful link text also does not replace the denied target metadata.

## Validation and limits

The local CPU preflight passed **681 Python tests**, including the **22 exact-backing model cases**, and **19 native self-test assertions**. The independent synthetic review repeated the 22 cases, refused 15 private mutations and accepted seven positive controls; these repeated checks are not additional Python unit tests. The checks cover distinct same-content files, role/range swaps, unavailable target metadata, failed or pending operations, cleanup, bounds and immutable receipts. Remote CI results are tracked separately; the local counts do not certify a future CI run.

A separate literal parent audit rejoined the five occurrences and 140 transitions without calling the model or its maps parser. The [public proof](proofs/exact-backing-metadata-v1.json) records its hash alongside the raw collection, model audit and final private authority. It publishes hashes and aggregate facts. Exact paths, PID/cookie values, addresses, inodes, FSID values and maps payloads remain private.

The optional backing facet remains unknown. No remote backing identity, loaded-memory attestation, translated Unix-device ownership, PE-to-Unix handle conversion, global query absence or sampled GPU resource is established. The remote gate route remains unadmitted. This later self-control does not retrospectively repair OFF2 or any other exited process.
