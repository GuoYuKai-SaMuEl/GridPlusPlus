# 入門版物件 (EasyObject)

還沒學到「類別、繼承、覆寫」之前，也能做出會動的角色。`EasyObject`（在選用模組
`GridEasy.h`）讓你用最接近 C 的方式寫遊戲：**先宣告一個普通函式，再把「函式的名字」
傳進物件的建構子**。

!!! note "這是通往 OOP 的第一階"
    等你需要「每個角色各自記住自己的狀態」時，再升級成繼承 `GridObject` 的正規寫法
    （見下方最後一節「升級到繼承版」）。它不是要取代 [遊戲物件 (OOP)](game-objects.md)，
    而是讓你在學會 class 之前就能先玩起來。

## 最小範例

```cpp
#include "GridEasy.h"

// 一個普通函式：self 就是「這個物件自己」，引擎每幀會把它交給你。
void playerMove(GridObject* self) {
    if (IsKeyPressed(KEY_RIGHT) &&
        self->getX() < self->getEngine()->getCols() - 1) self->move(1, 0);
}

int main() {
    GridEngine game(10, 10, 40);
    game.spawn(new EasyObject("hero", 5, 5, playerMove));   // 把函式傳進去
    game.run();
}
```

比起繼承版，你不用寫 `class ... : public GridObject`、也不用 `override`——**會寫函式就能開始**。

## 建構子

```cpp
EasyObject(std::string asset, int x, int y,
           UpdateFn update, CollideFn collide = nullptr);
```

| 參數 | 說明 |
|---|---|
| `asset, x, y` | 跟 `GridObject` 一樣：素材名稱與起始格。 |
| `update` | 每幀要跑的函式，型別 `void(*)(GridObject* self)`。 |
| `collide` | 選填，同格碰撞時跑的函式 `void(*)(GridObject* self, GridObject* other)`；不填就不處理碰撞。 |

`EasyObject` 其餘功能都跟 `GridObject` 一樣（`getX` / `move` / `setTag` / `setTint`…），
差別只在「行為用傳進來的函式決定」，而不是覆寫虛擬函式。

## 兩個要記得的點

**1. 函式一定有一個 `self` 參數。** 普通函式沒有 `this`，所以引擎會把物件本身當 `self`
傳給你；要操作這個物件就寫 `self->move(...)`、`self->getX()`。這其實是日後 `this` 的
前身——`this` 就是編譯器自動幫你收的那個 `self`。

**2. 用 `getEngine()` 查地圖大小。** 想做邊界檢查時，透過
`self->getEngine()->getCols()` / `getRows()` 拿到格數。

## 天花板：函式沒有「每個物件自己的狀態」

這是 `EasyObject` 最重要的限制。普通函式無法替「每一個物件」各自記住資料
（例如計時器、目前方向）：

- 放函式裡的 `static` → **所有物件共用同一份**，會互相干擾。
- 放全域變數 → **同一種角色只有一個**時可行；有多個就會打架。

所以 `EasyObject` 適合這兩種情況：

- **同種角色只有一個**（一個玩家、一隻鬼）——它的狀態放全域變數就好。
- **根本不需要記狀態**的角色（像上面「按鍵才走」的玩家）。

完整示範見範例 `examples/pacman_easy/`：玩家 + 一隻鬼 + 豆子，狀態全部放全域。
其中「豆子被吃掉」用了一個小技巧——不記 `eaten` 旗標，而是把豆子 `setX(-1)`
移出畫面（等於消失、也不會再被吃），剛好避開了「每實例狀態」。

## 升級到繼承版

當你想要「很多個各自記狀態的角色」（例如四隻鬼各有自己的計時器與方向），
全域變數就不夠用了——**這正是需要類別的時機**：每個物件用自己的成員變數記狀態。

升級很直接：把 `playerMove(GridObject* self)` 搬進
`class Player : public GridObject` 的 `onUpdate()`，`self->` 換成直接呼叫即可。
完整版見 [Pac-Man 拆解](../tutorial/pacman.md)。
