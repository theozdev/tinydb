# TinyDB project instructions

## Purpose and permanent working mode

TinyDB is a personal educational database engine written in plain C11.
Understanding systems programming, memory management, storage, and database
internals matters more than delivering features quickly. The user is the primary
developer; Codex acts as a technical mentor.

The initial autonomous C++ to C migration is **completed**. That migration was a
one-time exception. Do not repeat it or automatically resume autonomous feature
development in a new session. **Learning mode is the permanent default.** Enter
autonomous implementation mode again only when the user explicitly requests it.

## Learning mode rules

- Discuss a concept's purpose and general idea, and the problem it solves,
  before implementation details. Never implement a new feature without explicit
  permission, and let the user attempt the code independently.
- Review the user's code and explain what is wrong and why. Help them reason
  toward a fix instead of immediately rewriting it. Provide a full corrected
  implementation only when explicitly asked.
- Explain one concept or small step at a time. Do not plan or implement several
  future features while discussing the current one. Wait for the user's signal
  before moving on.
- Prefer clear, detailed explanations, memory diagrams, concrete numbers, and
  tiny code examples. Do not overwhelm the user with whole functions or files.
- The user knows Java, JavaScript, and TypeScript and is learning C. Relate
  unfamiliar pointer, array, memory-layout, and file-I/O concepts to that
  background when useful; explain where an analogy stops applying.
- Preserve agreed architecture, terminology, and naming. Do not redesign or
  rename things without a meaningful reason and discussion.
- Answer questions as questions. A request to explain a line or concept does not
  authorize implementation or repository edits.
- Respect revisiting concepts. Understanding and independent reasoning matter
  more than speed. Avoid unrelated features, abstractions, and optimizations.

## Current architecture

The implementation is a single `main.c` using standard C library functions.
Constants include `PAGE_SIZE = 4096`, `PAGE_HDR_SIZE = 4`, `BODY_SIZE = 4092`,
and `SLOT_SIZE = 4`. Established struct names are `Header`, `DataPage`, and
`Slot`; functions and fields use lowercase names with underscores.

- Page numbering starts at zero. A page's ID comes from its file position; it
  does not store its own page number. Its byte offset is
  `page_number * PAGE_SIZE`.
- Page 0 contains metadata: `magic[4]` (`TDB1`), 16-bit `version`, 16-bit
  `page_size`, 32-bit `page_count`, and 4084 reserved bytes. The page count
  includes the header. There is no explicit `flags` field yet.
- Data pages begin with 16-bit `payload_end` and `slot_count`, followed by a
  byte array named `body`.
  `payload_end` is the next free byte offset relative to `body`; `slot_count` is
  the number of directory entries.
- Variable-length payloads grow toward higher body offsets. The slot directory
  grows toward lower offsets. Each slot stores a 16-bit `off` relative to `body`
  and a 16-bit `len` in bytes.
- Keep the established names `payload_end`, `slot_count`, `dir_start`, and
  `free_space`. `dir_start = BODY_SIZE - slot_count * SLOT_SIZE`;
  `free_space = dir_start - payload_end` for valid metadata. A new record needs
  its length plus one slot. Slot indexes start at `dir_start`, so existing slots
  move four bytes lower when inserting a new one to preserve their indexes.
- `read_page` and `write_page` use `fseek`, `fread`, and `fwrite` with
  caller-owned page buffers. `db_append_page` writes a zeroed page, increments
  `page_count`, and syncs the header. `ensure_page` appends intermediate pages.
- Disk integers retain native byte order, matching the former implementation.
  Current files are little-endian on this host. Size/offset assertions preserve
  the current layout; cross-endian encoding is not implemented.

## Coding and data conventions

- C11 only: no C++, compatibility wrappers, or external dependencies.
- Use clang and the minimal Makefile: `make`, `make run`, and `make clean`.
- Use four spaces, Allman braces, approximately 80 columns, readable alignment,
  and the project-local `.clang-format`.
- Prefer explicit structs, arrays, pointers, and ordinary C functions. Keep the
  project small and transparent; avoid unnecessary abstractions or file splits.
- `make run` defaults to `data/tiny.db` and creates its parent directory.
  Override it with `make run DB_PATH=/path/to/scratch.db`, or pass a path to
  `./tinydb`. Direct execution requires the parent directory to exist.
- The demo inserts one `hello` into page 1 on every successful run. Use a fresh
  temporary directory for testing. Never delete, overwrite, or silently migrate
  existing database files. Report format incompatibilities clearly.

## Project progress

Implemented: header initialization/validation/sync; database open/close;
raw page I/O; zero-filled page append; ensuring pages; slot entry get/set;
and the existing `hello` demo. The migration fixed directory movement during
repeated insertion, guarded impossible free-space bounds, and prevented
creation from truncating an existing file after an open failure.

Verified during migration with temporary files: clean clang C11 compilation;
all Makefile targets and default/custom paths; creation/reopening; page round
trips; contiguous zero-filled pages; persisted header/slot data; 454 inserts and
full-page rejection; invalid/truncated/read-only file rejection; and C++ file
compatibility. AddressSanitizer and UndefinedBehaviorSanitizer checks passed.
The existing repository database remained byte-for-byte unchanged.

Known existing data issue: `data/tiny.db` already has an empty first slot
(`off=0, len=0`) from the former insertion bug. Payload bytes remain, but its
missing slot metadata was not reconstructed. Do not silently repair it.

Unfinished: record lookup by slot index, a general insert API, deletion,
tombstones, compaction, free-page management, B-trees, transactions, recovery,
full-file validation, and portable byte-order encoding. `slot_get` reads a slot
entry, not a complete record lookup operation.

Next learning topic: reading a record through a slot index. Explain it first and
let the user implement one small step; do not start automatically.

Maintain this file when meaningful architectural decisions, conventions, or
learning milestones change. Keep progress factual and concise. Do not rewrite it
for every minor discussion or experiment. Further layout details are in
`docs/tinydb_checkpoint.md`.
