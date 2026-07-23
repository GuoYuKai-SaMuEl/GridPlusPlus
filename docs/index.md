# Grid++

**Grid++** 是一個專為「程式初學者」設計的輕量化、網格導向（grid-based）遊戲引擎。
它把圖形繪製、遊戲主循環、檔案讀取的複雜度全部藏起來，提供「一行搞定」的 API，
同時刻意留出接口，引導你練習 C++ 的物件導向（繼承、覆寫、重載、封裝）。

![Pac-Man 範例畫面](images/preview.png){ width="280" }

## 特色

- **Header-only**：`#include "GridPlusPlus.h"` 就能開始寫。
- **一行 API**：開視窗、載素材、放物件、開跑，各一行。
- **OOP 教學核心**：繼承 `GridObject`，覆寫 `onStart` / `onUpdate` / `onCollide`。
- **不需要連結 SQLite**：素材包 `assets.db` 由引擎內建的迷你讀取器處理。
- **選用的迷宮模組**：`GridMaze` 可讀地圖、自動拼接牆壁外觀、提供碰撞查詢。

## 最小範例

```cpp
#include "GridPlusPlus.h"

class Player : public GridObject {
public:
    Player() : GridObject("hero", 5, 5) {}
    void onUpdate() override {
        if (IsKeyPressed(KEY_RIGHT)) move(1, 0);
    }
};

int main() {
    GridEngine game(10, 10, 32);   // 10x10 網格，每格 32 像素
    game.loadAssets("assets.db");
    game.spawn(new Player());
    game.run();
}
```

## 接下來

- 還沒跑過？先看 [開始使用](getting-started.md)。
- 想了解核心概念？看 [網格與引擎](guide/grid-and-engine.md) 與 [遊戲物件](guide/game-objects.md)。
- 想做一個完整遊戲？看 [Pac-Man 拆解](tutorial/pacman.md)。
- 好奇素材包怎麼被讀出來？看 [迷你 SQLite 讀取器](internals/sqlite-reader.md)。
