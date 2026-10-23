#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    PAGE_SIZE     = 4096,
    VERSION       = 1,
    HEADER_USED   = 12,
    PAGE_HDR_SIZE = 4,
    BODY_SIZE     = PAGE_SIZE - PAGE_HDR_SIZE
};

static const char MAGIC[4] = {'T', 'D', 'B', '1'};

typedef struct
{
    char     magic[4];
    uint16_t version;
    uint16_t page_size;
    uint32_t page_count;
    uint8_t  reserved[PAGE_SIZE - HEADER_USED];
} Header;

typedef struct
{
    uint16_t off;
    uint16_t len;
} Slot;

enum
{
    SLOT_SIZE = sizeof(Slot)
};

typedef struct
{
    uint16_t payload_end;
    uint16_t slot_count;
    uint8_t  body[BODY_SIZE];
} DataPage;

/* Keep the existing native-endian file layout exactly as it was. */
_Static_assert(sizeof(Header) == PAGE_SIZE, "Header must fill one page");
_Static_assert(offsetof(Header, version) == 4, "Header version offset");
_Static_assert(offsetof(Header, page_size) == 6, "Header page_size offset");
_Static_assert(offsetof(Header, page_count) == 8, "Header page_count offset");
_Static_assert(offsetof(Header, reserved) == HEADER_USED, "Header layout");
_Static_assert(sizeof(Slot) == 4, "Slot must be four bytes");
_Static_assert(offsetof(Slot, len) == 2, "Slot length offset");
_Static_assert(sizeof(DataPage) == PAGE_SIZE, "DataPage must fill one page");
_Static_assert(offsetof(DataPage, slot_count) == 2, "Page slot_count offset");
_Static_assert(offsetof(DataPage, body) == PAGE_HDR_SIZE, "Page body offset");

size_t dir_start(const DataPage *p)
{
    return BODY_SIZE - (size_t)p->slot_count * SLOT_SIZE;
}

size_t free_space(const DataPage *p)
{
    if (p->slot_count > BODY_SIZE / SLOT_SIZE)
    {
        return 0;
    }

    const size_t dir = dir_start(p);
    return (p->payload_end <= dir) ? dir - p->payload_end : 0;
}

size_t slot_offset(const DataPage *p, uint16_t i)
{
    return dir_start(p) + (size_t)i * SLOT_SIZE;
}

Slot slot_get(const DataPage *p, uint16_t i)
{
    assert(i < p->slot_count);
    Slot s;
    memcpy(&s, p->body + slot_offset(p, i), SLOT_SIZE);
    return s;
}

void slot_set(DataPage *p, uint16_t i, const Slot *s)
{
    assert(i < p->slot_count);
    memcpy(p->body + slot_offset(p, i), s, SLOT_SIZE);
}

void header_set_defaults(Header *h)
{
    memset(h, 0, sizeof(*h));
    memcpy(h->magic, MAGIC, sizeof(MAGIC));
    h->version    = VERSION;
    h->page_size  = PAGE_SIZE;
    h->page_count = 1;
}

bool write_page(FILE *f, uint32_t page_n, const void *buf)
{
    const uint64_t off = (uint64_t)page_n * PAGE_SIZE;
    if (off > (uint64_t)LONG_MAX || fseek(f, (long)off, SEEK_SET) != 0)
    {
        return false;
    }
    return fwrite(buf, 1, PAGE_SIZE, f) == PAGE_SIZE;
}

bool read_page(FILE *f, uint32_t page_n, void *buf)
{
    const uint64_t off = (uint64_t)page_n * PAGE_SIZE;
    if (off > (uint64_t)LONG_MAX || fseek(f, (long)off, SEEK_SET) != 0)
    {
        return false;
    }
    return fread(buf, 1, PAGE_SIZE, f) == PAGE_SIZE;
}

bool db_sync_header(FILE *f, const Header *h)
{
    return write_page(f, 0, h) && fflush(f) == 0;
}

bool db_open(const char *path, FILE **out_f, Header *out_hdr)
{
    if (!path || !out_f || !out_hdr)
    {
        return false;
    }

    *out_f = NULL;

    FILE  *f = fopen(path, "r+b");
    Header h = {0};

    if (!f)
    {
        if (errno != ENOENT)
        {
            perror("fopen r+b");
            return false;
        }

        /* C11 exclusive creation prevents truncating an existing file. */
        f = fopen(path, "w+bx");
        if (!f)
        {
            perror("fopen w+bx");
            return false;
        }

        header_set_defaults(&h);
        if (!db_sync_header(f, &h))
        {
            fprintf(stderr, "failed to sync header\n");
            fclose(f);
            return false;
        }
    }
    else
    {
        if (!read_page(f, 0, &h))
        {
            fprintf(stderr, "failed to read header\n");
            fclose(f);
            return false;
        }

        if (memcmp(h.magic, MAGIC, sizeof(MAGIC)) != 0 ||
            h.version != VERSION || h.page_size != PAGE_SIZE ||
            h.page_count < 1)
        {
            fprintf(stderr, "invalid header\n");
            fclose(f);
            return false;
        }
    }

    *out_f   = f;
    *out_hdr = h;
    return true;
}

bool db_close(FILE *f)
{
    return f && fclose(f) == 0;
}

bool db_append_page(FILE *f, Header *h, uint32_t *out_page_n)
{
    if (!f || !h || h->page_count == UINT32_MAX)
    {
        return false;
    }

    const uint32_t new_id = h->page_count;
    DataPage       pg     = {0};

    if (!write_page(f, new_id, &pg))
    {
        return false;
    }

    h->page_count++;
    if (!db_sync_header(f, h))
    {
        return false;
    }

    if (out_page_n)
    {
        *out_page_n = new_id;
    }
    return true;
}

bool ensure_page(FILE *f, Header *hdr, uint32_t page_n)
{
    if (!f || !hdr)
    {
        return false;
    }

    while (hdr->page_count <= page_n)
    {
        if (!db_append_page(f, hdr, NULL))
        {
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    const char *db_path = (argc > 1) ? argv[1] : "data/tiny.db";
    FILE       *f       = NULL;
    Header      hdr     = {0};

    if (!db_open(db_path, &f, &hdr))
    {
        fprintf(stderr, "db_open failed for %s\n", db_path);
        return 1;
    }

    if (!ensure_page(f, &hdr, 1))
    {
        fprintf(stderr, "ensure_page failed\n");
        db_close(f);
        return 1;
    }

    printf("Opened DB: pages=%" PRIu32 " (before)\n", hdr.page_count);

    DataPage buf = {0};
    if (!read_page(f, 1, &buf))
    {
        fprintf(stderr, "read_page failed\n");
        db_close(f);
        return 1;
    }

    printf("before: payload_end=%" PRIu16 " slot_count=%" PRIu16 "\n",
           buf.payload_end, buf.slot_count);

    const char    *msg   = "hello";
    const uint16_t len   = (uint16_t)strlen(msg);
    const size_t   space = free_space(&buf);

    if (space < SLOT_SIZE + len)
    {
        fprintf(stderr, "no space in page 1\n");
        db_close(f);
        return 1;
    }

    const uint16_t rec_off = buf.payload_end;
    memcpy(buf.body + rec_off, msg, len);
    buf.payload_end = (uint16_t)(buf.payload_end + len);

    /* Moving dir_start must preserve the indexes of existing slots. */
    const size_t old_dir = dir_start(&buf);
    memmove(buf.body + old_dir - SLOT_SIZE, buf.body + old_dir,
            (size_t)buf.slot_count * SLOT_SIZE);
    buf.slot_count++;

    const Slot s = {rec_off, len};
    slot_set(&buf, (uint16_t)(buf.slot_count - 1), &s);

    if (!write_page(f, 1, &buf) || fflush(f) != 0)
    {
        fprintf(stderr, "write_page/fflush failed\n");
        db_close(f);
        return 1;
    }

    printf("after: payload_end=%" PRIu16 " slot_count=%" PRIu16 "\n",
           buf.payload_end, buf.slot_count);
    printf("Opened DB: pages=%" PRIu32 " (after)\n", hdr.page_count);

    if (!db_close(f))
    {
        perror("db_close");
        return 1;
    }
    return 0;
}
