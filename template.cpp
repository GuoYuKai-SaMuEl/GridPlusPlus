// =============================================================================
//  template.cpp —— Grid++ 起手式模板（只用 GridPlusPlus.h，不含 GridMaze / GridUI）
//
//  要寫自己的遊戲，從這支複製一份開始改：一個角色用方向鍵在「地圖邊界以內」移動，
//  按一下走一格。沒有素材包：找不到素材時引擎會用紅色方塊代替，所以不需要 assets.db 就能跑。
//
//  編譯（在專案根目錄；raylib 已安裝。其他平台見 docs/getting-started.md）：
//    g++ template.cpp -o game -lraylib -lopengl32 -lgdi32 -lwinmm           # Windows / MinGW
//    g++ template.cpp -o game -lraylib -lGL -lm -lpthread -ldl -lrt -lX11   # Linux
// =============================================================================
#include "GridPlusPlus.h"

// 玩家：方向鍵移動。移動前先檢查目標格還在地圖內，避免走出邊界。
class Player : public GridObject {
public:
    Player(int x, int y) : GridObject("player", x, y) {}

    void onUpdate() override {
        // engine 是 GridObject 內建、指向所屬引擎的指標（spawn 時自動設好），
        // 用它查地圖有幾欄幾列來做邊界檢查。
        if (IsKeyPressed(KEY_RIGHT) && getX() < engine->getCols() - 1) move(1, 0);
        if (IsKeyPressed(KEY_LEFT)  && getX() > 0)                     move(-1, 0);
        if (IsKeyPressed(KEY_UP)    && getY() > 0)                     move(0, -1);
        if (IsKeyPressed(KEY_DOWN)  && getY() < engine->getRows() - 1) move(0, 1);
    }
};

int main() {
    GridEngine game(10, 10, 48);   // 10 欄 x 10 列，每格 48 像素
    game.setShowGrid(true);        // 打開網格線，看得出角色一次走一整格
    game.spawn(new Player(5, 5));  // 從正中央出發
    game.run();
    return 0;
}
