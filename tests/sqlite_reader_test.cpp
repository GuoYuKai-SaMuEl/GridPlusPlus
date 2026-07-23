#include "GridSQLite.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static std::vector<unsigned char> readFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    assert(file);
    return std::vector<unsigned char>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

static bool isRejected(const std::vector<unsigned char>& data) {
    try {
        gridpp_detail::SQLiteReader reader(data);
        (void)reader;
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

static bool isTableRejected(const std::vector<unsigned char>& data) {
    try {
        gridpp_detail::SQLiteReader reader(data);
        reader.walkTable(
            1,
            [](int64_t, const std::vector<gridpp_detail::Column>&) {});
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

int main(int argc, char** argv) {
    const std::vector<unsigned char> data =
        readFile(argc > 1 ? argv[1] : "examples/pacman/pacman.db");
    gridpp_detail::SQLiteReader database(data);

    uint32_t spritesRoot = 0;
    database.walkTable(
        1,
        [&](int64_t, const std::vector<gridpp_detail::Column>& columns) {
            if (columns.size() < 4 || columns[0].kind != 1 ||
                columns[2].kind != 1 || columns[3].kind != 0)
                return;
            const std::string type(columns[0].bytes.begin(),
                                   columns[0].bytes.end());
            const std::string name(columns[2].bytes.begin(),
                                   columns[2].bytes.end());
            if (type == "table" && name == "sprites")
                spritesRoot = static_cast<uint32_t>(columns[3].i);
        });
    assert(spritesRoot != 0);

    size_t spriteCount = 0;
    database.walkTable(
        spritesRoot,
        [&](int64_t, const std::vector<gridpp_detail::Column>& columns) {
            assert(columns.size() >= 4);
            assert(columns[1].kind == 1);
            assert(columns[3].kind == 1);
            assert(columns[3].bytes.size() == 4096);
            ++spriteCount;
        });
    assert(spriteCount == 9);

    std::vector<unsigned char> badMagic = data;
    badMagic[0] = 0;
    assert(isRejected(badMagic));

    std::vector<unsigned char> truncated = data;
    truncated.pop_back();
    assert(isRejected(truncated));

    std::vector<unsigned char> wal = data;
    wal[18] = wal[19] = 2;
    assert(isRejected(wal));

    std::vector<unsigned char> badCell = data;
    badCell[108] = badCell[109] = 0;
    assert(isTableRejected(badCell));
}
