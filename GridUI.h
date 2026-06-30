// =============================================================================
//  GridUI.h —— Grid++ 的 UI 元件模組（選用）
//  需要現成 UI 元件時才 #include "GridUI.h"（會自動帶進核心 GridPlusPlus.h）。
//  提供 Label（文字）與 Button（可點按鈕）。用法詳見 docs/guide/ui.md。
//
//  兩者都是 UIElement 的子類，用像素座標繪製，畫在所有網格物件之上。
// =============================================================================
#ifndef GRIDUI_H
#define GRIDUI_H

#include "GridPlusPlus.h"
#include <string>

// 文字標籤：在 (x, y) 畫一行文字。
class Label : public UIElement {
public:
    Label(const std::string& text, int x, int y, int fontSize = 20, Color color = BLACK)
        : text(text), x(x), y(y), fontSize(fontSize), color(color) {}

    void setText(const std::string& t) { text = t; }   // 之後可更新文字（例如分數）

    void draw() override {
        DrawText(text.c_str(), x, y, fontSize, color);
    }

protected:
    std::string text;
    int x, y, fontSize;
    Color color;
};

// 可點按鈕：一個矩形 + 置中文字。覆寫 onClick() 決定被點擊時做什麼。
class Button : public UIElement {
public:
    Button(const std::string& text, int x, int y, int w, int h)
        : text(text), x(x), y(y), w(w), h(h) {}

    // 學生覆寫這個：按鈕被點擊時呼叫。
    virtual void onClick() {}

    void onUpdate() override {
        Vector2 m = GetMousePosition();
        hover = (m.x >= x && m.x <= x + w && m.y >= y && m.y <= y + h);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) onClick();
    }

    void draw() override {
        DrawRectangle(x, y, w, h, hover ? SKYBLUE : LIGHTGRAY);
        DrawRectangleLines(x, y, w, h, DARKGRAY);
        int fs = 20, tw = MeasureText(text.c_str(), fs);
        DrawText(text.c_str(), x + (w - tw) / 2, y + (h - fs) / 2, fs, BLACK);
    }

protected:
    std::string text;
    int x, y, w, h;
    bool hover = false;
};

#endif // GRIDUI_H
