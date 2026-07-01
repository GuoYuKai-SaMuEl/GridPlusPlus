// =============================================================================
//  examples/pacman_easy —— Pac-Man「入門版」（示範 GridEasy.h 的 EasyObject）
//
//  刻意寫得像 C：沒有自訂 class、沒有繼承，只用「全域變數 + 一般函式」。
//  角色的行為靠 EasyObject——先寫好一個函式，初始化時把「函式的名字」傳進去即可。
//  因為只有一個玩家、一隻鬼，它們的狀態放全域變數就夠了。
//
//  玩法：方向鍵移動，吃光所有豆子獲勝，碰到鬼魂失敗。（結束後畫面會定格）
//
//  編譯（在本資料夾內，raylib 已安裝；其他平台見 docs/getting-started.md）：
//    g++ main.cpp -I../.. -o game -lraylib -lopengl32 -lgdi32 -lwinmm
//  執行前確保本資料夾有 pacman.db（素材，沿用 examples/pacman 的那份）。
// =============================================================================
#include "GridEasy.h"   // EasyObject（會自動帶進核心 GridPlusPlus.h）
#include "GridMaze.h"    // 迷宮
#include <cstdio>        // printf

#define W 11             // 地圖寬（欄）
#define H 9              // 地圖高（列）

// 地圖：1=牆  0=豆子  2=玩家起點  3=鬼魂起點
int gMap[H][W] = {
    {1,1,1,1,1,1,1,1,1,1,1},
    {1,2,0,0,0,0,0,0,0,0,1},
    {1,0,1,0,1,0,1,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,0,1,0,1,0,1,0,1},
    {1,0,0,0,0,3,0,0,0,0,1},
    {1,0,1,0,1,0,1,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1},
};

// 遊戲用的全域變數（只有一個玩家、一隻鬼，所以放全域就好）
GridMaze*   gMaze       = NULL;   // 用來查某一格是不是牆
GridObject* gPlayer     = NULL;   // 讓鬼知道玩家在哪
int         gPellets    = 0;      // 還剩幾顆豆子
int         gGhostTimer = 0;      // 鬼的移動計時器
int         gState      = 0;      // 0=遊戲中  1=獲勝  2=失敗

// 這一格是不是牆？（玩家和鬼都用它來判斷能不能走）
int isWall(int x, int y) {
    return gMaze->isWall(x, y);
}

// 玩家：方向鍵按一下走一格，前面是牆就不走
void playerMove(GridObject* self) {
    if (gState != 0) return;                              // 遊戲結束就不再動
    int x = self->getX();
    int y = self->getY();
    if (IsKeyPressed(KEY_RIGHT) && !isWall(x + 1, y)) self->move(1, 0);
    if (IsKeyPressed(KEY_LEFT)  && !isWall(x - 1, y)) self->move(-1, 0);
    if (IsKeyPressed(KEY_DOWN)  && !isWall(x, y + 1)) self->move(0, 1);
    if (IsKeyPressed(KEY_UP)    && !isWall(x, y - 1)) self->move(0, -1);
}

// 玩家撞到東西：撞到鬼就失敗
void playerHit(GridObject* self, GridObject* other) {
    if (other->getTag() == "ghost") {
        gState = 2;
        SetWindowTitle("GAME OVER");
        printf("被鬼抓到了，失敗！\n");
    }
}

// 豆子被玩家吃到：移出地圖（等於消失），剩餘數量 -1；吃光就贏
void pelletEaten(GridObject* self, GridObject* other) {
    if (other->getTag() == "player") {
        self->setX(-1);                                  // 移到畫面外：看不到、也不會再被吃
        self->setY(-1);
        gPellets = gPellets - 1;
        if (gPellets <= 0) {
            gState = 1;
            SetWindowTitle("YOU WIN!");
            printf("豆子吃光了，獲勝！\n");
        }
    }
}

// 鬼：每隔幾幀往玩家的方向走一格（會避開牆）
void ghostMove(GridObject* self) {
    if (gState != 0) return;
    gGhostTimer = gGhostTimer + 1;
    if (gGhostTimer < 15) return;                        // 每 15 幀才走一次（比玩家慢）
    gGhostTimer = 0;

    int gx = self->getX();
    int gy = self->getY();
    int px = gPlayer->getX();
    int py = gPlayer->getY();

    // 先往玩家的左右方向靠近，走不動再試上下
    if      (px > gx && !isWall(gx + 1, gy)) self->move(1, 0);
    else if (px < gx && !isWall(gx - 1, gy)) self->move(-1, 0);
    else if (py > gy && !isWall(gx, gy + 1)) self->move(0, 1);
    else if (py < gy && !isWall(gx, gy - 1)) self->move(0, -1);
}

int main() {
    GridEngine game(W, H, 40);            // W×H 格，每格 40 像素
    game.loadAssets("pacman.db");         // 沿用 pacman 範例的素材
    game.setBackgroundColor(BLACK);       // 經典黑底

    // 建立迷宮：所有牆都用同一張圖（最簡單的畫法）
    gMaze = new GridMaze(W, H);
    gMaze->setWallAsset("wall_cross");
    game.spawn(gMaze);

    // 照著 gMap 擺好牆、豆子、玩家、鬼
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int t = gMap[y][x];
            if (t == 1) {
                gMaze->setWall(x, y, true);
            } else if (t == 0) {
                game.spawn(new EasyObject("pellet", x, y, NULL, pelletEaten));
                gPellets = gPellets + 1;
            } else if (t == 2) {
                gPlayer = new EasyObject("pacman", x, y, playerMove, playerHit);
                gPlayer->setTag("player");
                game.spawn(gPlayer);
            } else if (t == 3) {
                EasyObject* ghost = new EasyObject("ghost", x, y, ghostMove);
                ghost->setTag("ghost");
                ghost->setTint(RED);      // 白色鬼魂素材染成紅色
                game.spawn(ghost);
            }
        }
    }

    SetWindowTitle("Pac-Man (Easy) - arrow keys");
    game.run();
    return 0;
}
