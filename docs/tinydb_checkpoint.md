# tinydb — C++ (C-style) checkpoint

## Ground rules
- No user-defined classes, no exceptions, no RTTI, no iostreams.
- C-style APIs + plain `struct`s; compile as C++ for nicer types.
- Use only: `constexpr`, `static_assert`, `enum class`, `std::array`, `std::string_view`, `std::span`.
- All disk integers are **little-endian**. All I/O is page-aligned and page-sized.

---

## Constants & format (v1)
- `PAGE_SIZE = 4096`
- `MAGIC = "TDB1"` (4 bytes)
- `VERSION = 1`
- **Page 0** is the **header** page.
- `page_count` **includes** the header (min value: 1 ⇒ header only).

### Header (page 0) layout (4096 bytes total)
| Field        | Type    | Size       | Notes                           |
|--------------|---------|------------|---------------------------------|
| magic        | char[4] | 4          | `"TDB1"`                        |
| version      | u32     | 4          | `1`                             |
| page_size    | u32     | 4          | must equal `4096`               |
| page_count   | u32     | 4          | total pages incl. header        |
| reserved     | bytes   | 4096 - 16  | zero-filled (future use)        |

**Invariants on open**
- `magic == "TDB1"`
- `version == 1`
- `page_size == 4096`
- `page_count >= 1`

---

## Modules (incremental)

### 1) io (file handle + raw I/O)
**Goal:** thin wrapper around OS calls, still C-style.

Functions:
- `Status io_open_rw(const char* path, /*out*/ int* fd, bool create)`
- `void   io_close(int fd)`
- `Status io_pread_all(int fd, void* buf, size_t n, uint64_t off)`
- `Status io_pwrite_all(int fd, const void* buf, size_t n, uint64_t off)`
- `Status io_truncate(int fd, uint64_t size)`
- `Status io_fsync(int fd)` *(later; stub ok now)*

---

### 2) pager (page math + header cache)
**Goal:** page-numbered access, append pages, maintain header cache.

Types:
- `struct TDB { int fd; Header header; /*maybe path*/ };`

Functions:
- `Status tdb_open(const char* path, /*out*/ TDB* db)`  
  - Create if missing: write fresh header (count=1).  
  - If exists: read+validate header.
- `void   tdb_close(TDB* db)`
- `Status tdb_read_page(TDB* db, uint32_t page_n, void* out_4096)`
- `Status tdb_write_page(TDB* db, uint32_t page_n, const void* in_4096)`
- `Status tdb_append_page(TDB* db, /*out*/ uint32_t* new_page_n)`  
  - Zero-fill new page, write it, update `header.page_count`, persist header.
- `Status tdb_sync(TDB* db)` *(optional for now)*

**Bounds/Checks**
- `page_n < header.page_count` for read/write.
- Offset: `off = uint64_t(page_n) * PAGE_SIZE`.
- New pages are zeroed; header rewritten after successful page write.

---

### 3) freelist (simple page allocator)
**Goal:** reuse freed pages instead of always appending.

Plan (v0): a single linked list of free pages; head stored in header reserved area (we’ll later pick exact offsets).

Functions:
- `Status tdb_alloc_page(TDB* db, /*out*/ uint32_t* page_n)`
- `Status tdb_free_page(TDB* db, uint32_t page_n)`

---

### 4) btree (node format & operations)
Start with one table (key→value store).  

**v0 simplification:** fixed-size keys (e.g., u64) and fixed-size values (e.g., u64) to avoid varlen complexity.

Node page header (conceptual): node type, count, left/right sibling (optional), array of slots.

Functions (sequence we’ll build):
- parse node from page buffer
- search in node (binary search)
- insert into node (no split)
- split node, update links
- root split handling

---

### 5) transactions (later)
Choose **shadow paging** first (simpler mental model):
- Write modified pages to new locations.
- Atomically update a root pointer (stored in header) at commit.
- Crash either leaves old root or new root (no partial).  

WAL can be a later evolution.

---

## Status & errors (shared)
- Codes: `ok, io_error, invalid, corruption, nomem`.
- A `Status` is tiny: `{ code, message(optional) }`.
- Hot paths should avoid formatting messages; tests/tools can check `.code`.

---

## Tests (per lesson)

### Lesson 1 — pager
- **Create & validate header**
  - New file → `page_count == 1`, magic/version/size ok.
- **Append + roundtrip**
  - Append page → write known bytes → read back → compare.
- **Reopen**
  - Close → open → header persists; read written page.

### Lesson 2 — freelist
- **Alloc/free/alloc**
  - Append 3 pages; free one; alloc again returns freed page id.
- **Bounds**
  - Free invalid page id → `invalid` or `corruption`.

### Lesson 3/4 — btree
- **Insert/search (no split)**
  - Insert few pairs; search finds all.
- **Split**
  - Force split; both sides contain appropriate keys; parent/root updated.

---

## Non-goals (for now)
- Variable-length records/overflow pages.
- Concurrency (single-writer, single-process assumed).
- Checksums/compression/encryption.
- Fancy cache; we’ll read/write directly until correctness is proven.

---

## Build & style
- C++20 (or 17), compile with:
  - `-Wall -Wextra -Wconversion -Werror`
  - `-fno-exceptions -fno-rtti`
- Prefer `_t`-free names; use lowercase with underscores for C vibe.
- Keep headers minimal; no templates authored by us.

---

## Open questions
- Do we store a **freelist head** in header’s reserved area (and at which byte offset)?  
  - Suggestion: reserve first 16 bytes in `reserved` for future “superblock” fields:
    - `u32 freelist_head_page` (0 == none)
    - `u32 root_page` (for btree/shadow paging)
    - rest still zero.
- Fixed key/value sizes for v0? (recommend `u64 key`, `u64 value` to start)
- Endianness helpers: add tiny encode/decode or rely on native + assert LE for now?

---

## Next step
Start **Lesson 1**: define `Header` fields & `DB` struct (names only), then implement `tdb_open` behavior step-by-step (create vs open + validation), followed by `tdb_append_page`, `tdb_read_page`, `tdb_write_page`. Tiny pieces, explained as we go.

