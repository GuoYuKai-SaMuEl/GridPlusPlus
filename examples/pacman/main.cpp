// =============================================================================
//  examples/pacman —— Grid++ 範例：Pac-Man 小遊戲
//
//  編譯指令、map.txt 格式、遊戲流程與邏輯總覽見同資料夾的 README.md；
//  逐行拆解見文件 docs/tutorial/pacman.md。
// =============================================================================
#include "GridMaze.h"   // 用 -I../.. 指到專案根；會自動帶進核心 GridPlusPlus.h
#include "GridUI.h"     // Label / Button 等 UI 元件
#include <fstream>      // 讀地圖檔
#include <string>

using namespace std;

// 本範例除了 string 外不使用 STL 容器：地圖邊讀邊處理，物件直接 spawn。

// 遊戲階段：0=開始畫面, 1=遊戲中, 2=獲勝, 3=失敗
int         g_state       = 0;
int         g_pelletsLeft = 0;        // 還剩幾顆豆子
bool        g_paused      = false;    // 是否暫停（由暫停按鈕切換）
GridMaze*   g_maze        = nullptr;  // 讓角色查詢牆壁
GridObject* g_player      = nullptr;  // 讓鬼魂知道玩家在哪
GridEngine* g_game        = nullptr;  // 讓「重新開始」按鈕能重建世界

// 四個方向，對應 setDirection 的 0~3：右、上、左、下
static const int DX[4] = { 1, 0, -1, 0 };
static const int DY[4] = { 0, -1, 0, 1 };

// 曼哈頓距離（自己寫，避免引入 <cstdlib> 的 abs）
static int manhattan(int ax, int ay, int bx, int by) {
    int dx = ax - bx, dy = ay - by;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy;
}

// 豆子：被小精靈吃到就消失
class Pellet : public GridObject {
public:
    Pellet(int x, int y) : GridObject("pellet", x, y) { setTag("pellet"); }

    void onCollide(GridObject* other) override {
        if (!eaten && other->getTag() == "pacman") {
            eaten = true;
            if (--g_pelletsLeft <= 0) g_state = 2;     // 吃完 → 獲勝
        }
    }
    void render(GridEngine* engine) override {
        if (!eaten) GridObject::render(engine);
    }
private:
    bool eaten = false;
};

// 鬼魂：貪心追逐（挑離玩家最近、且不回頭的方向）或隨機。各自有不同顏色(tint)。
class Ghost : public GridObject {
public:
    Ghost(int x, int y, Color color, bool isRandom)
        : GridObject("ghost", x, y), random(isRandom) {
        setTag("ghost");
        setTint(color);                                // 同一張白色素材染成不同顏色
    }

    void onUpdate() override {
        if (g_state != 1 || g_paused || !g_player) return;
        if (++timer < 12) return;                      // 移動速度（比玩家稍慢）
        timer = 0;

        int options[4], nopt = 0;                      // 非牆、且不回頭
        int any[4], nany = 0;                          // 非牆（死路時才用，允許回頭）
        for (int d = 0; d < 4; d++) {
            if (g_maze->isWall(getX() + DX[d], getY() + DY[d])) continue;
            any[nany++] = d;
            if (d == (dir ^ 2)) continue;              // d 與目前方向相反 → 回頭，先跳過
            options[nopt++] = d;
        }
        int* pool = nopt ? options : any;
        int n     = nopt ? nopt   : nany;
        if (n == 0) return;

        int pick = pool[0];
        if (random) {
            pick = pool[GetRandomValue(0, n - 1)];     // 隨機鬼
        } else {                                       // 貪心鬼：挑離玩家曼哈頓距離最小的
            int best = 1 << 30;
            for (int k = 0; k < n; k++) {
                int d = pool[k];
                int dist = manhattan(getX() + DX[d], getY() + DY[d],
                                     g_player->getX(), g_player->getY());
                if (dist < best) { best = dist; pick = d; }
            }
        }
        dir = pick;
        move(DX[dir], DY[dir]);
    }
private:
    bool random;
    int  timer = 0;
    int  dir   = 0;
};

// 小精靈（玩家）：持續朝目前方向走；按方向鍵且該方向不是牆才轉向。
class Pacman : public GridObject {
public:
    Pacman(int x, int y) : GridObject("pacman", x, y) { setTag("pacman"); }

    void onUpdate() override {
        if (g_state != 1 || g_paused) return;
        if (IsKeyDown(KEY_RIGHT)) want = 0;            // 每幀記下想要的方向（緩衝）
        if (IsKeyDown(KEY_UP))    want = 1;
        if (IsKeyDown(KEY_LEFT))  want = 2;
        if (IsKeyDown(KEY_DOWN))  want = 3;

        if (++timer < 8) return;                       // 移動速度
        timer = 0;

        if (want >= 0 && !g_maze->isWall(getX() + DX[want], getY() + DY[want]))
            dir = want;                                // 想要的方向合法 → 轉過去
        if (dir >= 0 && !g_maze->isWall(getX() + DX[dir], getY() + DY[dir])) {
            move(DX[dir], DY[dir]);
            setDirection(dir);                         // 旋轉素材朝向移動方向
        }
    }
    void onCollide(GridObject* other) override {
        if (other->getTag() == "ghost") g_state = 3;   // 被鬼抓到 → 失敗
    }
private:
    int timer = 0;
    int dir   = -1;     // -1 = 還沒開始移動
    int want  = -1;
};

// 依 map.txt 佈置一局：清掉上一局 → 重建迷宮、豆子、角色。開始與重新開始都呼叫它。
void buildLevel(GridEngine& game) {
    game.clearObjects();
    g_pelletsLeft = 0;
    g_paused = false;

    ifstream f("map.txt");
    int rows, cols;
    f >> rows >> cols;

    GridMaze* maze = new GridMaze(cols, rows);
    // 自動拼接：給 6 種基本牆形狀，其餘方向引擎會旋轉素材湊出來。
    maze->setWallTiles("wall_iso", "wall_end", "wall_straight",
                       "wall_corner", "wall_tee", "wall_cross");
    game.spawn(maze);                                  // 先放迷宮（畫最底層）
    g_maze = maze;

    Color  colors[4] = { RED, PINK, SKYBLUE, ORANGE }; // 四隻鬼的顏色
    int    gi = 0;
    Pacman* player = nullptr;
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++) {
            int t;
            f >> t;
            if      (t == 1) maze->setWall(x, y, true);
            else if (t == 0) { game.spawn(new Pellet(x, y)); g_pelletsLeft++; }
            else if (t == 2) player = new Pacman(x, y);
            else if (t == 3) { game.spawn(new Ghost(x, y, colors[gi % 4], gi == 3)); gi++; }
        }
    if (player) { g_player = player; game.spawn(player); }  // 玩家最後生成（畫最上層）
}

// 分數與各畫面的訊息（UI 覆蓋層，跳脫網格、用像素座標）
class ScoreUI : public UIElement {
public:
    void draw() override {
        if (g_state == 1) {                            // 遊戲中：左上角分數
            DrawText(TextFormat("Pellets: %d", g_pelletsLeft), 8, 8, 20, YELLOW);
            if (g_paused) bigText("PAUSED", ORANGE);
            return;
        }
        // 非遊戲中：壓暗背景，畫大字（按鈕由各自的類別畫在這之上）
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));
        if      (g_state == 0) bigText("PAC-MAN",   YELLOW);
        else if (g_state == 2) bigText("YOU WIN!",  GREEN);
        else if (g_state == 3) bigText("GAME OVER", RED);
    }
private:
    static void bigText(const char* msg, Color col) {
        int fs = 40, tw = MeasureText(msg, fs);
        DrawText(msg, GetScreenWidth() / 2 - tw / 2, GetScreenHeight() / 2 - 70, fs, col);
    }
};

// 各按鈕只在「對的階段」顯示與作用：覆寫 onClick（行為）與 onUpdate/draw（階段判斷）。
class StartButton : public Button {
public:
    StartButton(int x, int y, int w, int h) : Button("Start", x, y, w, h) {}
    void onClick()  override { g_state = 1; }                          // 開始遊戲
    void onUpdate() override { if (g_state == 0) Button::onUpdate(); }
    void draw()     override { if (g_state == 0) Button::draw(); }
};

class RestartButton : public Button {
public:
    RestartButton(int x, int y, int w, int h) : Button("Restart", x, y, w, h) {}
    void onClick()  override { buildLevel(*g_game); g_state = 1; }      // 重來一局
    void onUpdate() override { if (g_state == 2 || g_state == 3) Button::onUpdate(); }
    void draw()     override { if (g_state == 2 || g_state == 3) Button::draw(); }
};

class PauseButton : public Button {
public:
    PauseButton(int x, int y, int w, int h) : Button("Pause", x, y, w, h) {}
    void onClick()  override { g_paused = !g_paused; }
    void onUpdate() override { if (g_state == 1) Button::onUpdate(); }
    void draw()     override { if (g_state == 1) Button::draw(); }
};

int main() {
    int rows, cols;
    { ifstream f("map.txt"); f >> rows >> cols; }      // 先讀尺寸決定視窗大小

    GridEngine game(cols, rows, 32);
    g_game = &game;
    game.loadAssets("pacman.db");
    game.setBackgroundColor(BLACK);                    // Pac-Man 經典黑底（格線預設已關）

    buildLevel(game);                                  // 先建好一局
    g_state = 0;                                        // 停在開始畫面

    // UI 覆蓋層（ScoreUI 先加，按鈕畫在它之上）
    int bw = 120, bh = 40, bx = cols * 32 / 2 - bw / 2, by = rows * 32 / 2;
    game.addUI(new ScoreUI());
    game.addUI(new StartButton(bx, by, bw, bh));
    game.addUI(new RestartButton(bx, by, bw, bh));
    game.addUI(new PauseButton(cols * 32 - 88, 6, 82, 24));

    game.run();
    return 0;
}
