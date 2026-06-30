# UI 覆蓋層

遊戲世界是網格，但分數、訊息、按鈕這些東西不該被綁在格子上。Grid++ 提供一個
**UI 覆蓋層**：跳脫網格、用**像素座標**自由繪製，並且畫在所有網格物件「之上」。

- 基底類別 `UIElement` 在核心 `GridPlusPlus.h`。
- 現成元件 `Label`、`Button` 在選用模組 `GridUI.h`（`#include "GridUI.h"`）。

## 加入 UI

和 `spawn(GridObject*)` 對稱，用 `addUI(UIElement*)` 把 UI 放進覆蓋層：

```cpp
game.addUI(new ScoreUI());
game.addUI(new MyButton(10, 10, 80, 30));
```

引擎每幀會對每個 UI 呼叫 `onUpdate()`（互動）與 `draw()`（繪製），`draw()` 在網格畫完後才執行，所以一定蓋在最上面。

## 自訂 UI：繼承 UIElement

需要完全自訂的畫面（例如會變動的分數），就繼承 `UIElement`、覆寫 `draw()`：

```cpp
class ScoreUI : public UIElement {
public:
    void draw() override {
        DrawText(TextFormat("Score: %d", g_score), 8, 8, 20, YELLOW);
    }
};
```

`draw()` 裡用的是 raylib 的像素繪圖函式（`DrawText`、`DrawRectangle`…），座標是像素、原點在左上角。視窗大小可用 `GetScreenWidth()` / `GetScreenHeight()` 查詢（方便置中）。

## 現成元件：Label

純文字標籤。

```cpp
#include "GridUI.h"

game.addUI(new Label("方向鍵移動", 8, 8, 20, WHITE));
```

| 建構子 | 說明 |
|---|---|
| `Label(text, x, y, fontSize = 20, color = BLACK)` | 在 (x, y) 畫一行文字。 |
| `setText(t)` | 更新文字內容（例如即時分數）。 |

## 現成元件：Button

一個可點的矩形按鈕。**覆寫 `onClick()`** 決定被點擊時要做什麼：

```cpp
class StartButton : public Button {
public:
    StartButton(int x, int y, int w, int h) : Button("Start", x, y, w, h) {}
    void onClick() override {
        g_state = 1;        // 例如：開始遊戲
    }
};

game.addUI(new StartButton(260, 300, 100, 40));
```

!!! tip "讓按鈕只在某個畫面出現"
    若想讓按鈕只在特定階段顯示與作用（例如「開始」只在開始畫面、「重新開始」只在結束畫面），
    可以覆寫 `onUpdate()` 與 `draw()`，在裡面判斷遊戲狀態，符合時才呼叫基底版本：

    ```cpp
    void onUpdate() override { if (g_state == 0) Button::onUpdate(); }
    void draw()     override { if (g_state == 0) Button::draw(); }
    ```

| 成員 | 說明 |
|---|---|
| `Button(text, x, y, w, h)` | 在像素矩形 (x, y, w, h) 畫一個有文字的按鈕。 |
| `onClick()` | **覆寫它**：按鈕被滑鼠左鍵點擊時呼叫。 |

按鈕會自動偵測滑鼠是否在它上面（hover 時變色），點下去就呼叫 `onClick()`——這些都由
`Button` 的 `onUpdate()` 處理好了，你只要管 `onClick()` 裡要做什麼。

!!! tip "Pac-Man 範例怎麼用"
    範例把分數與「YOU WIN / GAME OVER / PAUSED」訊息放在一個 `ScoreUI : UIElement`，
    右上角放一個 `PauseButton : Button`（`onClick()` 切換暫停）。這樣 UI 就和遊戲角色
    完全分開，不再塞在某個角色的繪製裡。
