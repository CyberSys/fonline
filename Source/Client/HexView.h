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

#pragma once

#include "Common.h"

#include "MapSprite.h"

class MapView;
class FOEngineBase;

class HexView
{
public:
    HexView() = delete;
    explicit HexView(MapView* map);
    HexView(const HexView&) = delete;
    HexView(HexView&&) noexcept = delete;
    auto operator=(const HexView&) = delete;
    auto operator=(HexView&&) noexcept = delete;
    virtual ~HexView() = default;

    [[nodiscard]] auto GetMap() -> MapView* { return _map; }
    [[nodiscard]] auto GetMap() const -> const MapView* { return _map; }
    [[nodiscard]] auto IsVisible() const -> bool { return Visible; }
    [[nodiscard]] auto IsSpriteValid() const -> bool { return _mapSprValid; }
    [[nodiscard]] auto GetSprite() const -> const MapSprite*;
    [[nodiscard]] auto GetSprite() -> MapSprite*;
    [[nodiscard]] auto IsTransparent() const -> bool { return _maxAlpha < 0xFF; }
    [[nodiscard]] auto IsFullyTransparent() const -> bool { return _maxAlpha == 0; }
    [[nodiscard]] auto IsFading() const -> bool { return _fading; }
    [[nodiscard]] auto IsFinishing() const -> bool { return _finishing; }
    [[nodiscard]] auto IsFinished() const -> bool;

    auto AddSprite(MapSpriteList& list, DrawOrderType draw_order, uint16 hx, uint16 hy, const int* sx, const int* sy) -> MapSprite*;
    auto InsertSprite(MapSpriteList& list, DrawOrderType draw_order, uint16 hx, uint16 hy, const int* sx, const int* sy) -> MapSprite*;
    void Finish();
    auto StoreFading() -> tuple<bool, bool, time_point> { return {_fading, _fadeUp, _fadingTime}; }
    void RestoreFading(const tuple<bool, bool, time_point>& data) { std::tie(_fading, _fadeUp, _fadingTime) = data; }
    void FadeUp();
    void RestoreAlpha();
    void SetMaxAlpha(uint8 alpha);
    void RefreshSprite();
    void InvalidateSprite();

    // Todo: incapsulate hex view fileds
    bool Visible {true};
    const Sprite* Spr {};
    int ScrX {};
    int ScrY {};
    uint8 Alpha {0xFF};
    RenderEffect* DrawEffect {};

protected:
    MapView* _map;

    virtual void SetupSprite(MapSprite* mspr);
    void ProcessFading();

private:
    void SetFade(bool fade_up);
    auto EvaluateFadeAlpha() -> uint8;

    MapSprite* _mapSpr {};
    bool _mapSprValid {};

    uint8 _maxAlpha {0xFF};

    bool _fading {};
    bool _fadeUp {};
    time_point _fadingTime {};

    bool _finishing {};
    time_point _finishingTime {};
};
