# API 速查

涵蓋學生會用到的公開介面。內部命名空間 `gridpp_detail`（SQLite 讀取器）不在此列，
詳見 [引擎內部原理](../internals/sqlite-reader.md)。

## GridEngine

主引擎。定義於 `GridPlusPlus.h`。

```cpp
GridEngine(int cols, int rows, int gridSize = 32);
void loadAssets(const std::string& dbPath);
void spawn(GridObject* obj);
void run();
void addUI(UIElement* e);
void clearObjects();
void  setBackgroundColor(Color c);   // 背景色，預設 RAYWHITE
Color getBackgroundColor() const;
void setShowGrid(bool show);         // 網格線開關，預設關閉
bool getShowGrid() const;
int  getCols() const;
int  getRows() const;
int  getGridSize() const;
void drawCell(const std::string& asset, int gx, int gy, int direction = 0, Color tint = WHITE);
```

| 成員 | 說明 |
|---|---|
| `GridEngine(cols, rows, gridSize)` | 建立引擎並開視窗。 |
| `loadAssets(path)` | 載入 `assets.db`。需在建立引擎後呼叫。 |
| `spawn(obj)` | 放入物件並呼叫其 `onStart()`。 |
| `addUI(e)` | 把 UI 元件加入覆蓋層（畫在網格之上）。 |
| `clearObjects()` | 刪除並清空所有遊戲物件（用於重新開始；不影響 UI）。 |
| `run()` | 進入主循環直到視窗關閉。 |
| `setBackgroundColor/getBackgroundColor` | 背景顏色（每幀清畫面用），預設 `RAYWHITE`。 |
| `setShowGrid/getShowGrid` | 網格線開關，預設關閉，需要時傳 `true` 打開。 |
| `getCols/getRows/getGridSize()` | 查詢地圖大小與格子像素。 |
| `drawCell(asset, gx, gy)` | 在某格畫素材（給 `render()` 用）。 |

## GridObject

所有遊戲物件的基底類別。定義於 `GridPlusPlus.h`。

```cpp
GridObject();
GridObject(std::string assetName, int x, int y);

virtual void onStart();
virtual void onUpdate();
virtual void onCollide(GridObject* other);
virtual void render(GridEngine* engine);

int  getX() const;  int  getY() const;
void setX(int x);   void setY(int y);
void move(int dx, int dy);

std::string getAssetName() const;
std::string getTag() const;
void        setTag(const std::string& t);

int   getDirection() const;          // 朝向 0~3
void  setDirection(int dir);         // 0~3，逆時針每 +1 轉 90°
Color getTint() const;
void  setTint(Color c);              // 調色，預設 WHITE

GridEngine* getEngine() const;       // 取得所屬引擎（spawn 後才有效）
```

| 成員 | 說明 |
|---|---|
| 建構子（兩個） | 空的 / 指定素材與座標（重載示範）。 |
| `onStart()` | 被 `spawn` 時呼叫一次。覆寫用。 |
| `onUpdate()` | 每幀呼叫。覆寫用。 |
| `onCollide(other)` | 同格碰撞時呼叫。覆寫用。 |
| `render(engine)` | 畫自己；預設畫 `assetName`。可覆寫。 |
| `getX/getY/setX/setY/move` | 存取格子座標。 |
| `getTag/setTag` | 身分標記，碰撞時分辨對象。 |
| `getDirection/setDirection` | 朝向 0~3，繪製時旋轉素材（重複利用同一張圖）。 |
| `getTint/setTint` | 調色，把素材染成不同顏色（diffuse color）。 |
| `getEngine()` | 取得所屬引擎（`spawn` 後才有效），例如查地圖大小做邊界檢查。 |

## EasyObject

入門版物件。定義於選用模組 `GridEasy.h`（繼承 `GridObject`）。用「傳進來的函式」決定行為，
不必先學繼承與覆寫。概念與限制見 [入門版物件 (EasyObject)](../guide/easy-object.md)。

```cpp
using UpdateFn  = void(*)(GridObject* self);                 // 每幀要跑的函式
using CollideFn = void(*)(GridObject* self, GridObject* other); // 同格碰撞時要跑的函式

EasyObject(std::string asset, int x, int y,
           UpdateFn update, CollideFn collide = nullptr);
```

| 成員 | 說明 |
|---|---|
| `EasyObject(asset, x, y, update, collide)` | 建立物件；`update` 每幀呼叫，`collide` 選填（同格碰撞時呼叫）。 |
| 其餘 | 與 `GridObject` 相同（`getX/move/setTag/setTint/getEngine`…）。 |

!!! note "限制"
    普通函式沒有「每個物件自己的狀態」。只適合**同種角色只有一個**或**不需記狀態**的情況；
    需要多個各自記狀態的角色時，請改成繼承 `GridObject`。

## GridMaze

選用的迷宮模組。定義於 `GridMaze.h`（繼承 `GridObject`）。

```cpp
GridMaze(int cols, int rows);
void setWall(int x, int y, bool wall);

void setWallAsset(const std::string& asset);
void setWallTiles(const std::string& iso, const std::string& end,
                  const std::string& straight, const std::string& corner,
                  const std::string& tee, const std::string& cross);
bool isWall(int x, int y) const;
int  getWidth() const;
int  getHeight() const;
```

| 成員 | 說明 |
|---|---|
| `GridMaze(cols, rows)` | 建立指定大小的空迷宮（全是空地）。 |
| `setWall(x, y, wall)` | 設定某格是不是牆。 |
| `setWallAsset(name)` | 所有牆同一張圖。 |
| `setWallTiles(...)` | 6 種基本形狀，靠旋轉自動拼接。 |
| `setWallAsset(mask, name)` | 依連通情況選圖（上1/右2/下4/左8）。 |
| `isWall(x, y)` | 那格是否為牆（界外當牆）。 |
| `getWidth/getHeight` | 迷宮欄數 / 列數。 |

## UIElement

UI 覆蓋層的基底類別。定義於 `GridPlusPlus.h`。

```cpp
virtual void onUpdate();   // 每幀呼叫（互動用）
virtual void draw();       // 每幀呼叫，用像素座標繪製，畫在網格之上
```

繼承它、覆寫 `draw()`（與選擇性的 `onUpdate()`），再用 `engine.addUI(...)` 加入。

## Label / Button

現成 UI 元件，定義於 `GridUI.h`（皆繼承 `UIElement`）。

```cpp
// 文字標籤
Label(const std::string& text, int x, int y, int fontSize = 20, Color color = BLACK);
void setText(const std::string& t);

// 可點按鈕（覆寫 onClick 決定行為）
Button(const std::string& text, int x, int y, int w, int h);
virtual void onClick();
```

| 成員 | 說明 |
|---|---|
| `Label(text, x, y, …)` | 在 (x, y) 畫一行文字。 |
| `Label::setText(t)` | 更新文字內容。 |
| `Button(text, x, y, w, h)` | 在像素矩形畫一個按鈕。 |
| `Button::onClick()` | 覆寫它：被滑鼠左鍵點擊時呼叫。 |

## 常用的 raylib 函式

這些來自 raylib，寫遊戲時很常用：

| 函式 | 說明 |
|---|---|
| `IsKeyPressed(KEY_*)` | 該鍵「剛按下的那一幀」回傳 true。 |
| `IsKeyDown(KEY_*)` | 該鍵「持續被按住」回傳 true。 |
| `GetRandomValue(min, max)` | 取一個範圍內的隨機整數。 |
| `DrawText(text, x, y, size, color)` | 在畫面上畫文字（像素座標）。 |
