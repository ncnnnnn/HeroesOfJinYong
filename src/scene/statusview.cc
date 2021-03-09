/*
 * Heroes of Jin Yong.
 * A reimplementation of the DOS game `The legend of Jin Yong Heroes`.
 * Copyright (C) 2021, Soar Qin<soarchin@gmail.com>

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "statusview.hh"

#include "window.hh"
#include "mem/savedata.hh"
#include "mem/action.hh"
#include "core/config.hh"
#include "util/conv.hh"
#include <fmt/format.h>

namespace hojy::scene {

void StatusView::show(std::int16_t charId) {
    charId_ = charId;
}

void StatusView::handleKeyInput(Node::Key key) {
    switch (key) {
    case KeyOK: case KeySpace: case KeyCancel:
        delete this;
        break;
    default:
        break;
    }
}

void StatusView::makeCache() {
    auto *ttf = renderer_->ttf();
    auto fontSize = ttf->fontSize();
    auto lineheight = fontSize + TextLineSpacing;
    int x0 = SubWindowBorder;
    int x1 = x0 + fontSize * 5 / 2;
    int x2 = x1 + fontSize * 5 + SubWindowBorder;
    int x3 = x2 + fontSize * 9 / 2;
    int w = x3 + fontSize * 2 + SubWindowBorder;
    bool showPotential = core::config.showPotential();
    int lines = showPotential ? 12 : 11;
    int h = SubWindowBorder * 2 + lineheight * lines - TextLineSpacing;
    width_ = w;
    height_ = h;

    cacheBegin();
    renderer_->fillRoundedRect(0, 0, w, h, RoundedRectRad, 64, 64, 64, 208);
    renderer_->drawRoundedRect(0, 0, w, h, RoundedRectRad, 224, 224, 224, 255);
    int y = SubWindowBorder;
    const auto *headTex = gWindow->headTexture(charId_);
    if (headTex) {
        int hx = x2 / 2 - headTex->width();
        int hy = lineheight * 4 - headTex->height() * 2;
        renderer_->renderTexture(headTex, float(hx), float(hy), 2.f, true);
    }
    const auto *charInfo = mem::gSaveData.charInfo[charId_];
    ttf->render(L"攻擊力", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->attack), x3, y, true);
    y += lineheight;
    ttf->render(L"防禦力", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->defence), x3, y, true);
    y += lineheight;
    ttf->render(L"輕功", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->speed), x3, y, true);
    y += lineheight;
    ttf->render(L"醫療能力", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->medic), x3, y, true);
    y += lineheight;
    auto name = util::big5Conv.toUnicode(charInfo->name);
    ttf->render(name, (x2 - ttf->stringWidth(name)) / 2, y, true);
    ttf->render(L"用毒能力", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->poison), x3, y, true);
    y += lineheight;
    ttf->render(L"等級", x0, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->level), x1, y, true);
    ttf->render(L"解毒能力", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->depoison), x3, y, true);
    y += lineheight;
    ttf->render(L"生命", x0, y, true);
    ttf->render(fmt::format(L"{:>3}/{:>3}", charInfo->hp, charInfo->maxHp), x1, y, true);
    ttf->render(L"拳掌功夫", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->fist), x3, y, true);
    y += lineheight;
    ttf->render(L"內力", x0, y, true);
    ttf->render(fmt::format(L"{:>3}/{:>3}", charInfo->mp, charInfo->maxHp), x1, y, true);
    ttf->render(L"御劍能力", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->sword), x3, y, true);
    y += lineheight;
    ttf->render(L"體力", x0, y, true);
    ttf->render(fmt::format(L"{:>3}/{:>3}", charInfo->stamina, mem::StaminaMax), x1, y, true);
    ttf->render(L"耍刀技巧", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->blade), x3, y, true);
    y += lineheight;
    ttf->render(L"經驗", x0, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->exp), x1, y, true);
    ttf->render(L"特殊兵器", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->special), x3, y, true);
    y += lineheight;
    ttf->render(L"升級", x0, y, true);
    auto exp = mem::getExpForLevelUp(charInfo->level);
    if (exp) {
        ttf->render(fmt::format(L"{:>3}", exp), x1, y, true);
    } else {
        ttf->render(L"-", x1, y, true);
    }
    ttf->render(L"暗器技巧", x2, y, true);
    ttf->render(fmt::format(L"{:>3}", charInfo->throwing), x3, y, true);
    if (showPotential) {
        y += lineheight;
        ttf->render(L"資質", x0, y, true);
        ttf->render(fmt::format(L"{:>3}", charInfo->potential), x1, y, true);
    }
    cacheEnd();
}

}