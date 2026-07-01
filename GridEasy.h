// =============================================================================
//  GridEasy.h —— Grid++ 的「入門版物件」模組（選用）
//  還沒學到繼承（class / override）之前，用它就能做出會動的角色：
//  先宣告一個普通函式，再把「函式名字」傳進 EasyObject 的建構子即可。
//  需要時才 #include "GridEasy.h"（會自動帶進核心 GridPlusPlus.h）。
//
//  範例：
//    void playerMove(GridObject* self) {          // 一個普通函式，self 就是這個物件
//        if (IsKeyPressed(KEY_RIGHT) &&
//            self->getX() < self->getEngine()->getCols() - 1) self->move(1, 0);
//    }
//    game.spawn(new EasyObject("hero", 5, 5, playerMove));   // 初始化時把函式傳進去
//
//  限制：函式沒有「每個物件自己的狀態」。同一種角色只有一個時，狀態可放全域變數；
//        需要多個各自記狀態的角色（計時器、方向…）時，請改成繼承 GridObject。
// =============================================================================
#ifndef GRIDEASY_H
#define GRIDEASY_H

#include "GridPlusPlus.h"

// 入門版物件：把行為交給「傳進來的函式」決定，不必先學繼承與覆寫。
class EasyObject : public GridObject {
public:
    // 幫「函數指標」型別取個好讀的名字（學生寫函式時不必用到這兩個名字）：
    //   UpdateFn  —— 每幀要跑的函式，引擎會把物件本身當 self 傳進來。
    //   CollideFn —— 同格碰撞時要跑的函式，self 是自己、other 是撞到的對象。
    using UpdateFn  = void(*)(GridObject* self);
    using CollideFn = void(*)(GridObject* self, GridObject* other);

    // asset / x / y 跟 GridObject 一樣；update 是每幀要跑的函式；
    // collide 選填，預設 nullptr（不處理碰撞）。
    EasyObject(std::string asset, int x, int y,
               UpdateFn update, CollideFn collide = nullptr)
        : GridObject(asset, x, y), updateFn(update), collideFn(collide) {}

    // 引擎每幀呼叫 → 轉呼叫你傳進來的函式（沒給就不做事）。
    void onUpdate() override               { if (updateFn)  updateFn(this); }
    void onCollide(GridObject* other) override { if (collideFn) collideFn(this, other); }

private:
    UpdateFn  updateFn;
    CollideFn collideFn;
};

#endif // GRIDEASY_H
