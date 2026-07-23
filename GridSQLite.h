/**
 * @file GridSQLite.h
 * @brief Grid++ 內部使用的唯讀 SQLite 素材讀取器。
 *
 * 支援 UTF-8、非 WAL 的 table B-tree 與必要的 record 型別。
 * 只提供資料表走訪，不提供 SQL 或寫入功能。
 */
#ifndef GRID_SQLITE_H
#define GRID_SQLITE_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gridpp_detail {

// 解碼後的 record 欄位
struct Column {
    int kind = 2;  // 0 整數，1 文字或 BLOB，2 NULL，3 浮點數
    int64_t i = 0;
    std::vector<unsigned char> bytes;
};

class SQLiteReader {
public:
    // data 的生命週期必須長於 reader。
    explicit SQLiteReader(const std::vector<unsigned char>& data)
        : data(data) {
        static const unsigned char magic[] = "SQLite format 3";
        if (data.size() < 100 || std::memcmp(data.data(), magic, 16) != 0)
            fail("不是有效的 SQLite 3 檔案");

        pageSize = readBEU16(16);
        if (pageSize == 1) pageSize = 65536;
        if (pageSize < 512 || pageSize > 65536 ||
            (pageSize & (pageSize - 1)) != 0)
            fail("page size 無效");
        if (data.size() < pageSize || data.size() % pageSize != 0)
            fail("檔案大小不是完整頁面");

        const size_t reserved = data[20];
        if (reserved >= pageSize || pageSize - reserved < 480)
            fail("usable page size 無效");
        usable = pageSize - reserved;

        if (data[21] != 64 || data[22] != 32 || data[23] != 32)
            fail("payload fraction 無效");
        const uint32_t schemaFormat = readBEU32(44);
        if (schemaFormat < 1 || schemaFormat > 4)
            fail("schema format 無效");
        if (readBEU32(56) != 1)
            fail("只支援 UTF-8 SQLite 資料庫");
        if (data[18] == 2 || data[19] == 2)
            fail("不支援 WAL 格式的 SQLite 資料庫");
        if (data[18] != 1 || data[19] != 1)
            fail("SQLite file format version 無效");

        const uint32_t declaredPages = readBEU32(28);
        if (declaredPages != 0 && readBEU32(24) == readBEU32(92) &&
            declaredPages != pageCount())
            fail("檔頭頁數與檔案大小不一致");
    }

    SQLiteReader(std::vector<unsigned char>&&) = delete;

    // 從 root page 走訪 table B-tree。
    void walkTable(
        uint32_t rootPage,
        const std::function<void(int64_t, const std::vector<Column>&)>& callback
    ) const {
        std::vector<bool> visited(pageCount() + 1, false);
        walkTablePage(rootPage, callback, visited, 0);
    }

private:
    static void fail(const std::string& message) {
        throw std::runtime_error("Grid++ SQLite 錯誤：" + message);
    }

    void requireRange(size_t offset, size_t length, size_t end,
                      const char* what) const {
        if (offset > end || length > end - offset)
            fail(std::string(what) + "超出檔案範圍");
    }

    size_t pageCount() const {
        return data.size() / pageSize;
    }

    size_t pageOffset(uint32_t pageNumber) const {
        if (pageNumber == 0 || pageNumber > pageCount())
            fail("頁碼超出檔案範圍");
        return static_cast<size_t>(pageNumber - 1) * pageSize;
    }

    // SQLite 多位元組整數使用 big-endian。
    uint16_t readBEU16(size_t offset) const {
        requireRange(offset, 2, data.size(), "16-bit 整數");
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(data[offset]) << 8) |
            static_cast<uint16_t>(data[offset + 1]));
    }

    uint32_t readBEU32(size_t offset) const {
        requireRange(offset, 4, data.size(), "32-bit 整數");
        return (static_cast<uint32_t>(data[offset]) << 24) |
               (static_cast<uint32_t>(data[offset + 1]) << 16) |
               (static_cast<uint32_t>(data[offset + 2]) << 8) |
                static_cast<uint32_t>(data[offset + 3]);
    }

    // SQLite varint 最長 9 bytes，最後一個 byte 使用全部 8 bits。
    uint64_t readVarint(size_t& offset, size_t end) const {
        uint64_t value = 0;
        for (int i = 0; i < 9; ++i) {
            requireRange(offset, 1, end, "varint");
            const unsigned char byte = data[offset++];
            if (i == 8) return (value << 8) | byte;
            value = (value << 7) | (byte & 0x7f);
            if ((byte & 0x80) == 0) return value;
        }
        fail("varint 無效");
        return 0;
    }

    static int64_t unsignedBitsToI64(uint64_t value) {
        if (value <= static_cast<uint64_t>(INT64_MAX))
            return static_cast<int64_t>(value);
        return -1 - static_cast<int64_t>(~value);
    }

    // 將 1 到 8 bytes 的有號整數解碼為 int64_t。
    static int64_t readBEI64(const std::vector<unsigned char>& bytes,
                             size_t offset, size_t length) {
        if (length < 1 || length > 8 || offset > bytes.size() ||
            length > bytes.size() - offset)
            fail("有號整數超出 record 範圍");

        uint64_t value = 0;
        for (size_t i = 0; i < length; ++i)
            value = (value << 8) | bytes[offset + i];
        if (length < 8 && (bytes[offset] & 0x80))
            value |= ~uint64_t{0} << (length * 8);
        return unsignedBitsToI64(value);
    }

    // 從 record buffer 讀取 varint。
    static uint64_t readRecordVarint(const std::vector<unsigned char>& record,
                                     size_t& offset, size_t end) {
        uint64_t value = 0;
        for (int i = 0; i < 9; ++i) {
            if (offset >= end || offset >= record.size())
                fail("record varint 超出範圍");
            const unsigned char byte = record[offset++];
            if (i == 8) return (value << 8) | byte;
            value = (value << 7) | (byte & 0x7f);
            if ((byte & 0x80) == 0) return value;
        }
        fail("record varint 無效");
        return 0;
    }

    // 依 serial type 解碼 record。
    static std::vector<Column> decodeRecord(
        const std::vector<unsigned char>& record
    ) {
        size_t headerOffset = 0;
        const uint64_t headerSize64 =
            readRecordVarint(record, headerOffset, record.size());
        if (headerSize64 < headerOffset || headerSize64 > record.size())
            fail("record header size 無效");
        const size_t headerSize = static_cast<size_t>(headerSize64);
        size_t contentOffset = headerSize;
        std::vector<Column> columns;

        while (headerOffset < headerSize) {
            const uint64_t serial =
                readRecordVarint(record, headerOffset, headerSize);
            Column column;
            size_t length = 0;

            if (serial == 0) {
                column.kind = 2;
            } else if (serial >= 1 && serial <= 6) {
                length = serial <= 4 ? static_cast<size_t>(serial)
                                     : (serial == 5 ? 6 : 8);
                column.kind = 0;
                column.i = readBEI64(record, contentOffset, length);
            } else if (serial == 7) {
                length = 8;
                column.kind = 3;
            } else if (serial == 8 || serial == 9) {
                column.kind = 0;
                column.i = serial == 9 ? 1 : 0;
            } else if (serial == 10 || serial == 11) {
                fail("遇到 SQLite 保留的 serial type");
            } else {
                const uint64_t length64 = (serial - 12) / 2;
                if (length64 > record.size() - contentOffset)
                    fail("欄位內容超出 record 範圍");
                length = static_cast<size_t>(length64);
                column.kind = 1;
                column.bytes.assign(record.begin() + contentOffset,
                                    record.begin() + contentOffset + length);
            }

            if (length > record.size() - contentOffset)
                fail("欄位內容超出 record 範圍");
            contentOffset += length;
            columns.push_back(std::move(column));
        }

        if (contentOffset != record.size())
            fail("record 內容長度不一致");
        return columns;
    }

    // 拒絕循環與重複頁面。
    void markPage(uint32_t pageNumber, std::vector<bool>& visited) const {
        if (pageNumber == 0 || pageNumber >= visited.size())
            fail("頁碼超出檔案範圍");
        if (visited[pageNumber])
            fail("偵測到重複或循環頁面");
        visited[pageNumber] = true;
    }

    void walkTablePage(
        uint32_t pageNumber,
        const std::function<void(int64_t, const std::vector<Column>&)>& callback,
        std::vector<bool>& visited,
        size_t depth
    ) const {
        if (depth > 64) fail("B-tree 過深");
        markPage(pageNumber, visited);

        const size_t start = pageOffset(pageNumber);
        const size_t end = start + usable;
        // 第 1 頁包含 100-byte database header。
        const size_t header = start + (pageNumber == 1 ? 100 : 0);
        requireRange(header, 8, end, "B-tree header");

        const unsigned char type = data[header];
        if (type != 0x05 && type != 0x0d)
            fail("不是 table B-tree 頁面");
        const size_t headerSize = type == 0x05 ? 12 : 8;
        requireRange(header, headerSize, end, "B-tree header");

        const uint16_t cellCount = readBEU16(header + 3);
        const size_t cellPointers = header + headerSize;
        requireRange(cellPointers, static_cast<size_t>(cellCount) * 2,
                     end, "cell pointer array");

        // Interior page 先走 cell children，再走 right-most child。
        if (type == 0x05) {
            for (uint16_t i = 0; i < cellCount; ++i) {
                const size_t cell = start + readBEU16(cellPointers + i * 2);
                if (cell < cellPointers + static_cast<size_t>(cellCount) * 2)
                    fail("interior table cell 與 header 重疊");
                requireRange(cell, 4, end, "interior table cell");
                walkTablePage(readBEU32(cell), callback, visited, depth + 1);
            }
            walkTablePage(readBEU32(header + 8), callback, visited, depth + 1);
            return;
        }

        // Leaf page 包含 rowid 與 record payload。
        for (uint16_t i = 0; i < cellCount; ++i) {
            size_t cell = start + readBEU16(cellPointers + i * 2);
            if (cell < cellPointers + static_cast<size_t>(cellCount) * 2)
                fail("leaf table cell 與 header 重疊");
            requireRange(cell, 1, end, "leaf table cell");
            const uint64_t payloadLength64 = readVarint(cell, end);
            const int64_t rowid = unsignedBitsToI64(readVarint(cell, end));
            if (payloadLength64 > data.size())
                fail("payload length 無效");
            const size_t payloadLength =
                static_cast<size_t>(payloadLength64);

            // 依 SQLite 規格計算 leaf page 的 local payload。
            const size_t maxLocal = usable - 35;
            size_t local = payloadLength;
            if (payloadLength > maxLocal) {
                const size_t minLocal = ((usable - 12) * 32 / 255) - 23;
                const size_t candidate =
                    minLocal + (payloadLength - minLocal) % (usable - 4);
                local = candidate <= maxLocal ? candidate : minLocal;
            }

            const size_t overflowPointer = payloadLength > local ? 4 : 0;
            requireRange(cell, local + overflowPointer, end, "leaf payload");
            std::vector<unsigned char> payload(data.begin() + cell,
                                               data.begin() + cell + local);

            uint32_t overflowPage =
                overflowPointer ? readBEU32(cell + local) : 0;
            size_t remaining = payloadLength - local;
            // Overflow page 前 4 bytes 是 next page number。
            while (remaining > 0) {
                if (overflowPage == 0)
                    fail("overflow chain 提早結束");
                markPage(overflowPage, visited);
                const size_t overflowStart = pageOffset(overflowPage);
                requireRange(overflowStart, usable, overflowStart + pageSize,
                             "overflow page");
                const uint32_t next = readBEU32(overflowStart);
                const size_t take = std::min(usable - 4, remaining);
                payload.insert(payload.end(),
                               data.begin() + overflowStart + 4,
                               data.begin() + overflowStart + 4 + take);
                remaining -= take;
                overflowPage = next;
            }
            if (overflowPage != 0)
                fail("overflow chain 長度超過 payload");

            callback(rowid, decodeRecord(payload));
        }
    }

    const std::vector<unsigned char>& data;
    size_t pageSize = 0;
    size_t usable = 0;
};

} // namespace gridpp_detail

#endif // GRID_SQLITE_H
