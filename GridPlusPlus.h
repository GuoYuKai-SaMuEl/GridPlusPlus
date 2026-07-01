// =============================================================================
//  Grid++ (GridPlusPlus.h) —— 給初學者的網格遊戲引擎（核心）
//
//  用法：#include "GridPlusPlus.h"
//  完整教學、概念與 API 說明請見專案文件（docs/）。
//
//  依賴：Raylib（需連結）。SQLite 由下方 gridpp_detail 自己手寫讀取器，
//        不需連結 sqlite3。內部原理見 docs/internals/sqlite-reader.md。
// =============================================================================
#ifndef GRIDPLUSPLUS_H
#define GRIDPLUSPLUS_H

#include "raylib.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>   // 編成 WebAssembly 時才需要（瀏覽器主循環）
#endif

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <functional>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <algorithm>

// =============================================================================
//  Part A. 迷你 SQLite 讀取器 (gridpp_detail) —— 引擎內部，學生平常不用看。
//  直接讀 .db 檔，把 sprites 表的每一列(row)讀出來。完整原理（page / b-tree /
//  varint / record / 溢位頁）見 docs/internals/sqlite-reader.md；這裡保留關鍵註解。
// =============================================================================
namespace gridpp_detail {

// 讀取一個 SQLite「變長整數(varint)」：最多 9 個 byte，用最高位元當「還有下一個」的旗標。
// 回傳數值，並把實際用掉幾個 byte 寫回 used。
inline long long readVarint(const unsigned char* p, int& used) {
    long long result = 0;
    used = 0;
    for (int i = 0; i < 9; i++) {
        unsigned char c = p[i];
        used++;
        if (i == 8) {                 // 第 9 個 byte 整個 8 位元都算數值
            result = (result << 8) | c;
            break;
        }
        result = (result << 7) | (c & 0x7F);
        if ((c & 0x80) == 0) break;   // 最高位元是 0 表示結束
    }
    return result;
}

// 讀取一個「大端序(big-endian)」的 4-byte 整數（SQLite 的頁碼就是這種格式）。
inline uint32_t readBE32(const unsigned char* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

// 讀取 n 個 byte 的大端序「有號」整數，並做正負號延伸。
inline long long readBigEndianInt(const unsigned char* p, int n) {
    long long v = 0;
    for (int i = 0; i < n; i++) v = (v << 8) | p[i];
    if (n > 0 && n < 8 && (p[0] & 0x80)) {   // 最高位元是 1 → 是負數，要補 1
        v |= (~0LL) << (n * 8);
    }
    return v;
}

// 一個欄位(column)的值。為了簡單，我們只分成四種：整數、位元組(文字/BLOB)、NULL、浮點數。
struct Column {
    int kind = 2;                    // 0=整數, 1=位元組(文字或BLOB), 2=NULL, 3=浮點數
    long long i = 0;                 // kind==0 時的整數值
    std::vector<unsigned char> bytes;// kind==1 時的位元組內容
};

// 把一筆 record 的位元組解碼成「一排欄位」。
// record 開頭是一段 header，header 裡按順序列出每個欄位的 serial type，
// serial type 同時告訴我們「這個欄位是什麼型別、佔幾個 byte」。
inline std::vector<Column> decodeRecord(const std::vector<unsigned char>& rec) {
    std::vector<Column> cols;
    int used;
    long long headerSize = readVarint(rec.data(), used);
    size_t hpos = used;              // 在 header 區裡讀到哪了
    size_t content = (size_t)headerSize; // 真正的欄位資料從 header 之後開始

    while (hpos < (size_t)headerSize) {
        long long serial = readVarint(rec.data() + hpos, used);
        hpos += used;

        Column c;
        if (serial == 0) {                       // NULL
            c.kind = 2;
        } else if (serial >= 1 && serial <= 6) {  // 1~6：不同長度的整數
            int n = (serial <= 4) ? (int)serial : (serial == 5 ? 6 : 8);
            c.kind = 0;
            c.i = readBigEndianInt(rec.data() + content, n);
            content += n;
        } else if (serial == 7) {                 // 8-byte 浮點數
            c.kind = 3;
            content += 8;
        } else if (serial == 8) {                 // 常數 0（不佔空間）
            c.kind = 0; c.i = 0;
        } else if (serial == 9) {                 // 常數 1（不佔空間）
            c.kind = 0; c.i = 1;
        } else if (serial >= 12) {                // 文字或 BLOB
            int len = (int)((serial - 12) / 2);   // 偶數=BLOB、奇數=文字，長度公式一樣
            c.kind = 1;
            c.bytes.assign(rec.begin() + content, rec.begin() + content + len);
            content += len;
        }
        cols.push_back(std::move(c));
    }
    return cols;
}

// 走訪一張表的 B-tree，把每一列資料交給 cb 回呼函式處理。
//   base/pageSize/usable：整個 .db 的記憶體起點、頁大小、一頁可用大小。
//   pageNum：現在要走訪哪一頁（從 1 開始算）。
inline void walkTable(const unsigned char* base, int pageSize, int usable,
                      uint32_t pageNum,
                      const std::function<void(long long, const std::vector<Column>&)>& cb) {
    const unsigned char* page = base + (size_t)(pageNum - 1) * pageSize;
    int hdr = (pageNum == 1) ? 100 : 0;  // 第 1 頁前面有 100 bytes 的檔案總標頭
    unsigned char type = page[hdr];      // 頁的種類
    int numCells = (page[hdr + 3] << 8) | page[hdr + 4]; // 這一頁有幾個 cell

    if (type == 0x05) {
        // 「內部頁(interior table)」：它只是路標，指向更下層的頁。
        int cellPtr = hdr + 12;
        for (int i = 0; i < numCells; i++) {
            int off = (page[cellPtr + i * 2] << 8) | page[cellPtr + i * 2 + 1];
            uint32_t child = readBE32(page + off);
            walkTable(base, pageSize, usable, child, cb);
        }
        uint32_t right = readBE32(page + hdr + 8); // 最右邊還有一個子頁
        walkTable(base, pageSize, usable, right, cb);

    } else if (type == 0x0D) {
        // 「葉頁(leaf table)」：真正存放資料的地方。
        int cellPtr = hdr + 8;
        for (int i = 0; i < numCells; i++) {
            int off = (page[cellPtr + i * 2] << 8) | page[cellPtr + i * 2 + 1];

            int used;
            long long payloadLen = readVarint(page + off, used); off += used; // 這列總共多大
            long long rowid      = readVarint(page + off, used); off += used; // 這列的 rowid

            // 計算「有多少資料留在本頁、有多少要去溢位頁」。
            // 這幾條公式是 SQLite 官方規定的，照抄即可。
            int X = usable - 35;
            long long local;
            if (payloadLen <= X) {
                local = payloadLen;                 // 整列都塞得下，沒有溢位
            } else {
                long long M = ((usable - 12) * 32 / 255) - 23;
                long long K = M + (payloadLen - M) % (usable - 4);
                local = (K <= X) ? K : M;
            }

            std::vector<unsigned char> payload(page + off, page + off + local);

            // 如果有溢位，順著「溢位頁鏈」一頁一頁把剩下的接回來。
            if (local < payloadLen) {
                uint32_t ov = readBE32(page + off + local);
                long long remaining = payloadLen - local;
                while (ov != 0 && remaining > 0) {
                    const unsigned char* op = base + (size_t)(ov - 1) * pageSize;
                    uint32_t next = readBE32(op);   // 每個溢位頁前 4 bytes 指向下一頁
                    long long take = std::min<long long>(usable - 4, remaining);
                    payload.insert(payload.end(), op + 4, op + 4 + take);
                    remaining -= take;
                    ov = next;
                }
            }

            cb(rowid, decodeRecord(payload));
        }
    }
    // 其他頁型別(索引等)我們用不到，直接忽略。
}

} // namespace gridpp_detail


class GridEngine;   // 先宣告，讓 GridObject 能保存指向引擎的指標

// =============================================================================
//  Part B. GridObject —— 遊戲物件基底類別（OOP 教學核心）
//  繼承它並覆寫 onStart / onUpdate / onCollide。詳見 docs/guide/game-objects.md。
// =============================================================================
class GridObject {
public:
    GridObject()
        : gridX(0), gridY(0), assetName("") {}
    GridObject(std::string assetName, int x, int y)
        : gridX(x), gridY(y), assetName(assetName) {}

    virtual ~GridObject() {}

    // 生命週期：由引擎自動呼叫，學生覆寫這些。
    virtual void onStart() {}                                    // 出生時呼叫一次
    virtual void onUpdate() {}                                   // 每一幀呼叫
    virtual void onCollide(GridObject* other) { (void)other; }   // 同格碰撞時呼叫

    int  getX() const { return gridX; }
    int  getY() const { return gridY; }
    void setX(int x)  { gridX = x; }
    void setY(int y)  { gridY = y; }
    void move(int dx, int dy) { gridX += dx; gridY += dy; }

    std::string getAssetName() const { return assetName; }

    std::string getTag() const { return tag; }          // 身分標記，碰撞時分辨對方
    void setTag(const std::string& t) { tag = t; }

    // 朝向：0~3，逆時針每 +1 轉 90°（0 = 素材原本方向）。用來旋轉素材、重複利用同一張圖。
    int  getDirection() const { return direction; }
    void setDirection(int dir) { direction = ((dir % 4) + 4) % 4; }

    // 調色(diffuse color)：繪製時把素材整體乘上這個顏色，預設 WHITE = 不變。
    // 例如白色的鬼魂素材 setTint(RED) 就會變成紅色的鬼。
    Color getTint() const { return tint; }
    void  setTint(Color c) { tint = c; }

    // 把自己畫出來；預設畫 assetName，子類別可覆寫（如 GridMaze 畫整片牆）。
    // 定義在本檔最後，因為需要用到尚未宣告完整的 GridEngine。
    virtual void render(GridEngine* engine);

    void setEngine(GridEngine* e) { engine = e; }       // 由引擎在 spawn() 時設定
    GridEngine* getEngine() const { return engine; }    // 取得所屬引擎（例如查地圖大小做邊界檢查）

protected:
    int gridX, gridY;
    std::string assetName;
    std::string tag;
    int direction = 0;        // 0~3，繪製時旋轉用
    Color tint = WHITE;       // 調色，預設不改變顏色
    GridEngine* engine = nullptr;
};


// =============================================================================
//  Part C. GridAssetManager —— 讀取 assets.db，把 sprites 做成 Texture2D。
//  用 multimap 存（name 允許重複），get() 遇重複名稱會丟例外。
//  詳見 docs/guide/assets.md。
// =============================================================================
class GridAssetManager {
public:
    void load(const std::string& path) {
        // 1) 把整個 .db 檔讀進記憶體
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("Grid++ 錯誤：找不到素材檔 '" + path + "'");
        std::vector<unsigned char> data(
            (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (data.size() < 100)
            throw std::runtime_error("Grid++ 錯誤：'" + path + "' 不是有效的 SQLite 檔");

        // 2) 從檔案標頭算出頁大小
        int pageSize = (data[16] << 8) | data[17];
        if (pageSize == 1) pageSize = 65536;       // SQLite 的特殊規定
        int reserved = data[20];
        int usable = pageSize - reserved;
        const unsigned char* base = data.data();

        // 3) 第 1 頁是「總目錄(sqlite_master)」，找出 sprites 表存在哪一頁。
        //    sqlite_master 每列欄位順序為：type, name, tbl_name, rootpage, sql
        uint32_t spritesRoot = 0;
        gridpp_detail::walkTable(base, pageSize, usable, 1,
            [&](long long, const std::vector<gridpp_detail::Column>& cols) {
                if (cols.size() >= 4 && cols[0].kind == 1 && cols[3].kind == 0) {
                    std::string ttype(cols[0].bytes.begin(), cols[0].bytes.end());
                    std::string tname(cols[2].bytes.begin(), cols[2].bytes.end());
                    if (ttype == "table" && tname == "sprites")
                        spritesRoot = (uint32_t)cols[3].i;
                }
            });
        if (spritesRoot == 0)
            throw std::runtime_error("Grid++ 錯誤：'" + path + "' 裡找不到 sprites 資料表");

        // 4) 走訪 sprites 表，把每一列做成 Texture2D。
        //    欄位順序：id(0), name(1), tags(2), image_data(3)
        gridpp_detail::walkTable(base, pageSize, usable, spritesRoot,
            [&](long long, const std::vector<gridpp_detail::Column>& cols) {
                if (cols.size() < 4) return;
                std::string name(cols[1].bytes.begin(), cols[1].bytes.end());
                const std::vector<unsigned char>& blob = cols[3].bytes;
                if (blob.size() != 4096) {           // 必須是 32x32 RGBA = 4096 bytes
                    std::cout << "Grid++ 警告：素材 '" << name
                              << "' 不是 32x32 RGBA，已略過。\n";
                    return;
                }
                textures.insert({ name, makeTexture(blob) });
            });

        // 5) 載入時先檢查有沒有重複名稱，提早提醒。
        for (auto it = textures.begin(); it != textures.end(); ) {
            const std::string key = it->first;
            size_t c = textures.count(key);
            if (c > 1)
                std::cout << "Grid++ 警告：素材名稱 '" << key << "' 重複了 " << c
                          << " 次！之後呼叫 get(\"" << key << "\") 會直接報錯。\n";
            it = textures.upper_bound(key);
        }
    }

    // 依名稱取得素材；名稱重複時丟例外（刻意的教學提醒）。
    Texture2D get(const std::string& name) {
        size_t c = textures.count(name);
        if (c == 0)
            throw std::runtime_error("Grid++ 錯誤：找不到素材 '" + name + "'");
        if (c > 1)
            throw std::runtime_error(
                "Grid++ 錯誤：素材名稱 '" + name + "' 重複出現 " + std::to_string(c) +
                " 次，無法分辨你要哪一個！請讓素材名稱保持唯一。");
        return textures.find(name)->second;
    }

    bool has(const std::string& name) const { return textures.count(name) >= 1; }

private:
    // 4096 bytes 的 RGBA 原始資料 → 一張 Raylib 材質（需先有視窗才能上傳顯卡）。
    static Texture2D makeTexture(const std::vector<unsigned char>& rgba) {
        Image img = { 0 };
        img.data    = (void*)rgba.data();
        img.width   = 32;
        img.height  = 32;
        img.mipmaps = 1;
        img.format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        return LoadTextureFromImage(img);
    }

    std::multimap<std::string, Texture2D> textures;
};


// =============================================================================
//  Part C-2. UIElement —— UI 覆蓋層的基底類別（跳脫網格，用像素座標繪製）
//  繼承它、覆寫 draw()（繪製）與 onUpdate()（互動），再用 engine.addUI() 加入。
//  UI 會畫在所有網格物件「之上」。現成的 Label / Button 在選用模組 GridUI.h。
//  詳見 docs/guide/ui.md。
// =============================================================================
class UIElement {
public:
    virtual ~UIElement() {}
    virtual void onUpdate() {}    // 每幀呼叫（互動用，例如讀滑鼠）
    virtual void draw() {}        // 每幀呼叫，用像素座標自由繪製
};


// =============================================================================
//  Part D. GridEngine —— 開視窗、跑主循環、畫網格/物件、處理碰撞。
//  流程：建立 → loadAssets → spawn → run。詳見 docs/guide/grid-and-engine.md。
// =============================================================================
class GridEngine {
public:
    GridEngine(int cols, int rows, int gridSize = 32)
        : cols(cols), rows(rows), gridSize(gridSize) {
        // 先開視窗，之後載入素材時才能把圖片上傳到顯示卡。
        InitWindow(cols * gridSize, rows * gridSize, "Grid++ Game");
        SetTargetFPS(60);
    }

    ~GridEngine() { CloseWindow(); }

    void loadAssets(const std::string& dbPath) { assets.load(dbPath); }

    // 設定背景顏色（每幀清畫面時使用），預設 RAYWHITE。
    void  setBackgroundColor(Color c) { bgColor = c; }
    Color getBackgroundColor() const  { return bgColor; }

    // 開啟/關閉網格線（每格之間的淺灰線），預設關閉，需要時自行打開。
    void setShowGrid(bool show) { showGrid = show; }
    bool getShowGrid() const    { return showGrid; }

    // 把物件放進世界，並立刻呼叫它的 onStart()。
    void spawn(GridObject* obj) {
        obj->setEngine(this);
        objects.push_back(obj);
        obj->onStart();
    }

    // 把 UI 元件加入覆蓋層（畫在所有網格物件之上）。
    void addUI(UIElement* e) { uiElements.push_back(e); }

    // 清掉所有遊戲物件（用於「重新開始」；不影響 UI 與已載入的素材）。
    void clearObjects() {
        for (GridObject* o : objects) delete o;
        objects.clear();
    }

    // 內建遊戲主循環：每一幀做「更新 → 碰撞 → 繪製」（實作在 tick()）。
    void run() {
#ifdef __EMSCRIPTEN__
        // 瀏覽器(WebAssembly)不允許用 while 霸佔執行緒，必須把每一幀
        // 交還給瀏覽器的事件迴圈。emscripten 會以 60fps 反覆呼叫 tick()。
        emscripten_set_main_loop_arg(
            [](void* self) { static_cast<GridEngine*>(self)->tick(); },
            this, 0, 1);
#else
        while (!WindowShouldClose()) tick();   // 按 ESC 或關視窗就會結束
#endif
    }

    int getCols() const { return cols; }
    int getRows() const { return rows; }
    int getGridSize() const { return gridSize; }

    // 在某格畫上指定素材，可旋轉(direction 0~3)與調色(tint)。
    // 找不到素材就畫紅色方塊當預設外觀。給 render() 使用。
    void drawCell(const std::string& asset, int gx, int gy,
                  int direction = 0, Color tint = WHITE) {
        int px = gx * gridSize, py = gy * gridSize;
        if (!asset.empty() && assets.has(asset)) {
            Texture2D tex = assets.get(asset);
            Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
            // 以格子中心為旋轉軸：dest 用中心點，origin 也設成中心。
            Rectangle dst = { (float)px + gridSize / 2.0f, (float)py + gridSize / 2.0f,
                              (float)gridSize, (float)gridSize };
            Vector2 origin = { gridSize / 2.0f, gridSize / 2.0f };
            DrawTexturePro(tex, src, dst, origin, -90.0f * direction, tint); // 負角度=逆時針
        } else {
            DrawRectangle(px, py, gridSize, gridSize, RED);
        }
    }

private:
    // 推進一幀：更新 → 碰撞 → 繪製。native 與 WebAssembly 兩種模式都靠它。
    void tick() {
        // (1) 更新
        for (GridObject* o : objects) o->onUpdate();
        for (UIElement* u : uiElements) u->onUpdate();

        // (2) 碰撞：兩兩比對，同一格就互相通知
        for (size_t i = 0; i < objects.size(); i++)
            for (size_t j = i + 1; j < objects.size(); j++)
                if (objects[i]->getX() == objects[j]->getX() &&
                    objects[i]->getY() == objects[j]->getY()) {
                    objects[i]->onCollide(objects[j]);
                    objects[j]->onCollide(objects[i]);
                }

        // (3) 繪製
        BeginDrawing();
        ClearBackground(bgColor);
        if (showGrid) drawGrid();
        for (GridObject* o : objects) o->render(this);
        for (UIElement* u : uiElements) u->draw();   // UI 疊在最上層
        EndDrawing();
    }

    void drawGrid() {
        for (int x = 0; x <= cols; x++)
            DrawLine(x * gridSize, 0, x * gridSize, rows * gridSize, LIGHTGRAY);
        for (int y = 0; y <= rows; y++)
            DrawLine(0, y * gridSize, cols * gridSize, y * gridSize, LIGHTGRAY);
    }

    int cols, rows, gridSize;
    Color bgColor = RAYWHITE;     // 背景色，由 setBackgroundColor() 設定
    bool  showGrid = false;       // 是否畫網格線（預設關），由 setShowGrid() 切換
    GridAssetManager assets;
    std::vector<GridObject*> objects;
    std::vector<UIElement*> uiElements;
};


// GridObject 的預設畫法（定義在最後，因為需要完整的 GridEngine 才能呼叫 drawCell）。
inline void GridObject::render(GridEngine* engine) {
    engine->drawCell(assetName, gridX, gridY, direction, tint);
}

#endif // GRIDPLUSPLUS_H
