#include "tinydb/core.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits.h>

using namespace tinydb;

int main(int argc, char **argv)
{
    const char *db_path = (argc > 1) ? argv[1] : "data/tiny.db";
    FILE       *f       = nullptr;
    Header      hdr{};

    if (!db_open(db_path, &f, &hdr))
    {
        std::fprintf(stderr, "db_open failed for %s\n", db_path);
        return 1;
    }

    if (!ensure_page(f, &hdr, 1))
    {
        std::fprintf(stderr, "ensure_page");
        db_close(f);
        return 1;
    }

    std::printf("Opened DB: pages=%u (before)\n", hdr.page_count);

    DataPage buf{};
    if (!read_page(f, 1, &buf))
    {
        std::perror("read_page");
        db_close(f);
        return 1;
    }

    std::printf("before: payload_end=%u slot_count=%u\n", buf.payload_end,
                buf.slot_count);

    const char   *msg = "hello";
    std::uint16_t len = static_cast<std::uint16_t>(std::strlen(msg));

    std::size_t space = free_space(buf);

    if (space < SLOT_SIZE + len)
    {
        std::fprintf(stderr, "no space in page 1\n");
        db_close(f);
        return 1;
    }

    std::uint16_t rec_off = buf.payload_end;
    std::memcpy(buf.body.data() + rec_off, msg, len);
    buf.payload_end = static_cast<std::uint16_t>(buf.payload_end + len);

    buf.slot_count = static_cast<std::uint16_t>(buf.slot_count + 1);
    Slot s{rec_off, len};
    slot_set(buf, static_cast<std::uint16_t>(buf.slot_count - 1), s);

    if (!write_page(f, 1, &buf) || std::fflush(f) != 0)
    {
        std::perror("write_page/fflush");
        db_close(f);
        return 1;
    }
    std::printf("after: payload_end=%u slot_count=%u\n", buf.payload_end,
                buf.slot_count);
    std::printf("Opened DB: pages=%u (after)\n", hdr.page_count);
    db_close(f);

    return 0;
}
