# 素材包

Grid++ 的圖片素材放在一個 SQLite 檔（例如 `assets.db`）裡。引擎內建的迷你讀取器
會把素材逐筆做成 raylib 材質（`Texture2D`）。**你不需要安裝或連結 SQLite。**
讀取器的用途與限制見 [迷你 SQLite 讀取器](../internals/sqlite-reader.md)。

## sprites 資料表

`assets.db` 裡有一張 `sprites` 表，欄位如下：

| 欄位 | 型別 | 說明 |
|---|---|---|
| `id` | INTEGER (PK) | 主鍵 |
| `name` | TEXT | 素材呼叫名稱（**允許重複**，見下方注意） |
| `tags` | TEXT | 逗號分隔的標籤，目前僅作分類參考 |
| `image_data` | BLOB | 固定為 32×32 像素的 RGBA 原始資料（4096 bytes） |

程式裡用名稱取用素材，例如 `GridObject("pacman", x, y)` 就會去找名為 `pacman` 的素材。

## 名稱重複的報錯機制

`name` 在資料庫中允許重複，但這通常是設定錯誤。Grid++ 的處理方式：

- **載入時**：若偵測到重複名稱，會在主控台印出警告。
- **取用時**：若你呼叫的名稱對應到多筆素材，引擎會**直接丟出例外**，
  訊息會告訴你哪個名稱重複了——因為它無法替你決定要用哪一張。

!!! warning "保持名稱唯一"
    請讓每個素材的 `name` 都唯一。重複名稱在取用當下才報錯，容易讓人摸不著頭緒。

## 產生 / 修改素材包

範例素材包在 `examples/pacman/pacman.db`。要建立自己的素材包，可以使用 Python
標準庫的 `sqlite3`；每張圖必須是 **32×32 的 RGBA**，攤平成 4096 bytes 後
`INSERT` 進 `sprites` 表：

```python
import sqlite3
db = sqlite3.connect("assets.db")
db.execute("""CREATE TABLE IF NOT EXISTS sprites (
    id INTEGER PRIMARY KEY,
    name TEXT,
    tags TEXT,
    image_data BLOB
)""")
# image_bytes 必須剛好 4096 bytes（32*32*4，順序為 R,G,B,A）
db.execute("INSERT INTO sprites (name, tags, image_data) VALUES (?, ?, ?)",
           ("myhero", "player", image_bytes))
db.commit()
db.close()
```

!!! note "為什麼是 4096 bytes"
    32 寬 × 32 高 × 每像素 4 個位元組（紅綠藍透明）= 4096。
    不是這個大小的素材會在載入時被略過並印出警告。
