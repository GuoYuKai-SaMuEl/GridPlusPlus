// =============================================================================
//  GridMaze.h —— Grid++ 的「迷宮」擴充模組（選用）
//  需要迷宮時才 #include "GridMaze.h"（會自動帶進核心 GridPlusPlus.h）。
//  它是一個會把整片牆畫出來的 GridObject。用法詳見 docs/guide/maze.md。
// =============================================================================
#ifndef GRIDMAZE_H
#define GRIDMAZE_H

#include "GridPlusPlus.h"

class GridMaze : public GridObject {
public:
    // 建立 cols×rows 的迷宮，一開始全是空地；再用 setWall 逐格設定牆壁。
    GridMaze(int cols, int rows) {
        gridX = -1; gridY = -1;   // 放到畫面外，避開引擎的同格碰撞
        tag = "maze";
        width  = (cols <= MAX_W) ? cols : MAX_W;   // 夾在陣列容量內，避免超出
        height = (rows <= MAX_H) ? rows : MAX_H;
        for (int y = 0; y < height; y++)           // 一開始全部設為空地
            for (int x = 0; x < width; x++)
                walls[y][x] = false;
    }

    // 設定某一格是不是牆壁（界外會自動忽略）。
    void setWall(int x, int y, bool wall) {
        if (x >= 0 && y >= 0 && x < width && y < height) walls[y][x] = wall;
    }

    void setWallAsset(const std::string& asset) { singleWall = asset; }   // 所有牆同一張圖

    // 自動拼接：只給「6 種基本形狀」的素材，其餘 15 種連通情況由引擎「旋轉素材」湊出。
    // 每種形狀各畫成一個朝向即可（畫法見 docs/guide/maze.md）：
    //   孤立 / 端點(朝上) / 直線(上下) / 轉角(上右) / T形(上右下) / 十字
    void setWallTiles(const std::string& iso, const std::string& end,
                      const std::string& straight, const std::string& corner,
                      const std::string& tee, const std::string& cross) {
        wallTiles[0] = iso;    wallTiles[1] = end;  wallTiles[2] = straight;
        wallTiles[3] = corner; wallTiles[4] = tee;  wallTiles[5] = cross;
        useTiles = true;
    }

    // 給角色做碰撞判斷：這格是不是牆？（界外也當作牆）
    bool isWall(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return true;
        return walls[y][x];
    }

    int getWidth()  const { return width; }
    int getHeight() const { return height; }

    // 覆寫 render：把整片牆壁畫出來。
    void render(GridEngine* engine) override {
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                if (!walls[y][x]) continue;
                if (useTiles) {                       // 自動拼接：選基本形狀 + 旋轉
                    int shape, dir;
                    shapeFor(connectivityMask(x, y), shape, dir);
                    engine->drawCell(wallTiles[shape], x, y, dir);
                } else {
                    engine->drawCell(singleWall, x, y);
                }
            }
    }

private:
    // 上下左右鄰居是否為牆 → 組成 bitmask（上1/右2/下4/左8）。
    int connectivityMask(int x, int y) const {
        int m = 0;
        if (isWall(x, y - 1)) m |= 1;
        if (isWall(x + 1, y)) m |= 2;
        if (isWall(x, y + 1)) m |= 4;
        if (isWall(x - 1, y)) m |= 8;
        return m;
    }

    // 把連通情況 mask 轉成「基本形狀索引 + 旋轉方向(0~3)」。
    // 形狀：0=孤立 1=端點 2=直線 3=轉角 4=T形 5=十字。
    // 例如各種「端點」都用同一張端點素材，只是旋轉到不同方向。
    static void shapeFor(int mask, int& shape, int& dir) {
        static const int SHAPE[16] = { 0,1,1,3,1,2,3,4,1,3,2,4,3,4,4,5 };
        static const int DIR[16]   = { 0,0,3,0,2,0,3,0,1,1,1,1,2,2,3,0 };
        shape = SHAPE[mask];
        dir   = DIR[mask];
    }

    static const int MAX_W = 64, MAX_H = 64; // 地圖上限（32px 下遠超螢幕容量）
    bool walls[MAX_H][MAX_W];                // walls[y][x]，true 表示牆壁
    int  width = 0, height = 0;
    std::string singleWall;          // 單一牆壁素材（簡單模式）
    std::string wallTiles[6];        // 6 種基本形狀的素材（自動拼接模式）
    bool useTiles = false;
};

#endif // GRIDMAZE_H
