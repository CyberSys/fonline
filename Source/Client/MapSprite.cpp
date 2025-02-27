//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "MapSprite.h"
#include "ModelSprites.h"
#include "SpriteManager.h"

void MapSprite::Invalidate()
{
    NO_STACK_TRACE_ENTRY();

    if (!Valid) {
        return;
    }
    Valid = false;

    if (ValidCallback != nullptr) {
        *ValidCallback = false;
        ValidCallback = nullptr;
    }

    if (ExtraChainRoot != nullptr) {
        *ExtraChainRoot = ExtraChainChild;
    }
    if (ExtraChainParent != nullptr) {
        ExtraChainParent->ExtraChainChild = ExtraChainChild;
    }
    if (ExtraChainChild != nullptr) {
        ExtraChainChild->ExtraChainParent = ExtraChainParent;
    }
    if (ExtraChainRoot != nullptr && ExtraChainChild != nullptr) {
        ExtraChainChild->ExtraChainRoot = ExtraChainRoot;
    }
    ExtraChainRoot = nullptr;
    ExtraChainParent = nullptr;
    ExtraChainChild = nullptr;

    if (ChainRoot != nullptr) {
        *ChainRoot = ChainChild;
    }
    if (ChainLast != nullptr) {
        *ChainLast = ChainParent;
    }
    if (ChainParent != nullptr) {
        ChainParent->ChainChild = ChainChild;
    }
    if (ChainChild != nullptr) {
        ChainChild->ChainParent = ChainParent;
    }
    if (ChainRoot != nullptr && ChainChild != nullptr) {
        ChainChild->ChainRoot = ChainRoot;
    }
    if (ChainLast != nullptr && ChainParent != nullptr) {
        ChainParent->ChainLast = ChainLast;
    }
    ChainRoot = nullptr;
    ChainLast = nullptr;
    ChainParent = nullptr;
    ChainChild = nullptr;

    if (MapSpr != nullptr) {
        MapSpr->Release();
        MapSpr = nullptr;
    }

    Root->_invalidatedSprites.push_back(this);

    Root = nullptr;
}

auto MapSprite::GetDrawRect() const -> IRect
{
    STACK_TRACE_ENTRY();

    const auto* spr = PSpr != nullptr ? *PSpr : Spr;
    RUNTIME_ASSERT(spr);

    auto x = ScrX - spr->Width / 2 + spr->OffsX + *PScrX;
    auto y = ScrY - spr->Height + spr->OffsY + *PScrY;
    if (OffsX != nullptr) {
        x += *OffsX;
    }
    if (OffsY != nullptr) {
        y += *OffsY;
    }

    return {x, y, x + spr->Width, y + spr->Height};
}

auto MapSprite::GetViewRect() const -> IRect
{
    STACK_TRACE_ENTRY();

    auto rect = GetDrawRect();

    const auto* spr = PSpr != nullptr ? *PSpr : Spr;
    RUNTIME_ASSERT(spr);

    if (auto&& view_rect = spr->GetViewSize(); view_rect.has_value()) {
        const auto view_width = view_rect->Left;
        const auto view_height = view_rect->Top;
        const auto view_ox = view_rect->Right;
        const auto view_oy = view_rect->Bottom;

        rect.Left = rect.CenterX() - view_width / 2 + view_ox;
        rect.Right = rect.Left + view_width;
        rect.Bottom = rect.Bottom + view_oy;
        rect.Top = rect.Bottom - view_height;
    }

    return rect;
}

auto MapSprite::CheckHit(int ox, int oy, bool check_transparent) const -> bool
{
    STACK_TRACE_ENTRY();

    if (ox < 0 || oy < 0) {
        return false;
    }

    return !check_transparent || Root->_sprMngr.SpriteHitTest(PSpr != nullptr ? *PSpr : Spr, ox, oy, true);
}

void MapSprite::SetEggAppearence(EggAppearenceType egg_appearence)
{
    STACK_TRACE_ENTRY();

    EggAppearence = egg_appearence;
}

void MapSprite::SetContour(ContourType contour)
{
    STACK_TRACE_ENTRY();

    Contour = contour;
}

void MapSprite::SetContour(ContourType contour, ucolor color)
{
    STACK_TRACE_ENTRY();

    Contour = contour;
    ContourColor = color;
}

void MapSprite::SetColor(ucolor color)
{
    STACK_TRACE_ENTRY();

    Color = color;
}

void MapSprite::SetAlpha(const uint8* alpha)
{
    STACK_TRACE_ENTRY();

    Alpha = alpha;
}

void MapSprite::SetFixedAlpha(uint8 alpha)
{
    STACK_TRACE_ENTRY();

    auto* color_alpha = reinterpret_cast<uint8*>(&Color) + 3;
    *color_alpha = alpha;
    Alpha = color_alpha;
}

void MapSprite::SetLight(CornerType corner, const ucolor* light, uint16 maxhx, uint16 maxhy)
{
    STACK_TRACE_ENTRY();

    if (HexX >= 1 && HexX < maxhx - 1 && HexY >= 1 && HexY < maxhy - 1) {
        Light = &light[HexY * maxhx + HexX];

        switch (corner) {
        case CornerType::EastWest:
        case CornerType::East:
            LightRight = Light - 1;
            LightLeft = Light + 1;
            break;
        case CornerType::NorthSouth:
        case CornerType::West:
            LightRight = Light + maxhx;
            LightLeft = Light - maxhx;
            break;
        case CornerType::South:
            LightRight = Light - 1;
            LightLeft = Light - maxhx;
            break;
        case CornerType::North:
            LightRight = Light + maxhx;
            LightLeft = Light + 1;
            break;
        }
    }
    else {
        Light = nullptr;
        LightRight = nullptr;
        LightLeft = nullptr;
    }
}

MapSpriteList::MapSpriteList(SpriteManager& spr_mngr) :
    _sprMngr {spr_mngr}
{
    STACK_TRACE_ENTRY();
}

MapSpriteList::~MapSpriteList()
{
    STACK_TRACE_ENTRY();

    Invalidate();

    for (const auto* spr : _invalidatedSprites) {
        delete spr;
    }
    for (const auto* spr : _spritesPool) {
        delete spr;
    }
}

void MapSpriteList::GrowPool()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _spritesPool.reserve(_spritesPool.size() + SPRITES_POOL_GROW_SIZE);

    for (uint i = 0; i < SPRITES_POOL_GROW_SIZE; i++) {
        _spritesPool.push_back(new MapSprite());
    }
}

auto MapSpriteList::RootSprite() -> MapSprite*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _rootSprite;
}

auto MapSpriteList::PutSprite(MapSprite* child, DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, const int* sx, const int* sy, const Sprite* spr, const Sprite* const* pspr, const int* ox, const int* oy, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&
{
    STACK_TRACE_ENTRY();

    _spriteCount++;

    MapSprite* mspr;
    if (!_invalidatedSprites.empty()) {
        mspr = _invalidatedSprites.back();
        _invalidatedSprites.pop_back();
    }
    else {
        if (_spritesPool.empty()) {
            GrowPool();
        }

        mspr = _spritesPool.back();
        _spritesPool.pop_back();
    }

    mspr->Root = this;

    if (child == nullptr) {
        if (_lastSprite == nullptr) {
            _rootSprite = mspr;
            _lastSprite = mspr;
            mspr->ChainRoot = &_rootSprite;
            mspr->ChainLast = &_lastSprite;
            mspr->ChainParent = nullptr;
            mspr->ChainChild = nullptr;
            mspr->TreeIndex = 0;
        }
        else {
            mspr->ChainParent = _lastSprite;
            mspr->ChainChild = nullptr;
            _lastSprite->ChainChild = mspr;
            _lastSprite->ChainLast = nullptr;
            mspr->ChainLast = &_lastSprite;
            mspr->TreeIndex = _lastSprite->TreeIndex + 1;
            _lastSprite = mspr;
        }
    }
    else {
        mspr->ChainChild = child;
        mspr->ChainParent = child->ChainParent;
        child->ChainParent = mspr;
        if (mspr->ChainParent != nullptr) {
            mspr->ChainParent->ChainChild = mspr;
        }

        // Recalculate indices
        auto index = mspr->ChainParent != nullptr ? mspr->ChainParent->TreeIndex + 1 : 0;
        auto* mspr_ = mspr;
        while (mspr_ != nullptr) {
            mspr_->TreeIndex = index;
            mspr_ = mspr_->ChainChild;
            index++;
        }

        if (mspr->ChainParent == nullptr) {
            RUNTIME_ASSERT(child->ChainRoot);
            _rootSprite = mspr;
            mspr->ChainRoot = &_rootSprite;
            child->ChainRoot = nullptr;
        }
    }

    mspr->HexX = hx;
    mspr->HexY = hy;
    mspr->ScrX = x;
    mspr->ScrY = y;
    mspr->PScrX = sx;
    mspr->PScrY = sy;
    mspr->Spr = spr;
    mspr->PSpr = pspr;
    mspr->OffsX = ox;
    mspr->OffsY = oy;
    mspr->Alpha = alpha;
    mspr->Light = nullptr;
    mspr->LightRight = nullptr;
    mspr->LightLeft = nullptr;
    mspr->Valid = true;
    mspr->ValidCallback = callback;
    if (callback != nullptr) {
        *callback = true;
    }
    mspr->EggAppearence = EggAppearenceType::None;
    mspr->Contour = ContourType::None;
    mspr->ContourColor = ucolor::clear;
    mspr->Color = ucolor::clear;
    mspr->DrawEffect = effect;

    // Draw order
    mspr->DrawOrder = draw_order;

    if (draw_order < DrawOrderType::NormalBegin || draw_order > DrawOrderType::NormalEnd) {
        mspr->DrawOrderPos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(draw_order) + mspr->HexY * MAXHEX_MAX + mspr->HexX;
    }
    else {
        mspr->DrawOrderPos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(DrawOrderType::NormalBegin) + mspr->HexY * static_cast<int>(DrawOrderType::NormalBegin) * MAXHEX_MAX + mspr->HexX * static_cast<int>(DrawOrderType::NormalBegin) + (static_cast<int>(draw_order) - static_cast<int>(DrawOrderType::NormalBegin));
    }

    return *mspr;
}

auto MapSpriteList::AddSprite(DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, const int* sx, const int* sy, const Sprite* spr, const Sprite* const* pspr, const int* ox, const int* oy, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&
{
    STACK_TRACE_ENTRY();

    return PutSprite(nullptr, draw_order, hx, hy, x, y, sx, sy, spr, pspr, ox, oy, alpha, effect, callback);
}

auto MapSpriteList::InsertSprite(DrawOrderType draw_order, uint16 hx, uint16 hy, int x, int y, const int* sx, const int* sy, const Sprite* spr, const Sprite* const* pspr, const int* ox, const int* oy, const uint8* alpha, RenderEffect** effect, bool* callback) -> MapSprite&
{
    STACK_TRACE_ENTRY();

    // Find place
    uint pos;
    if (draw_order < DrawOrderType::NormalBegin || draw_order > DrawOrderType::NormalEnd) {
        pos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(draw_order) + hy * MAXHEX_MAX + hx;
    }
    else {
        pos = MAXHEX_MAX * MAXHEX_MAX * static_cast<int>(DrawOrderType::NormalBegin) + hy * static_cast<int>(DrawOrderType::NormalBegin) * MAXHEX_MAX + hx * static_cast<int>(DrawOrderType::NormalBegin) + (static_cast<int>(draw_order) - static_cast<int>(DrawOrderType::NormalBegin));
    }

    auto* parent = _rootSprite;
    while (parent != nullptr) {
        if (!parent->Valid) {
            continue;
        }
        if (pos < parent->DrawOrderPos) {
            break;
        }
        parent = parent->ChainChild;
    }

    return PutSprite(parent, draw_order, hx, hy, x, y, sx, sy, spr, pspr, ox, oy, alpha, effect, callback);
}

void MapSpriteList::Invalidate()
{
    STACK_TRACE_ENTRY();

    while (_rootSprite != nullptr) {
        _rootSprite->Invalidate();
    }

    _lastSprite = nullptr;
    _spriteCount = 0;
}

void MapSpriteList::Sort()
{
    STACK_TRACE_ENTRY();

    if (_rootSprite == nullptr) {
        return;
    }

    _sortSprites.reserve(_spriteCount);

    auto* mspr = _rootSprite;
    while (mspr != nullptr) {
        _sortSprites.emplace_back(mspr);
        mspr = mspr->ChainChild;
    }

    std::sort(_sortSprites.begin(), _sortSprites.end(), [](const MapSprite* mspr1, const MapSprite* mspr2) {
        if (mspr1->DrawOrderPos == mspr2->DrawOrderPos) {
            return mspr1->TreeIndex < mspr2->TreeIndex;
        }
        return mspr1->DrawOrderPos < mspr2->DrawOrderPos;
    });

    _rootSprite = _sortSprites.front();
    _lastSprite = _sortSprites.back();

    for (size_t i = 1; i < _sortSprites.size(); i++) {
        _sortSprites[i]->TreeIndex = i;
        _sortSprites[i - 1]->ChainChild = _sortSprites[i];
        _sortSprites[i]->ChainParent = _sortSprites[i - 1];
        _sortSprites[i]->ChainRoot = nullptr;
        _sortSprites[i]->ChainLast = nullptr;
    }

    _rootSprite->TreeIndex = 0;
    _rootSprite->ChainParent = nullptr;
    _rootSprite->ChainRoot = &_rootSprite;
    _rootSprite->ChainLast = nullptr;

    _lastSprite->ChainChild = nullptr;
    _lastSprite->ChainLast = &_lastSprite;

    _sortSprites.clear();
}
