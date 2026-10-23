# TinyDB checkpoint

The C++ to C migration is complete. TinyDB is a personal learning project in
plain C11. The user writes new features; Codex explains concepts and reviews
attempts. See `AGENTS.md` for the permanent learning rules.

## Build and run

```sh
make
make run
make clean
```

`make run` creates the parent directory if necessary and runs the demo against
`data/tiny.db`. Each successful run inserts one more five-byte `hello` record
into page 1. It stops with an error when that page is full.

For experiments, choose a fresh temporary directory so existing data stays safe:

```sh
scratch_dir=$(mktemp -d)
make run DB_PATH="$scratch_dir/tiny.db"
./tinydb "$scratch_dir/tiny.db"
```

The executable also defaults to `data/tiny.db` when no path is supplied. Direct
execution expects the parent directory to exist. `make clean` removes only the
executable, never database files.

## Actual file layout, version 1

The source is `main.c`. Pages are 4096 bytes, numbered from zero. A page's ID is
its position in the file, not a field stored inside it. Its byte offset is
`page_number * PAGE_SIZE`.

Page 0 is a `Header`:

| Byte offset | Field | Size |
| --- | --- | --- |
| 0 | `magic` (`TDB1`) | 4 |
| 4 | `version` (`uint16_t`, value 1) | 2 |
| 6 | `page_size` (`uint16_t`, value 4096) | 2 |
| 8 | `page_count` (`uint32_t`, includes page 0) | 4 |
| 12 | `reserved` (zero-filled on creation) | 4084 |

There is no separate `flags` field in the current implementation. The earlier
checkpoint's 32-bit version and page-size fields were inaccurate. The migration
preserves the actual C++ source layout, including all reserved bytes.

Integers are written in native byte order, as before. This host produces
little-endian files. Cross-endian portability is not implemented. C11 size and
offset assertions guard against struct layout changes; packed structs are not
needed on this platform.

Pages 1 onward are `DataPage` values:

- `payload_end`: a 16-bit offset of the next free payload byte in `body`.
- `slot_count`: a 16-bit count of directory entries.
- `body`: 4092 bytes shared by payloads, free space, and the directory.
- `Slot`: a 16-bit `off` (record offset relative to `body`) and a 16-bit `len`
  (record length in bytes).

```text
DataPage: [payload_end:2][slot_count:2][body:4092]
body:    [payloads --->][free space][slot 0][slot 1]...[slot N-1]
                                  ^ dir_start
```

`dir_start = BODY_SIZE - slot_count * SLOT_SIZE`.
`free_space = dir_start - payload_end` for a valid page. Insertion needs the
record length plus four bytes for its slot. Slots are indexed from `dir_start`;
adding one moves existing slots four bytes toward lower offsets so their indexes
remain stable.

## Implemented and verified

- Header creation, validation, synchronization, and reopening.
- Page-sized `read_page` and `write_page` with caller-owned buffers.
- Zero-filled append and `ensure_page`, including intermediate pages and the
  persisted page count.
- Slot entry access with `slot_get` and `slot_set`.
- The existing demo insertion into page 1; repeated insertion preserves slots.
- Rejection of full pages and impossible page-header bounds before insertion.
- New-file creation only after a missing-file error, using C11 exclusive
  creation to avoid truncating an existing file.

Migration checks used temporary files and
AddressSanitizer/UndefinedBehaviorSanitizer:
creation/reopening, raw page round trips, zero-filled intermediate pages, header
persistence, slot access, 454 inserts up to a full page, and rejection without
modification for full, malformed, truncated, and read-only files. The first C
insertion matched a C++-generated file byte for byte. C++ fixtures and a copy of
the existing database reopened successfully. All Makefile targets and the
default/custom paths were exercised. The repository database's SHA-256 stayed
unchanged.

## Remaining work

The existing repository database already has an empty first slot
(`off=0, len=0`) from the old insertion bug. Its payload bytes are present, but
the migration does not guess missing metadata or repair that file. The corrected
insertion preserves old directory entries as stored and prevents new occurrences
of that bug.

Record lookup by slot index, deletion/tombstones, compaction, free-page
management, indexes/B-trees, transactions, and crash recovery remain
unimplemented. There is no general insert API or interactive query interface.
`fflush` does not provide crash recovery. Full-file consistency validation and
cross-endian encoding are also unfinished.

The next learning topic is reading a record through a slot index. Discuss it
first, then let the user attempt one small step.
