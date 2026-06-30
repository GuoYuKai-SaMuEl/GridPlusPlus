# 開始使用

Grid++ 只依賴 [raylib](https://www.raylib.com/)（負責開視窗與畫圖），編譯器需支援 C++11
（GCC / Clang / MSVC 皆可）。下面依作業系統說明：先裝 raylib，再編譯內附的 Pac-Man 範例。

## 1. 安裝 raylib

=== "Windows"

    **方法 A：MSYS2 / MinGW（建議）**——裝好 [MSYS2](https://www.msys2.org/) 後，
    開「MSYS2 MinGW 64-bit」終端機：

    ```bash
    pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-raylib
    ```

    **方法 B：把 raylib 放進專案**——本 repo 已附 `raylib.h` 與 `lib/`
    （`libraylib.a` 靜態庫、`libraylibdll.a` + `raylib.dll` 動態庫），編譯時用
    `-I.`（找標頭）與 `-Llib`（找函式庫）指出位置即可，免另外安裝。

=== "Linux"

    用套件管理器安裝（Debian / Ubuntu 為例）：

    ```bash
    sudo apt install build-essential libraylib-dev   # 或從原始碼編譯 raylib
    ```

    找不到 `libraylib-dev` 套件時，依
    [raylib 官方說明](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux)
    從原始碼編譯安裝即可。

=== "macOS"

    用 [Homebrew](https://brew.sh/) 安裝：

    ```bash
    brew install raylib
    ```

## 2. 跑跑看內附的 Pac-Man 範例

範例放在 `examples/pacman/`。先進到該資料夾，確認裡面有 `map.txt` 與 `assets.db` 素材包，
再依平台編譯（`-I../..` 是用來讓編譯器找到專案根目錄的 `GridMaze.h` / `GridPlusPlus.h`）：

```bash
cd examples/pacman
```

=== "Windows"

    raylib 已裝進工具鏈（MSYS2 / MinGW）時，要連結幾個 Windows 系統函式庫：

    ```bash
    g++ main.cpp -I../.. -o game -lraylib -lopengl32 -lgdi32 -lwinmm
    ./game
    ```

    若改用 repo 內附的 raylib 動態庫（`lib/`），加上 `-L` 並把 `raylib.dll`
    複製到 `game.exe` 同一資料夾：

    ```bash
    g++ main.cpp -I../.. -L../../lib -lraylibdll -lopengl32 -lgdi32 -lwinmm -o game
    cp ../../lib/raylib.dll .      # 動態庫：dll 要和 game.exe 同資料夾
    ./game
    ```

=== "Linux"

    系統函式庫換成 OpenGL / X11 / pthread / 數學庫：

    ```bash
    g++ main.cpp -I../.. -o game -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    ./game
    ```

    Wayland 環境若連結出錯，可改加 raylib 官方文件列出的 Wayland 函式庫。

=== "macOS"

    用 Clang，並以 `-framework` 連結 macOS 系統框架：

    ```bash
    clang++ main.cpp -I../.. -o game -lraylib \
        -framework OpenGL -framework Cocoa -framework IOKit \
        -framework CoreVideo -framework CoreAudio
    ./game
    ```

    若 `brew` 把 raylib 裝在非預設路徑，加上
    `-I$(brew --prefix raylib)/include -L$(brew --prefix raylib)/lib`。

操作：方向鍵移動，吃光所有豆子獲勝，被鬼魂抓到失敗。逐步拆解見 [Pac-Man 拆解](tutorial/pacman.md)，
範例資料夾本身也附有 `examples/pacman/README.md`（編譯指令與程式邏輯總覽）。

## 3. 開始寫自己的遊戲

最小骨架如下，把它存成 `.cpp`，用上面的方式編譯即可：

```cpp
#include "GridPlusPlus.h"

class Player : public GridObject {
public:
    Player() : GridObject("pacman", 5, 5) {}
    void onUpdate() override {
        if (IsKeyPressed(KEY_RIGHT)) move(1, 0);
    }
};

int main() {
    GridEngine game(10, 10, 32);
    game.loadAssets("assets.db");
    game.spawn(new Player());
    game.run();
}
```

接著建議從 [遊戲物件 (OOP)](guide/game-objects.md) 開始——那是你寫遊戲時最常用的部分。
