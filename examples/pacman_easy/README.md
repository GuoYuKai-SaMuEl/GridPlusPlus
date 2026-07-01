# Pac-Man 入門版

用 [Grid++](../../docs/index.md) 寫成的 Pac-Man「入門版」，示範選用模組
[`GridEasy.h`](../../GridEasy.h) 的 `EasyObject`。

**刻意寫得像 C**：整支程式沒有自訂 `class`、沒有繼承，只用「全域變數 + 一般函式」。
角色的行為靠 `EasyObject`——先寫好一個普通函式，初始化時把「函式的名字」傳進去即可。
概念說明見 [入門版物件 (EasyObject)](../../docs/guide/easy-object.md)；
想看用繼承寫的完整版，見隔壁的 [`examples/pacman/`](../pacman/)。

> 玩法：**方向鍵**移動，吃光所有豆子獲勝，碰到鬼魂失敗。（結束後畫面會定格，重玩就重跑程式）

## 資料夾內容

| 檔案 | 說明 |
|---|---|
| `main.cpp` | 範例原始碼（地圖直接寫死在檔案裡，不讀外部檔案）。 |
| `pacman.db` | 素材包，沿用 `examples/pacman` 的那份。 |

## 編譯與執行

raylib 的安裝方式見 [開始使用](../../docs/getting-started.md)。`-I../..` 讓編譯器找到專案根目錄的
`GridEasy.h` / `GridMaze.h` / `GridPlusPlus.h`。依平台編譯：

=== "Windows"

    ```bash
    g++ main.cpp -I../.. -o game -lraylib -lopengl32 -lgdi32 -lwinmm
    ./game
    ```

    用 repo 內附的 raylib 動態庫時，改連 `-L../../lib -lraylibdll` 並把 `raylib.dll`
    複製到本資料夾。

=== "Linux"

    ```bash
    g++ main.cpp -I../.. -o game $(pkg-config --libs raylib)
    ./game
    ```

=== "macOS"

    ```bash
    clang++ main.cpp -I../.. -o game -lraylib \
        -framework OpenGL -framework Cocoa -framework IOKit \
        -framework CoreVideo -framework CoreAudio
    ./game
    ```

!!! note "執行前確認"
    程式以相對路徑讀取 `pacman.db`，所以要在 `examples/pacman_easy/` 資料夾內執行。

## 程式怎麼運作

**地圖**是一個寫死的二維陣列 `gMap[H][W]`（`1`=牆 `0`=豆子 `2`=玩家 `3`=鬼魂），
`main()` 用兩層 for 迴圈逐格擺好牆、豆子、玩家、鬼。迷宮用最簡單的畫法：
`GridMaze` + `setWallAsset("wall_cross")`，所有牆都用同一張實心方塊。

**角色的行為都是普通函式**，在 `new EasyObject(...)` 時傳進去：

- `playerMove(self)`：方向鍵按一下走一格，用 `isWall()` 檢查前面不是牆才走。
- `ghostMove(self)`：每 15 幀走一格（比玩家慢），往玩家的方向靠近、會避開牆。
- `pelletEaten(self, other)`：被玩家吃到就 `setX(-1)` 移出畫面（等於消失），剩餘 -1。
- `playerHit(self, other)`：撞到 tag 為 `ghost` 的東西就失敗。

**狀態全部放全域變數**：`gState`（遊戲中/贏/輸）、`gPellets`（剩幾顆豆子）、
`gGhostTimer`（鬼的計時器）、`gPlayer`（讓鬼知道玩家在哪）、`gMaze`（查牆用）。
因為**只有一個玩家、一隻鬼**，這些狀態放全域就夠——這正是 `EasyObject` 能用的前提。

!!! tip "兩個關鍵小地方"
    - **`self` 參數**：普通函式沒有 `this`，所以引擎把物件本身當 `self` 傳進來，
      要操作它就寫 `self->move(...)`。
    - **豆子不用記「吃了沒」**：直接把被吃的豆子移出畫面，就不需要每顆豆子各自的旗標——
      剛好繞過 `EasyObject`「沒有每實例狀態」的限制。

## 接下來

想讓「多隻鬼各自記住自己的計時器與方向」時，全域變數就不夠用了——那就是需要**類別**的時機。
把這些函式搬進繼承 `GridObject` 的子類別（用成員變數記狀態），就成了
[`examples/pacman/`](../pacman/) 的完整版。逐行拆解見 [Pac-Man 拆解](../../docs/tutorial/pacman.md)。
