/**
 * @file GridPlusPlus.h
 * @brief Grid++ 核心介面。
 *
 * 提供遊戲物件、素材管理、UI 基底與遊戲主循環。
 * 繪圖與視窗功能依賴 raylib。
 */
#ifndef GRIDPLUSPLUS_H
#define GRIDPLUSPLUS_H

#include "raylib.h"
#include "GridSQLite.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstdint>
#include <fstream>
#include <iostream>


class GridEngine;

// 遊戲物件基底類別
// 繼承它並覆寫 onStart / onUpdate / onCollide。詳見 docs/guide/game-objects.md。
class GridObject {
public:
    GridObject()
        : gridX(0), gridY(0), assetName("") {}
    GridObject(std::string assetName, int x, int y)
        : gridX(x), gridY(y), assetName(assetName) {}

    virtual ~GridObject() {}

    // 引擎呼叫的生命週期函式。
    virtual void onStart() {}
    virtual void onUpdate() {}
    virtual void onCollide(GridObject* other) { (void)other; }

    int  getX() const { return gridX; }
    int  getY() const { return gridY; }
    void setX(int x)  { gridX = x; }
    void setY(int y)  { gridY = y; }
    void move(int dx, int dy) { gridX += dx; gridY += dy; }

    std::string getAssetName() const { return assetName; }

    // 身分標記，碰撞時分辨對方
    std::string getTag() const { return tag; }
    void setTag(const std::string& t) { tag = t; }

    // 方向為 0 到 3，每增加 1 逆時針旋轉 90 度。
    int  getDirection() const { return direction; }
    void setDirection(int dir) { direction = ((dir % 4) + 4) % 4; }

    // 調色。繪製時把素材整體乘上這個顏色。預設 WHITE (不改變顏色)。
    Color getTint() const { return tint; }
    void  setTint(Color c) { tint = c; }

    // 把自己畫出來；預設繪製 assetName，子類別可覆寫。
    virtual void render(GridEngine* engine);

    void setEngine(GridEngine* e) { engine = e; }
    GridEngine* getEngine() const { return engine; }

protected:
    int gridX, gridY;
    std::string assetName;
    std::string tag;
    int direction = 0;
    Color tint = WHITE;
    GridEngine* engine = nullptr;
};


// 從素材資料庫建立 raylib Texture2D
//  詳見 docs/guide/assets.md。
class GridAssetManager {
public:
    void load(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("Grid++ 錯誤：找不到素材檔 '" + path + "'");
        std::vector<unsigned char> data(
            (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        gridpp_detail::SQLiteReader database(data);

        // sqlite_master 欄位依序為 type、name、tbl_name、rootpage、sql。
        uint32_t spritesRoot = 0;
        database.walkTable(1,
            [&](int64_t, const std::vector<gridpp_detail::Column>& cols) {
                if (cols.size() >= 4 && cols[0].kind == 1 && cols[3].kind == 0) {
                    std::string ttype(cols[0].bytes.begin(), cols[0].bytes.end());
                    std::string tname(cols[2].bytes.begin(), cols[2].bytes.end());
                    if (ttype == "table" && tname == "sprites")
                        spritesRoot = (uint32_t)cols[3].i;
                }
            });
        if (spritesRoot == 0)
            throw std::runtime_error("Grid++ 錯誤：'" + path + "' 裡找不到 sprites 資料表");

        // sprites 欄位依序為 id、name、tags、image_data。
        database.walkTable(spritesRoot,
            [&](int64_t, const std::vector<gridpp_detail::Column>& cols) {
                if (cols.size() < 4) return;
                std::string name(cols[1].bytes.begin(), cols[1].bytes.end());
                const std::vector<unsigned char>& blob = cols[3].bytes;
                if (blob.size() != 4096) {
                    std::cout << "Grid++ 警告：素材 '" << name
                              << "' 不是 32x32 RGBA，已略過。\n";
                    return;
                }
                textures.insert({ name, makeTexture(blob) });
            });

        // 載入後檢查重複名稱。
        for (auto it = textures.begin(); it != textures.end(); ) {
            const std::string key = it->first;
            size_t c = textures.count(key);
            if (c > 1)
                std::cout << "Grid++ 警告：素材名稱 '" << key << "' 重複了 " << c
                          << " 次！之後呼叫 get(\"" << key << "\") 會直接報錯。\n";
            it = textures.upper_bound(key);
        }
    }

    // 名稱不存在或重複時丟出例外。
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
    // 將 32×32 RGBA 資料上傳為 raylib texture。
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


// 使用像素座標的 UI 覆蓋層基底類別
// 詳見 docs/guide/ui.md。
class UIElement {
public:
    virtual ~UIElement() {}
    virtual void onUpdate() {}
    virtual void draw() {}
};


// 建立視窗並管理主迴圈、物件與碰撞
// 詳見 docs/guide/grid-and-engine.md
class GridEngine {
public:
    GridEngine(int cols, int rows, int gridSize = 32)
        : cols(cols), rows(rows), gridSize(gridSize) {
        // 材質需要在視窗建立後才能上傳至 GPU。
        InitWindow(cols * gridSize, rows * gridSize, "Grid++ Game");
        SetTargetFPS(60);
    }

    ~GridEngine() { CloseWindow(); }

    void loadAssets(const std::string& dbPath) { assets.load(dbPath); }

    // 背景預設為 RAYWHITE。
    void  setBackgroundColor(Color c) { bgColor = c; }
    Color getBackgroundColor() const  { return bgColor; }

    // 網格線預設關閉。
    void setShowGrid(bool show) { showGrid = show; }
    bool getShowGrid() const    { return showGrid; }

    // 加入物件並呼叫 onStart。
    void spawn(GridObject* obj) {
        obj->setEngine(this);
        objects.push_back(obj);
        obj->onStart();
    }

    void addUI(UIElement* e) { uiElements.push_back(e); }

    // 刪除所有遊戲物件，不影響 UI 與素材。
    void clearObjects() {
        for (GridObject* o : objects) delete o;
        objects.clear();
    }

    void run() {
#ifdef __EMSCRIPTEN__
        // WebAssembly 主迴圈由瀏覽器排程。
        emscripten_set_main_loop_arg(
            [](void* self) { static_cast<GridEngine*>(self)->tick(); },
            this, 0, 1);
#else
        while (!WindowShouldClose()) tick();
#endif
    }

    int getCols() const { return cols; }
    int getRows() const { return rows; }
    int getGridSize() const { return gridSize; }

    // 在網格座標繪製素材，素材不存在時繪製紅色方塊。
    void drawCell(const std::string& asset, int gx, int gy,
                  int direction = 0, Color tint = WHITE) {
        int px = gx * gridSize, py = gy * gridSize;
        if (!asset.empty() && assets.has(asset)) {
            Texture2D tex = assets.get(asset);
            Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
            // 以格子中心為旋轉軸。
            Rectangle dst = { (float)px + gridSize / 2.0f, (float)py + gridSize / 2.0f,
                              (float)gridSize, (float)gridSize };
            Vector2 origin = { gridSize / 2.0f, gridSize / 2.0f };
            DrawTexturePro(tex, src, dst, origin, -90.0f * direction, tint);
        } else {
            DrawRectangle(px, py, gridSize, gridSize, RED);
        }
    }

private:
    void tick() {
        // 更新
        for (GridObject* o : objects) o->onUpdate();
        for (UIElement* u : uiElements) u->onUpdate();

        // 碰撞
        for (size_t i = 0; i < objects.size(); i++)
            for (size_t j = i + 1; j < objects.size(); j++)
                if (objects[i]->getX() == objects[j]->getX() &&
                    objects[i]->getY() == objects[j]->getY()) {
                    objects[i]->onCollide(objects[j]);
                    objects[j]->onCollide(objects[i]);
                }

        // 繪製
        BeginDrawing();
        ClearBackground(bgColor);
        if (showGrid) drawGrid();
        for (GridObject* o : objects) o->render(this);
        for (UIElement* u : uiElements) u->draw();
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
    bool  showGrid = false;       // 是否畫網格線，由 setShowGrid() 切換
    GridAssetManager assets;
    std::vector<GridObject*> objects;
    std::vector<UIElement*> uiElements;
};


// GridEngine 定義完成後才能實作預設 render。
inline void GridObject::render(GridEngine* engine) {
    engine->drawCell(assetName, gridX, gridY, direction, tint);
}

#endif // GRIDPLUSPLUS_H
