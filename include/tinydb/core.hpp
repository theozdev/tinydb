#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tinydb
{

inline constexpr std::uint16_t PAGE_SIZE     = 4096;
inline constexpr char          MAGIC[4]      = {'T', 'D', 'B', '1'};
inline constexpr std::uint32_t VERSION       = 1;
inline constexpr std::uint32_t HEADER_USED   = (4 + 2 + 2 + 4);
inline constexpr std::size_t   PAGE_HDR_SIZE = 4;
inline constexpr std::size_t   BODY_SIZE =
    static_cast<std::size_t>(PAGE_SIZE) - PAGE_HDR_SIZE;

static_assert(BODY_SIZE + PAGE_HDR_SIZE == PAGE_SIZE, "size math mismatch");

struct Header
{
    char                                              magic[4];
    std::uint16_t                                     version;
    std::uint16_t                                     page_size;
    std::uint32_t                                     page_count;
    std::array<std::uint8_t, PAGE_SIZE - HEADER_USED> reserved{};
};

struct Slot
{
    std::uint16_t off;
    std::uint16_t len;
};

inline constexpr std::size_t SLOT_SIZE = sizeof(Slot);

struct DataPage
{

    uint16_t                            payload_end;
    uint16_t                            slot_count;
    std::array<std::uint8_t, BODY_SIZE> body{};
};

static_assert(sizeof(DataPage) == PAGE_SIZE,
              "DataPage must be exactly one page");
static_assert(sizeof(Header) == PAGE_SIZE, "Header must be exactly one page");

inline std::size_t dir_start(const DataPage &p)
{
    return BODY_SIZE - static_cast<std::size_t>(p.slot_count) * SLOT_SIZE;
}

inline std::size_t free_space(const DataPage &p)
{
    const std::size_t dir = dir_start(p);
    return (p.payload_end <= dir) ? (dir - p.payload_end) : 0u;
}

inline std::size_t slot_offset(const DataPage &p, std::uint16_t i)
{
    return dir_start(p) + static_cast<std::size_t>(i) * SLOT_SIZE;
}

inline Slot slot_get(const DataPage &p, std::uint16_t i)
{
    assert(i < p.slot_count);
    Slot s;
    std::memcpy(&s, p.body.data() + slot_offset(p, i), SLOT_SIZE);
    return s;
}

inline void slot_set(DataPage &p, std::uint16_t i, const Slot &s)
{
    assert(i < p.slot_count);
    std::memcpy(p.body.data() + slot_offset(p, i), &s, SLOT_SIZE);
}

inline void header_set_defaults(Header &h)
{
    std::memset(h.reserved.data(), 0, h.reserved.size());

    std::memcpy(h.magic, MAGIC, 4);
    h.version    = static_cast<decltype(h.version)>(VERSION);
    h.page_size  = static_cast<decltype(h.page_size)>(PAGE_SIZE);
    h.page_count = static_cast<decltype(h.page_count)>(1);
}

inline bool write_page(std::FILE *f, std::uint32_t page_n, const void *buf)
{
    const std::uint64_t off = static_cast<std::uint64_t>(page_n) * PAGE_SIZE;
    if (std::fseek(f, static_cast<long>(off), SEEK_SET) != 0)
    {
        std::perror("fseek(page)");
        return false;
    }
    const std::size_t n = std::fwrite(buf, 1, PAGE_SIZE, f);
    if(n != PAGE_SIZE)
    {
        std::perror("fwrite(page)");
        return false;
    }
    return true;
}

inline bool read_page(std::FILE *f, std::uint32_t page_n, void *buf)
{

    const std::uint64_t off = static_cast<std::uint64_t>(page_n) * PAGE_SIZE;
    if (std::fseek(f, static_cast<long>(off), SEEK_SET) != 0)
    {
        return false;
    }
    const std::size_t n = std::fread(buf, 1, PAGE_SIZE, f);
    return n == PAGE_SIZE;
}

inline bool db_sync_header(std::FILE *f, const Header &h)
{
    if (!write_page(f, 0, &h))
    {
        std::perror("write header (page 0)");
        return false;
    }
    if (std::fflush(f) != 0)
    {
        std::perror("fflush header");
        return false;
    }
    return true;
}

inline bool db_open(const char *path, std::FILE **out_f, Header *out_hdr)
{
    if (!path || !out_f || !out_hdr)
        return false;

    // try open existing
    if (std::FILE *f = std::fopen(path, "r+b"))
    {
        Header h{};
        if (!read_page(f, 0, &h))
        {
            std::fclose(f);
            return false;
        }

        const bool magic_ok =
            (std::memcmp(h.magic, MAGIC, 4) == 0);
        const bool version_ok =
            h.version == static_cast<decltype(h.version)>(VERSION);
        const bool psize_ok =
            h.page_size == static_cast<decltype(h.page_size)>(PAGE_SIZE);
        const bool count_ok =
            h.page_count >= static_cast<decltype(h.page_count)>(1);

        if (!magic_ok || !version_ok || !psize_ok || !count_ok)
        {
            std::fprintf(stderr, "invalid header\n");
            std::fclose(f);
            return false;
        }
        *out_f   = f;
        *out_hdr = h;
        return true;
    }
    else
    {
        std::perror("fopen r+b");
    }

    // create new
    if (std::FILE *f = std::fopen(path, "w+b"))
    {
        Header h{};
        header_set_defaults(h);
        if (!db_sync_header(f, h))
        {
            std::fprintf(stderr, "failed to sync header\n");
            std::fclose(f);
            return false;
        }
        *out_f   = f;
        *out_hdr = h;
        return true;
    }
    else
    {
        std::perror("fopen w+b");
        return false;
    }
}

inline bool db_close(std::FILE *f)
{
    if (!f)
    {
        return false;
    }
    int rc = std::fclose(f);
    f      = nullptr;
    return rc == 0;
}

inline bool db_append_page(std::FILE *f, Header *h, std::uint32_t *out_page_n)
{
    if (!f || !h)
    {
        return false;
    }

    const std::uint32_t new_id = h->page_count;

    DataPage pg{};
    pg.payload_end = 0;
    pg.slot_count  = 0;

    if (!write_page(f, new_id, &pg))
    {
        return false;
    }

    h->page_count = static_cast<decltype(h->page_count)>(h->page_count + 1);
    if (!db_sync_header(f, *h))
    {
        return false;
    }

    if (out_page_n)
    {
        *out_page_n = new_id;
    }

    return true;
}

inline bool ensure_page(std::FILE *f, Header *hdr, std::uint32_t page_n)
{
    if (!f || !hdr)
    {
        return false;
    }

    if (page_n == 0)
    {
        return true;
    }
    while (hdr->page_count <= page_n)
    {
        if (!db_append_page(f, hdr, nullptr))
        {
            return false;
        }
    }

    return true;
}

} // namespace tinydb
