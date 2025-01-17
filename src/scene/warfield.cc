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

#include "warfield.hh"

#include "colorpalette.hh"
#include "menu.hh"
#include "charlistmenu.hh"
#include "statusview.hh"
#include "itemview.hh"
#include "window.hh"
#include "effect.hh"
#include "data/grpdata.hh"
#include "data/warfielddata.hh"
#include "mem/savedata.hh"
#include "mem/strings.hh"
#include "core/config.hh"
#include "util/random.hh"
#include <fmt/format.h>
#include <map>

namespace hojy::scene {

Warfield::Warfield(Renderer *renderer, int x, int y, int width, int height, std::pair<int, int> scale):
    Map(renderer, x, y, width, height, scale),
    drawingTerrainTex2_(Texture::create(renderer, auxWidth_, auxHeight_)) {
    drawingTerrainTex2_->enableBlendMode(true);
    fightTexData_.resize(FightTextureListCount);
    for (size_t i = 0; i < FightTextureListCount; ++i) {
        data::GrpData::loadData(fmt::format("FIGHT{:03}.IDX", i), fmt::format("FIGHT{:03}.GRP", i), fightTexData_[i]);
    }
}

Warfield::~Warfield() {
    delete drawingTerrainTex2_;
    delete statusPanel_;
}

void Warfield::cleanup() {
    chars_.clear();
    charQueue_.clear();
    stage_ = Idle;
    knowledge_[0] = knowledge_[1] = 0;
    cursorX_ = 0;
    cursorY_ = 0;
    autoControl_ = false;
    skillLevelup_ = false;
    selCells_.clear();
    movingPath_.clear();
    actIndex_ = -1;
    actId_ = -1;
    actLevel_ = 0;
    effectId_ = -1;
    effectTexIdx_ = -1;
    fightTexIdx_ = -1;
    fightTexCount_ = 0;
    fightFrame_ = 0;
    attackTimesLeft_ = 0;
    fightTex_ = nullptr;
    popupNumbers_.clear();
}

bool Warfield::load(std::int16_t warId) {
    cleanup();

    warId_ = warId;
    const auto *info = data::gWarfieldData.info(warId);
    auto warMapId = info->warFieldId;
    const auto &layers = data::gWarfieldData.layers(warMapId)->layers;
    if (warMapLoaded_.find(warMapId) == warMapLoaded_.end()) {
        mapWidth_ = data::WarFieldWidth;
        mapHeight_ = data::WarFieldHeight;
        if (data::GrpData::loadData("WDX", "WMP", texData_)) {
            for (std::int16_t i = 0; i < 1000; ++i) {
                warMapLoaded_.insert(i);
            }
        } else {
            if (!data::GrpData::loadData(fmt::format("WDX{:03}", warMapId), fmt::format("WMP{:03}", warMapId), texData_)) {
                return false;
            }
            warMapLoaded_.insert(warMapId);
        }
    }
    {
        const auto *arr = reinterpret_cast<const uint16_t*>(texData_[0].data());
        cellWidth_ = arr[0];
        cellHeight_ = arr[1];
        offsetX_ = arr[2];
        offsetY_ = arr[3];
    }
    int cellDiffX = cellWidth_ / 2;
    int cellDiffY = cellHeight_ / 2;
    auto size = mapWidth_ * mapHeight_;
    cellInfo_.clear();
    cellInfo_.resize(size);

    int x = (mapHeight_ - 1) * cellDiffX + offsetX_;
    int y = offsetY_;
    int pos = 0;
    for (int j = mapHeight_; j; --j) {
        int tx = x, ty = y;
        for (int i = mapWidth_; i; --i, ++pos, tx += cellDiffX, ty += cellDiffY) {
            auto &ci = cellInfo_[pos];
            auto texId = layers[0][pos] >> 1;
            ci.earthId = texId;
            ci.buildingId = layers[1][pos] >> 1;
            ci.blocked = ci.buildingId > 0 || texId >= 179 && texId <= 181 || texId == 261 || texId == 511 || texId >= 662 && texId <= 665 || texId == 674;
        }
        x -= cellDiffX; y += cellDiffY;
    }

    subMapId_ = warMapId;
    resetFrame();
    if (!statusPanel_) {
        statusPanel_ = new StatusView(renderer_, x_, y_, width_, height_);
    }
    return true;
}

bool Warfield::getDefaultChars(std::set<std::int16_t> &chars) const {
    const auto *info = data::gWarfieldData.info(warId_);
    if (info->forceMembers[0] >= 0) { return false; }
    for (auto &id: info->defaultMembers) {
        if (id >= 0) { chars.insert(id); }
    }
    return true;
}

void Warfield::putChars(const std::vector<std::int16_t> &chars) {
    const auto *info = data::gWarfieldData.info(warId_);
    if (info->forceMembers[0] >= 0) {
        for (size_t i = 0; i < data::TeamMemberCount; ++i) {
            auto id = info->forceMembers[i];
            if (id < 0) { continue; }
            auto *charInfo = mem::gSaveData.charInfo[id];
            if (!charInfo) { continue; }
            chars_.emplace_back(CharInfo {0, id, charInfo->headId, info->memberX[i], info->memberY[i], DirLeft,
                                          *charInfo});
        }
    } else {
        std::map<std::int16_t, size_t> charMap;
        std::set<size_t> indices;
        for (size_t i = 0; i < data::TeamMemberCount; ++i) {
            auto id = info->defaultMembers[i];
            if (id >= 0) { charMap[id] = i; }
            else { indices.insert(i); }
        }
        for (auto id: chars) {
            auto *charInfo = mem::gSaveData.charInfo[id];
            if (!charInfo) { continue; }
            auto ite = charMap.find(id);
            size_t index;
            if (ite != charMap.end()) {
                index = ite->second;
            } else {
                index = *indices.begin();
                indices.erase(indices.begin());
            }
            chars_.emplace_back(CharInfo{0, id, charInfo->headId, info->memberX[index], info->memberY[index],
                                         DirLeft, *charInfo});
        }
    }
    for (size_t i = 0; i < data::WarFieldEnemyCount; ++i) {
        auto id = info->enemy[i];
        if (id < 0) { continue; }
        auto *charInfo = mem::gSaveData.charInfo[id];
        if (!charInfo) { continue; }
        chars_.emplace_back(CharInfo {1, id, charInfo->headId, info->enemyX[i], info->enemyY[i],
                                      DirRight, *charInfo});
    }
    auto ite = chars_.begin();
    while (ite != chars_.end()) {
        auto &ci = *ite;
        auto &cell = cellInfo_[ci.y * mapWidth_ + ci.x];
        /* NOTE: remove duplicate chars */
        if (cell.charInfo != nullptr) {
            ite = chars_.erase(ite);
            continue;
        }
        mem::addUpPropFromEquipToChar(&ci.info);
        if (ci.side == 1) {
            ci.info.hp = ci.info.maxHp;
            ci.info.mp = ci.info.maxMp;
            ci.info.stamina = data::StaminaMax;
        }
        cell.charInfo = &ci;
        ++ite;
    }
    recalcKnowledge();
    frameUpdate();
    if (info->music >= 0) {
        gWindow->playMusic(info->music);
    }
}

void Warfield::render() {
    Map::render();

    bool acting = stage_ == Acting;
    if (drawDirty_) {
        drawDirty_ = false;
        int cellDiffX = cellWidth_ / 2;
        int cellDiffY = cellHeight_ / 2;
        int curX = cameraX_, curY = cameraY_;
        auto aheight = int(auxHeight_);
        int nx = int(auxWidth_) / 2 + cellWidth_ * 2;
        int ny = aheight / 2 + cellHeight_ * 2;
        int wcount = nx * 2 / cellWidth_;
        int hcount = (ny * 2 + 4 * cellHeight_) / cellDiffY;
        int cx, cy, tx, ty;
        int delta = -mapWidth_ + 1;

        cx = (nx / cellDiffX + ny / cellDiffY) / 2;
        cy = (ny / cellDiffY - nx / cellDiffX) / 2;
        tx = int(auxWidth_) / 2 - (cx - cy) * cellDiffX;
        ty = int(auxHeight_) / 2 + cellDiffY - (cx + cy) * cellDiffY;
        cx = curX - cx; cy = curY - cy;
        bool selecting = stage_ == MoveSelecting || stage_ == AttackSelecting;
        bool movingOrActing = acting || stage_ == Moving;
        auto *ch = charQueue_.back();
        if (acting && effectTexIdx_ >= 0) {
            const auto *skillInfo = actId_ > 0 ? mem::gSaveData.skillInfo[actId_] : nullptr;
            const auto &effTexData = gEffect[effectId_];
            const auto *tex = effectTexIdx_ < effTexData.size() ? &effTexData[effectTexIdx_] : &effTexData.back();
            auto mw = mapWidth_;
            if (skillInfo == nullptr || skillInfo->attackAreaType == 0) {
                auto sx = cursorX_, sy = cursorY_;
                cellInfo_[sy * mw + sx].effectData = tex;
            } else {
                switch (skillInfo->attackAreaType) {
                case 1: {
                    auto sx = cameraX_, sy = cameraY_, st = sy * mw;
                    int r = skillInfo->selRange[actLevel_];
                    for (int i = r; i; --i) {
                        switch (charQueue_.back()->direction) {
                        case Map::DirUp:
                            if (sy >= i) {
                                auto &ci = cellInfo_[st - i * mw + sx];
                                if (ci.buildingId <= 0) { ci.effectData = tex; }
                            }
                            break;
                        case Map::DirRight:
                            if (sx + i < mapWidth_) {
                                auto &ci = cellInfo_[st + sx + i];
                                if (ci.buildingId <= 0) { ci.effectData = tex; }
                            }
                            break;
                        case Map::DirLeft:
                            if (sx >= i) {
                                auto &ci = cellInfo_[st + sx - i];
                                if (ci.buildingId <= 0) { ci.effectData = tex; }
                            }
                            break;
                        case Map::DirDown:
                            if (sy + i < mapHeight_) {
                                auto &ci = cellInfo_[st + i * mw + sx];
                                if (ci.buildingId <= 0) { ci.effectData = tex; }
                            }
                            break;
                        default:
                            break;
                        }
                    }
                    break;
                }
                case 2: {
                    auto sx = cameraX_, sy = cameraY_, st = sy * mw;
                    int r = skillInfo->selRange[actLevel_];
                    for (int i = r; i; --i) {
                        if (sy >= i) {
                            auto &ci = cellInfo_[st - i * mw + sx];
                            if (ci.buildingId <= 0) { ci.effectData = tex; }
                        }
                        if (sx + i < mapWidth_) {
                            auto &ci = cellInfo_[st + sx + i];
                            if (ci.buildingId <= 0) { ci.effectData = tex; }
                        }
                        if (sx >= i) {
                            auto &ci = cellInfo_[st + sx - i];
                            if (ci.buildingId <= 0) { ci.effectData = tex; }
                        }
                        if (sy + i < mapHeight_) {
                            auto &ci = cellInfo_[st + i * mw + sx];
                            if (ci.buildingId <= 0) { ci.effectData = tex; }
                        }
                    }
                    break;
                }
                case 3: {
                    auto sx = cursorX_, sy = cursorY_;
                    int r = skillInfo->selRange[actLevel_];
                    for (int j = -r; j <= r; ++j) {
                        auto ry = sy + j;
                        if (ry < 0 || ry >= mapHeight_) { continue; }
                        for (int i = -r; i <= r; ++i) {
                            auto rx = sx + i;
                            if (rx < 0 || rx >= mapWidth_) { continue; }
                            auto &ci = cellInfo_[ry * mw + rx];
                            if (ci.buildingId <= 0) { ci.effectData = tex; }
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        const auto *colors = gNormalPalette.colors();
        int pitch, pitch2;
        std::uint32_t *pixels = drawingTerrainTex_->lock(pitch);
        std::uint32_t *pixels2 = drawingTerrainTex2_->lock(pitch2);
        memset(pixels, 0, pitch * auxHeight_ * sizeof(std::uint32_t));
        memset(pixels2, 0, pitch * auxHeight_ * sizeof(std::uint32_t));
        for (int j = hcount; j; --j) {
            int x = cx, y = cy;
            int dx = tx;
            int offset = y * mapWidth_ + x;
            for (int i = wcount; i; --i, dx += cellWidth_, offset += delta, ++x, --y) {
                if (x < 0 || x >= data::WarFieldWidth || y < 0 || y >= data::WarFieldHeight) {
                    continue;
                }
                auto &ci = cellInfo_[offset];
                Texture::renderRLE(texData_[ci.earthId], colors, pixels, pitch, aheight, dx, ty);
                if (!movingOrActing) {
                    static std::uint32_t maskColors[256] = {0};
                    if (ci.insideMovingArea == 2) {
                        maskColors[254] = 0xA0A0A0A0u;
                        Texture::renderRLEBlending(texData_[0], maskColors, pixels, pitch, aheight, dx, ty);
                    } else if (ci.charInfo) {
                        maskColors[254] = 0x80A0A0A0u;
                        Texture::renderRLEBlending(texData_[0], maskColors, pixels, pitch, aheight, dx, ty);
                    } else if (selecting && !ci.insideMovingArea) {
                        maskColors[254] = 0xD0A0A0A0u;
                        Texture::renderRLEBlending(texData_[0], maskColors, pixels, pitch, aheight, dx, ty);
                    }
                }
                if (ci.buildingId > 0) {
                    Texture::renderRLE(texData_[ci.buildingId], colors, pixels2, pitch2, aheight, dx, ty);
                } else {
                    if (ci.charInfo) {
                        if (acting && ci.charInfo == ch && fightTex_ && fightTexIdx_ >= 0 && fightTexIdx_ < fightTex_->size()) {
                            Texture::renderRLE((*fightTex_)[fightTexIdx_], colors, pixels2, pitch2, aheight, dx, ty);
                        } else {
                            Texture::renderRLE(texData_[2553 + 4 * ci.charInfo->texId
                                + int(ci.charInfo->direction)], colors, pixels2, pitch2, aheight, dx, ty);
                        }
                    }
                    if (ci.effectData) {
                        Texture::renderRLE(*ci.effectData, colors, pixels2, pitch2, aheight, dx, ty);
                        ci.effectData = nullptr;
                    }
                }
            }
            if (j % 2) {
                ++cx;
                tx += cellDiffX;
                ty += cellDiffY;
            } else {
                ++cy;
                tx -= cellDiffX;
                ty += cellDiffY;
            }
        }
        drawingTerrainTex2_->unlock();
        drawingTerrainTex_->unlock();
    }
    renderer_->clear(0, 0, 0, 0);
    renderer_->renderTexture(drawingTerrainTex_, x_, y_, width_, height_, 0, 0, auxWidth_, auxHeight_);
    renderer_->renderTexture(drawingTerrainTex2_, x_, y_, width_, height_, 0, 0, auxWidth_, auxHeight_);
    if (acting && effectTexIdx_ >= 3) {
        int cellDiffX = cellWidth_ / 2;
        int cellDiffY = cellHeight_ / 2;
        int ax = int(auxWidth_) / 2, ay = int(auxHeight_) / 2 + cellDiffY;
        auto *ttf = renderer_->ttf();
        auto fsize = 12 * scale_.first / scale_.second;
        for (auto &n: popupNumbers_) {
            int deltax = n.x - cameraX_, deltay = n.y - cameraY_;
            int texX = (ax + (deltax - deltay) * cellDiffX) * scale_.first / scale_.second;
            int texY = (ay + (deltax + deltay) * cellDiffY - cellDiffY * 3 - fsize - effectTexIdx_ * 2) * scale_.first / scale_.second;
            texX -= ttf->stringWidth(n.str, fsize) / 2;
            ttf->setColor((n.r + 256) / 2, (n.g + 256) / 2, (n.b + 256) / 2);
            ttf->render(n.str, texX + 1, texY, false, fsize);
            ttf->setColor(n.r, n.g, n.b);
            ttf->render(n.str, texX, texY, false, fsize);
        }
    }
    if (stage_ == Idle || stage_ == PlayerMenu || stage_ == Moving) {
        statusPanel_->render();
    }
}

void Warfield::handleKeyInput(Node::Key key) {
    if (stage_ != MoveSelecting && stage_ != AttackSelecting) {
        if (key == KeyCancel) {
            if (charQueue_.back()->side == 0) {
                pendingAutoAction_ = nullptr;
            }
            autoControl_ = false;
        }
        return;
    }
    int x, y;
    switch (key) {
    case KeyUp:
        y = cursorY_ - 1;
        if (y < 0 || !cellInfo_[y * mapWidth_ + cursorX_].insideMovingArea) { break; }
        cellInfo_[cursorY_ * mapWidth_ + cursorX_].insideMovingArea = 1;
        cursorY_ = y;
        drawDirty_ = true;
        break;
    case KeyDown:
        y = cursorY_ + 1;
        if (y >= mapHeight_ || !cellInfo_[y * mapWidth_ + cursorX_].insideMovingArea) { break; }
        cellInfo_[cursorY_ * mapWidth_ + cursorX_].insideMovingArea = 1;
        cursorY_ = y;
        drawDirty_ = true;
        break;
    case KeyLeft:
        x = cursorX_ - 1;
        if (x < 0 || !cellInfo_[cursorY_ * mapWidth_ + x].insideMovingArea) { break; }
        cellInfo_[cursorY_ * mapWidth_ + cursorX_].insideMovingArea = 1;
        cursorX_ = x;
        drawDirty_ = true;
        break;
    case KeyRight:
        x = cursorX_ + 1;
        if (x >= mapWidth_ || !cellInfo_[cursorY_ * mapWidth_ + x].insideMovingArea) { break; }
        cellInfo_[cursorY_ * mapWidth_ + cursorX_].insideMovingArea = 1;
        cursorX_ = x;
        drawDirty_ = true;
        break;
    case KeyOK: case KeySpace: {
        x = cursorX_; y = cursorY_;
        switch (stage_) {
        case MoveSelecting: {
            if (x == cameraX_ && y == cameraY_) { stage_ = Idle; break; }
            if (cellInfo_[y * mapWidth_ + x].charInfo) {
                stage_ = Idle;
                break;
            }
            auto ite = selCells_.find(std::make_pair(x, y));
            if (ite != selCells_.end()) {
                stage_ = Moving;
                auto *sc = &ite->second;
                while (sc) {
                    movingPath_.emplace_back(std::make_pair(sc->x, sc->y));
                    sc = sc->moveParent;
                }
            } else {
                stage_ = Idle;
            }
            break;
        }
        case AttackSelecting: {
            startActAction();
            break;
        }
        default:
            break;
        }
        unmaskArea();
        drawDirty_ = true;
        return;
    }
    case KeyCancel:
        unmaskArea();
        drawDirty_ = true;
        playerMenu();
        return;
    default:
        return;
    }
    if (drawDirty_) {
        cellInfo_[cursorY_ * mapWidth_ + cursorX_].insideMovingArea = 2;
    }
}

void Warfield::frameUpdate() {
    switch (stage_) {
    case Idle:
        nextAction();
        break;
    case Moving: {
        if (movingPath_.empty()) { stage_ = Idle; break; }
        int x, y;
        std::tie(x, y) = movingPath_.back();
        if (x == cameraX_ && y == cameraY_) {
            movingPath_.pop_back();
            std::tie(x, y) = movingPath_.back();
        }
        movingPath_.pop_back();
        auto &ci = cellInfo_[cameraX_ + cameraY_ * mapWidth_];
        auto &newci = cellInfo_[x + y * mapWidth_];
        auto *charInfo = ci.charInfo;
        if (x < cameraX_) {
            charInfo->direction = DirLeft;
        } else if (x > cameraX_) {
            charInfo->direction = DirRight;
        } else if (y < cameraY_) {
            charInfo->direction = DirUp;
        } else if (y > cameraY_) {
            charInfo->direction = DirDown;
        }
        --charInfo->steps;
        newci.charInfo = charInfo;
        ci.charInfo = nullptr;
        charInfo->x = x;
        charInfo->y = y;
        cameraX_ = x;
        cameraY_ = y;
        drawDirty_ = true;
        break;
    }
    case Acting: {
        fightTexIdx_ = std::min(fightTexIdx_ + 1, fightTexCount_ - 1);
        if (fightFrame_ == 0) {
            const mem::SkillData *skillInfo;
            if (actId_ > 0 && (skillInfo = mem::gSaveData.skillInfo[actId_]) != nullptr) {
                gWindow->playAtkSound(skillInfo->soundId);
            } else {
                gWindow->playAtkSound(0);
            }
        } else if (fightFrame_ == 3) {
            gWindow->playEffectSound(effectId_);
        }
        ++fightFrame_;
        if (++effectTexIdx_ >= int(gEffect[effectId_].size()) + 3) {
            auto postFunc = [this]() {
                if (--attackTimesLeft_ > 0) {
                    auto *ch = charQueue_.back();
                    const auto *skill = mem::gSaveData.skillInfo[actId_];
                    if (skill) {
                        actLevel_ = mem::calcRealSkillLevel(skill->reqMp, actLevel_, ch->info.mp);
                    }
                    if (actLevel_ >= 0) {
                        startActAction();
                    } else {
                        actIndex_ = actId_ = -1;
                    }
                } else {
                    actIndex_ = actId_ = -1;
                }
                if (actIndex_ < 0) {
                    skillLevelup_ = false;
                    actLevel_ = 0;
                    effectId_ = -1;
                    effectTexIdx_ = -1;
                    fightTexIdx_ = -1;
                    fightTexCount_ = 0;
                    fightFrame_ = 0;
                    attackTimesLeft_ = 0;
                    fightTex_ = nullptr;
                    endTurn();
                }
            };
            if (skillLevelup_) {
                skillLevelup_ = false;
                stage_ = PoppingUp;
                const auto *skill = mem::gSaveData.skillInfo[actId_];
                auto *ch = charQueue_.back();
                auto *msgBox = new MessageBox(this, 0, height_ / 3, width_, 60);
                msgBox->popup({fmt::format(GETTEXT(81), GETSKILLNAME(actId_),
                                           ch->info.skillLevel[actIndex_] / 100 + 1)}, MessageBox::PressToCloseThis);
                msgBox->setCloseHandler([this, postFunc]() {
                    stage_ = Acting;
                    postFunc();
                });
            } else {
                postFunc();
            }
        }
        drawDirty_ = true;
        break;
    }
    default:
        break;
    }
}

void Warfield::nextAction() {
    CharInfo *ch;
    for (;;) {
        if (charQueue_.empty()) {
            charQueue_.reserve(chars_.size());
            for (auto &c: chars_) {
                if (c.info.hp > 0) {
                    charQueue_.emplace_back(&c);
                    c.steps = c.info.speed / 15;
                }
            }
            std::stable_sort(charQueue_.begin(), charQueue_.end(), [](const CharInfo *c0, const CharInfo *c1) {
                return c0->info.speed < c1->info.speed;
            });
        }
        ch = charQueue_.back();
        if (ch->info.hp <= 0) {
            charQueue_.pop_back();
            continue;
        }
        break;
    }
    mem::actPoisonDamage(&ch->info);
    cameraX_ = ch->x;
    cameraY_ = ch->y;
    drawDirty_ = true;
    auto *sv = dynamic_cast<StatusView*>(statusPanel_);
    auto windowBorder = core::config.windowBorder();
    sv->show(&ch->info, false, true);
    sv->forceUpdate();
    sv->setPosition(ch->side == 1 ? windowBorder * 4 : (width_ - windowBorder * 4 - sv->width()), height_ * 2 / 5 - sv->height() / 2);
    if (ch->side == 1 || autoControl_) {
        autoAction();
    } else {
        lastMenuIndex_ = 0;
        playerMenu();
    }
}

void Warfield::autoAction() {
    if (pendingAutoAction_) {
        pendingAutoAction_();
        pendingAutoAction_ = nullptr;
        return;
    }
    auto *ch = charQueue_.back();
    if (ch->info.stamina < 10) {
        pendingAutoAction_ = [this, ch]() {
            std::map<mem::PropType, std::int16_t> changes;
            auto delta = data::StaminaMax - ch->info.stamina;
            std::int16_t itemId;
            if (ch->side == 1) {
                itemId = mem::tryUseNpcItem(&ch->info, mem::PropType::Stamina, delta);
                if (!mem::useNpcItem(&ch->info, itemId, changes)) { itemId = -1; }
            } else {
                itemId = mem::tryUseBagItem(&ch->info, mem::PropType::Stamina, delta);
                if (!mem::useItem(&ch->info, itemId, changes)) { itemId = -1; }
            }
            if (itemId < 0) {
                doRest();
            } else {
                stage_ = PoppingUp;
                auto *msgBox = ItemView::popupUseResult(this, itemId, changes);
                msgBox->setCloseHandler([this] {
                    charQueue_.pop_back();
                    stage_ = Idle;
                });
            }
        };
    } else if (ch->info.hp < 20 || ch->info.hp <= ch->info.maxHp / 5) {
        auto delta = ch->info.maxHp - ch->info.hp;
        std::int16_t itemId;
        if (ch->side == 1) {
            itemId = mem::tryUseNpcItem(&ch->info, mem::PropType::Hp, delta);
        } else {
            itemId = mem::tryUseBagItem(&ch->info, mem::PropType::Hp, delta);
        }
        if (itemId >= 0 || ch->info.hp <= 20) {
            pendingAutoAction_ = [this, ch, itemId]() {
                std::map<mem::PropType, std::int16_t> changes;
                bool usedItem = false;
                if (itemId >= 0) {
                    if (ch->side == 1) {
                        usedItem = mem::useNpcItem(&ch->info, itemId, changes);
                    } else {
                        usedItem = mem::useItem(&ch->info, itemId, changes);
                    }
                }
                if (!usedItem) {
                    doRest();
                } else {
                    stage_ = PoppingUp;
                    auto *msgBox = ItemView::popupUseResult(this, itemId, changes);
                    msgBox->setCloseHandler([this] {
                        charQueue_.pop_back();
                        stage_ = Idle;
                    });
                }
            };
        }
    } else if (ch->info.poisoned > 33 && ch->side == 1) {
        auto delta = ch->info.poisoned;
        std::int16_t itemId;
        if (ch->side == 1) {
            itemId = mem::tryUseNpcItem(&ch->info, mem::PropType::Poisoned, delta);
        } else {
            itemId = mem::tryUseBagItem(&ch->info, mem::PropType::Poisoned, delta);
        }
        if (itemId >= 0) {
            pendingAutoAction_ = [this, ch, itemId]() {
                std::map<mem::PropType, std::int16_t> changes;
                bool usedItem = false;
                if (itemId >= 0) {
                    if (ch->side == 1) {
                        usedItem = mem::useNpcItem(&ch->info, itemId, changes);
                    } else {
                        usedItem = mem::useItem(&ch->info, itemId, changes);
                    }
                }
                if (!usedItem) {
                    doRest();
                } else {
                    stage_ = PoppingUp;
                    auto *msgBox = ItemView::popupUseResult(this, itemId, changes);
                    msgBox->setCloseHandler([this] {
                        charQueue_.pop_back();
                        stage_ = Idle;
                    });
                }
            };
        }
    }
    struct SkillPredict {
        const mem::SkillData *skill;
        std::int16_t index;
        std::int16_t level;
        std::int16_t atk;
        std::int16_t rangeType;
        std::int16_t skillRange, area;
    };
    SkillPredict skills[data::LearnSkillCount];
    int skillCount = 0, maxRange = 0;
    if (!pendingAutoAction_) {
        for (int i = 0; i < data::LearnSkillCount; ++i) {
            if (ch->info.skillId[i] <= 0) { continue; }
            const auto *skill = mem::gSaveData.skillInfo[ch->info.skillId[i]];
            if (!skill || skill->damageType > 0) { continue; }
            std::int16_t level = mem::calcRealSkillLevel(skill->reqMp,
                                                         std::clamp<std::int16_t>(ch->info.skillLevel[i] / 100, 0, 9),
                                                         ch->info.mp);
            if (level < 0) { continue; }
            std::int16_t atk = mem::calcRealAttack(&ch->info, knowledge_[ch->side], skill, level);
            std::int16_t type = skill->attackAreaType, range = skill->selRange[level], area = skill->area[level];
            skills[skillCount++] = SkillPredict{skill, std::int16_t(i), level, atk, type, range, area};
            if (type == 0 || type == 3) {
                maxRange = std::max<int>(maxRange, range);
            }
        }
        if (!skillCount) {
            pendingAutoAction_ = [this, ch]() {
                std::map<mem::PropType, std::int16_t> changes;
                auto delta = ch->info.maxMp - ch->info.mp;
                std::int16_t itemId;
                if (ch->side == 1) {
                    itemId = mem::tryUseNpcItem(&ch->info, mem::PropType::Mp, delta);
                    if (!mem::useNpcItem(&ch->info, itemId, changes)) { itemId = -1; }
                } else {
                    itemId = mem::tryUseBagItem(&ch->info, mem::PropType::Mp, delta);
                    if (!mem::useItem(&ch->info, itemId, changes)) { itemId = -1; }
                }
                if (itemId < 0) {
                    doRest();
                } else {
                    stage_ = PoppingUp;
                    auto *msgBox = ItemView::popupUseResult(this, itemId, changes);
                    msgBox->setCloseHandler([this] {
                        charQueue_.pop_back();
                        stage_ = Idle;
                    });
                }
            };
        }
    }
    CharInfo *enemies[std::max(data::WarFieldEnemyCount, data::TeamMemberCount)];
    int enemyCount = 0;
    auto enemySide = ch->side ^ 1;
    for (auto &ci: chars_) {
        if (ci.side != enemySide || ci.info.hp <= 0) { continue; }
        enemies[enemyCount++] = &ci;
    }
    if (pendingAutoAction_) {
        std::map<std::pair<int, int>, SelectableCell> selCells;
        getSelectableArea(ch, selCells, ch->steps, 0);
        int distance = 0;
        int mx = -1, my = -1;
        for (auto &c: selCells) {
            if (c.second.moves < 0) { continue; }
            std::int16_t x, y;
            std::tie(x, y) = c.first;
            for (int i = 0; i < enemyCount; ++i) {
                auto *enemy = enemies[i];
                int dist = std::abs(enemy->x - x) + std::abs(enemy->y - y);
                if (dist > distance) {
                    distance = dist;
                    mx = x; my = y;
                }
            }
        }
        if (mx != ch->x || my != ch->y) {
            stage_ = Moving;
            movingPath_.clear();
            auto sc = &selCells[std::make_pair(mx, my)];
            while (sc) {
                movingPath_.emplace_back(std::make_pair(sc->x, sc->y));
                sc = sc->moveParent;
            }
        } else {
            pendingAutoAction_();
            pendingAutoAction_ = nullptr;
        }
        return;
    }
    std::map<std::pair<int, int>, SelectableCell> selCells;
    int steps = ch->steps;
    getSelectableArea(ch, selCells, steps, maxRange);
    struct PredictScore {
        int score;
        std::int16_t fx, fy, tx, ty;
        int skillIndex;
    };
    std::vector<PredictScore> scores;
    scores.reserve(selCells.size());
    for (int j = 0; j < skillCount; ++j) {
        auto rt = skills[j].rangeType;
        for (auto &c: selCells) {
            std::int16_t x, y;
            std::tie(x, y) = c.first;
            switch (rt) {
            case 1: {
                if (c.second.moves < 0) { continue; }
                int totalDmg[4] = {0, 0, 0, 0};
                for (int i = 0; i < enemyCount; ++i) {
                    auto *enemy = enemies[i];
                    std::int16_t ex = enemy->x, ey = enemy->y;
                    auto r = skills[j].skillRange;
                    int distance;
                    if ((ex == x && (distance = std::abs(ey - y)) <= r)
                        || (ey == y && (distance = std::abs(ex - x)) <= r)) {
                        int dmg = mem::calcPredictDamage(skills[j].atk, enemy->info.defence,
                                                         ch->info.stamina, enemy->info.hurt,
                                                         distance);
                        if (dmg >= enemy->info.hp) { dmg = std::max<int>(dmg * 3 / 2, enemy->info.maxHp); }
                        if (ey < y)
                            totalDmg[0] += dmg;
                        else if (ex > x)
                            totalDmg[1] += dmg;
                        else if (ex < x)
                            totalDmg[2] += dmg;
                        else
                            totalDmg[3] += dmg;
                    }
                }
                for (std::int16_t i = 0; i < 4; ++i) {
                    if (totalDmg[i] > 0) {
                        scores.emplace_back(PredictScore{totalDmg[i], x, y, i, -1, skills[j].index});
                    }
                }
                break;
            }
            case 2: {
                if (c.second.moves < 0) { continue; }
                int totalDmg = 0;
                auto r = skills[j].skillRange;
                for (int i = 0; i < enemyCount; ++i) {
                    auto *enemy = enemies[i];
                    std::int16_t ex = enemy->x, ey = enemy->y;
                    int distance;
                    if ((ex == x && (distance = std::abs(ey - y)) <= r)
                        || (ey == y && (distance = std::abs(ex - x)) <= r)) {
                        int dmg = mem::calcPredictDamage(skills[j].atk, enemy->info.defence,
                                                         ch->info.stamina, enemy->info.hurt,
                                                         distance);
                        if (dmg >= enemy->info.hp) { dmg = std::max<int>(dmg * 3 / 2, enemy->info.maxHp); }
                        totalDmg += dmg;
                    }
                }
                if (totalDmg > 0) {
                    scores.emplace_back(PredictScore{totalDmg, x, y, x, y, skills[j].index});
                }
                break;
            }
            case 3: {
                if (c.second.ranges > skills[j].skillRange) { continue; }
                int totalDmg = 0;
                auto r = skills[j].area;
                auto *n = &c.second;
                if (n->moves > 0) {
                    n = n->moveParent;
                } else {
                    while (n->moves < 0) {
                        n = n->rangeParent;
                    }
                }
                std::int16_t mx = n->x, my = n->y;
                for (int i = 0; i < enemyCount; ++i) {
                    auto *enemy = enemies[i];
                    std::int16_t ex = enemy->x, ey = enemy->y;
                    if (std::abs(ex - x) > r || std::abs(ey - y) > r) { continue; }
                    int distance = std::abs(x - mx) + std::abs(y - my)
                                 + std::abs(x - ex) + std::abs(y - ey);
                    int dmg = mem::calcPredictDamage(skills[j].atk, enemy->info.defence,
                                                     ch->info.stamina, enemy->info.hurt,
                                                     distance);
                    if (dmg >= enemy->info.hp) { dmg = std::max<int>(dmg * 3 / 2, enemy->info.maxHp); }
                    totalDmg += dmg;
                }
                if (totalDmg > 0) {
                    scores.emplace_back(PredictScore{totalDmg, mx, my, x, y, skills[j].index});
                }
                break;
            }
            default: {
                if (c.second.ranges > skills[j].skillRange) { continue; }
                auto *enemy = cellInfo_[y * mapWidth_ + x].charInfo;
                if (!enemy || enemy->side != enemySide) { continue; }
                auto *n = &c.second;
                if (n->moves > 0) {
                    n = n->moveParent;
                } else {
                    while (n->moves < 0) {
                        n = n->rangeParent;
                    }
                }
                std::int16_t mx = n->x, my = n->y;
                int distance = std::abs(mx - x) + std::abs(my - y);
                int dmg = mem::calcPredictDamage(skills[j].atk, enemy->info.defence,
                                                 ch->info.stamina, enemy->info.hurt,
                                                 distance);
                if (dmg >= enemy->info.hp) { dmg = dmg * 3 / 2; }
                scores.emplace_back(PredictScore{dmg, mx, my, x, y, skills[j].index});
                break;
            }
            }
        }
    }
    if (scores.empty()) {
        int distance = 255;
        int mx = -1, my = -1;
        for (auto &c: selCells) {
            if (c.second.moves < 0) { continue; }
            std::int16_t x, y;
            std::tie(x, y) = c.first;
            for (int i = 0; i < enemyCount; ++i) {
                auto *enemy = enemies[i];
                int dist = std::abs(enemy->x - x) + std::abs(enemy->y - y);
                if (dist < distance) {
                    distance = dist;
                    mx = x; my = y;
                }
            }
        }
#ifndef NDEBUG
        fmt::print(stdout, "({},{})->({},{})\n", ch->x, ch->y, mx, my);
        fflush(stdout);
#endif
        pendingAutoAction_ = [this]() {
            doRest();
        };
        if (mx != ch->x || my != ch->y) {
            stage_ = Moving;
            movingPath_.clear();
            auto sc = &selCells[std::make_pair(mx, my)];
            while (sc) {
                movingPath_.emplace_back(std::make_pair(sc->x, sc->y));
                sc = sc->moveParent;
            }
        } else {
            pendingAutoAction_();
            pendingAutoAction_ = nullptr;
        }
    } else {
        std::sort(scores.begin(), scores.end(), [](const PredictScore &v0, const PredictScore &v1) {
            return v0.score > v1.score;
        });
        int ratio[3], ratioTotal = 0;
        int counter = 0;
        for (auto &s: scores) {
#ifndef NDEBUG
            fmt::print(stdout, "({},{})->({},{}): {}={}\n", s.fx, s.fy, s.tx, s.ty, s.skillIndex, s.score);
            fflush(stdout);
#endif
            ratio[counter] = s.score;
            ratioTotal += s.score;
            if (++counter == 3) {
                break;
            }
        }
        int randNum = util::gRandom(ratioTotal);
        int sel;
        if (counter > 1) {
            for (sel = 0; sel < 2; ++sel) {
                if (randNum < ratio[sel]) {
                    break;
                }
                randNum -= ratio[sel];
            }
        } else {
            sel = 0;
        }
        auto &s = scores[sel];
        pendingAutoAction_ = [this, ch, s]() {
            actIndex_ = s.skillIndex;
            actId_ = ch->info.skillId[s.skillIndex];
            attackTimesLeft_ = ch->info.doubleAttack ? 2 : 1;
            actLevel_ = std::clamp<std::int16_t>(ch->info.skillLevel[s.skillIndex] / 100, 0, 9);
            const auto *skill = mem::gSaveData.skillInfo[actId_];
            if (!skill || (actLevel_ = mem::calcRealSkillLevel(skill->reqMp, actLevel_, ch->info.mp)) < 0) {
                /* impossible to run these codes if no logic bug */
                charQueue_.pop_back();
                stage_ = Idle;
                return;
            }
            if (s.ty < 0) {
                ch->direction = Map::Direction(s.tx);
                cursorX_ = s.fx; cursorY_ = s.fy;
            } else {
                cursorX_ = s.tx; cursorY_ = s.ty;
            }
            startActAction();
        };
        if (s.fx != ch->x || s.fy != ch->y) {
            stage_ = Moving;
            movingPath_.clear();
            auto sc = &selCells[std::make_pair(s.fx, s.fy)];
            while (sc) {
                movingPath_.emplace_back(std::make_pair(sc->x, sc->y));
                sc = sc->moveParent;
            }
        } else {
            pendingAutoAction_();
            pendingAutoAction_ = nullptr;
        }
    }
}

void Warfield::recalcKnowledge() {
    knowledge_[0] = knowledge_[1] = 0;
    for (auto &ci: chars_) {
        if (ci.info.hp > 0 && ci.info.knowledge >= data::KnowledgeBarrier) {
            knowledge_[ci.side] += ci.info.knowledge;
        }
    }
}

void Warfield::playerMenu() {
    stage_ = PlayerMenu;
    auto windowBorder = core::config.windowBorder();
    auto *ch = charQueue_.back();
    auto *menu = new MenuTextList(this, windowBorder * 4, windowBorder * 4, width_ - windowBorder * 8, height_ - windowBorder * 8);
    std::vector<std::wstring> n;
    std::vector<int> menuIndices;
    n.reserve(10);
    menuIndices.reserve(10);
    auto &info = ch->info;
    if (ch->steps && info.stamina >= 5) {
        n.emplace_back(GETTEXT(82)); menuIndices.emplace_back(0);
    }
    if (info.stamina >= 10) {
        n.emplace_back(GETTEXT(83)); menuIndices.emplace_back(1);
        if (info.poison) {
            n.emplace_back(GETTEXT(84)); menuIndices.emplace_back(2);
        }
    }
    if (info.stamina >= 50) {
        if (info.depoison) {
            n.emplace_back(GETTEXT(85));
            menuIndices.emplace_back(3);
        }
        if (info.medic) {
            n.emplace_back(GETTEXT(86));
            menuIndices.emplace_back(4);
        }
    }
    n.emplace_back(GETTEXT(87)); menuIndices.emplace_back(5);
    if (charQueue_.size() > 1) {
        n.emplace_back(GETTEXT(88)); menuIndices.emplace_back(6);
    }
    n.emplace_back(GETTEXT(89)); menuIndices.emplace_back(7);
    n.emplace_back(GETTEXT(90)); menuIndices.emplace_back(8);
    n.emplace_back(GETTEXT(91)); menuIndices.emplace_back(9);
    menu->popup(n, lastMenuIndex_);
    menu->setHandler([this, menu, menuIndices, ch]() {
        auto index = menu->currIndex();
        if (index < 0 || index >= menuIndices.size()) { return; }
        lastMenuIndex_ = index;
        switch (menuIndices[index]) {
        case 0:
            maskSelectableArea(ch->steps, 0);
            stage_ = MoveSelecting;
            drawDirty_ = true;
            break;
        case 1:
            if (ch->info.skillId[1] > 0) {
                std::vector<std::wstring> items;
                std::vector<int> indices;
                for (int i = 0; i < data::LearnSkillCount; ++i) {
                    auto skillId = ch->info.skillId[i];
                    if (skillId <= 0) { continue; }
                    const auto *skillInfo = mem::gSaveData.skillInfo[skillId];
                    if (!skillInfo) { continue; }
                    auto skillLevel =
                        mem::calcRealSkillLevel(skillInfo->reqMp,
                                                std::clamp<std::int16_t>(ch->info.skillLevel[i] / 100, 0, 9),
                                                ch->info.mp);
                    if (skillLevel < 0) { continue; }
                    indices.emplace_back(i);
                    items.emplace_back(GETSKILLNAME(skillId));
                }
                if (!items.empty()) {
                    auto windowBorder = core::config.windowBorder();
                    auto *submenu = new MenuTextList(menu, menu->x() + menu->width() + windowBorder, windowBorder * 4,
                                                     width_ - menu->x() + menu->width() - windowBorder, height_ - windowBorder * 8);
                    submenu->popup(items);
                    submenu->setHandler([this, menu, submenu, indices]() {
                        if (tryUseSkill(indices[submenu->currIndex()])) {
                            delete menu;
                        } else {
                            delete submenu;
                        }
                    });
                    return;
                }
            } else {
                if (tryUseSkill(0)) {
                    delete menu;
                    return;
                }
            }
            {
                auto *msgBox = new MessageBox(this, 0, height_ / 3, width_, 60);
                msgBox->popup({GETTEXT(115)}, MessageBox::PressToCloseThis);
            }
            return;
        case 2:
            if (tryUseSkill(-3)) {
                delete menu;
            }
            return;
        case 3:
            if (tryUseSkill(-2)) {
                delete menu;
            }
            return;
        case 4:
            if (tryUseSkill(-1)) {
                delete menu;
            }
            return;
        case 5: {
            auto windowBorder = core::config.windowBorder();
            auto *iv = new ItemView(this, windowBorder * 4, windowBorder * 4, gWindow->width() - windowBorder * 4, gWindow->height() - windowBorder * 4);
            iv->setCharInfo(&ch->info);
            iv->show(true, [this](std::int16_t itemId) {
                if (itemId < 0) {
                    endTurn();
                } else {
                    auto *ch = charQueue_.back();
                    actIndex_ = itemId;
                    actId_ = -4;
                    actLevel_ = 0;
                    attackTimesLeft_ = 1;
                    maskSelectableArea(0, ch->info.throwing / 15);
                    stage_ = AttackSelecting;
                    drawDirty_ = true;
                }
            });
            iv->setCloseHandler([this]() { playerMenu(); });
            delete menu;
            return;
        }
        case 6:
            charQueue_.pop_back();
            charQueue_.insert(charQueue_.begin(), ch);
            stage_ = Idle;
            break;
        case 7: {
            std::vector<std::int16_t> idlist;
            for (auto &c: chars_) {
                idlist.emplace_back(c.side == 1 ? -c.id : c.id);
            }
            auto *svmenu = new CharListMenu(this, 0, 0, gWindow->width(), gWindow->height());
            svmenu->init({GETTEXT(59)}, idlist, {CharListMenu::LEVEL},
                         [this](std::int16_t charId) {
                             auto *sv = new StatusView(this, 0, 0, 0, 0);
                             bool found = false;
                             for (auto &p: chars_) {
                                 if (p.id == charId && p.side == 0) {
                                     sv->show(&p.info, false);
                                     found = true;
                                     break;
                                 }
                             }
                             if (!found) {
                                 sv->show(charId);
                             }
                             sv->makeCenter(width_, height_, x_, y_);
                         }, nullptr);
            svmenu->makeCenter(width_, height_ * 4 / 5, x_, y_);
            return;
        }
        case 8:
            doRest();
            break;
        case 9:
            autoControl_ = true;
            stage_ = Idle;
            break;
        default:
            return;
        }
        delete menu;
    }, []()->bool {
        return false;
    });
}

void Warfield::maskSelectableArea(int steps, int ranges, bool zoecheck) {
    auto *ch = charQueue_.back();
    getSelectableArea(ch, selCells_, steps, ranges, zoecheck);
    int w = mapWidth_;
    for (auto &c: selCells_) {
        auto &ci = cellInfo_[c.first.first + c.first.second * w];
        ci.insideMovingArea = true;
    }
    cursorX_ = ch->x;
    cursorY_ = ch->y;
}

void Warfield::unmaskArea() {
    int w = mapWidth_;
    for (auto c: selCells_) {
        cellInfo_[c.first.first + c.first.second * w].insideMovingArea = false;
    }
    selCells_.clear();
}

void Warfield::getSelectableArea(CharInfo *ch, std::map<std::pair<int, int>, SelectableCell> &selCells, int steps, int ranges, bool zoecheck) {
    struct CompareSelCells {
        bool operator()(const SelectableCell *a, const SelectableCell *b) {
            return a->moves > b->moves;
        }
    };
    auto myside = ch->side;
    int w = mapWidth_, h = mapHeight_;
    std::vector<SelectableCell*> sortedMovable;

    selCells.clear();
    auto &start = selCells[std::make_pair(ch->x, ch->y)];
    start.x = ch->x;
    start.y = ch->y;
    start.moves = 0;
    start.ranges = 0;
    start.moveParent = nullptr;
    start.rangeParent = nullptr;
    if (steps > 0) {
        sortedMovable.push_back(&start);
    }
    while (!sortedMovable.empty()) {
        std::pop_heap(sortedMovable.begin(), sortedMovable.end(), CompareSelCells());
        auto *mc = sortedMovable.back();
        sortedMovable.erase(sortedMovable.end() - 1);
        bool zoeblocked = false;
        int nx[4], ny[4], ncnt = 0;
        for (int i = 0; i < 4; ++i) {
            int tx, ty;
            switch (i) {
            case 0:
                if (mc->y <= 0) { continue; }
                tx = mc->x;
                ty = mc->y - 1;
                break;
            case 1:
                if (mc->x + 1 >= w) { continue; }
                tx = mc->x + 1;
                ty = mc->y;
                break;
            case 2:
                if (mc->x <= 0) { continue; }
                tx = mc->x - 1;
                ty = mc->y;
                break;
            default:
                if (mc->y + 1 >= h) { continue; }
                tx = mc->x;
                ty = mc->y + 1;
                break;
            }
            if (zoecheck) {
                auto &ci = cellInfo_[ty * w + tx];
                if (ci.charInfo && ci.charInfo->side == myside) {
                    zoeblocked = true;
                    break;
                }
            }
            nx[ncnt] = tx;
            ny[ncnt] = ty;
            ++ncnt;
        }
        if (zoeblocked) { continue; }
        for (int i = 0; i < ncnt; ++i) {
            int tx = nx[i], ty = ny[i];
            auto &ci = cellInfo_[ty * w + tx];
            if (ci.charInfo || ci.blocked) {
                continue;
            }
            auto currMove = mc->moves + 1;
            auto ite = selCells.find(std::make_pair(tx, ty));
            if (ite == selCells.end()) {
                auto &mcell = selCells[std::make_pair(tx, ty)];
                mcell.x = tx;
                mcell.y = ty;
                mcell.moves = currMove;
                mcell.moveParent = mc;
                if (currMove < steps) {
                    sortedMovable.push_back(&mcell);
                    std::push_heap(sortedMovable.begin(), sortedMovable.end(), CompareSelCells());
                }
            }
        }
    }
    if (ranges) {
        struct CompareRangeCells {
            bool operator()(const SelectableCell *a, const SelectableCell *b) {
                return a->ranges > b->ranges;
            }
        };
        std::vector<SelectableCell*> sortedAttackable;
        sortedAttackable.reserve(selCells.size());
        for (auto &p: selCells) {
            sortedAttackable.push_back(&p.second);
        }
        std::make_heap(sortedAttackable.begin(), sortedAttackable.end(), CompareRangeCells());
        while (!sortedAttackable.empty()) {
            std::pop_heap(sortedAttackable.begin(), sortedAttackable.end(), CompareRangeCells());
            auto *mc = sortedAttackable.back();
            sortedAttackable.erase(sortedAttackable.end() - 1);
            int nx[4], ny[4], ncnt = 0;
            for (int i = 0; i < 4; ++i) {
                int tx, ty;
                switch (i) {
                case 0:
                    if (mc->x <= 0) { continue; }
                    tx = mc->x - 1;
                    ty = mc->y;
                    break;
                case 1:
                    if (mc->y <= 0) { continue; }
                    tx = mc->x;
                    ty = mc->y - 1;
                    break;
                case 2:
                    if (mc->x + 1 >= w) { continue; }
                    tx = mc->x + 1;
                    ty = mc->y;
                    break;
                default:
                    if (mc->y + 1 >= h) { continue; }
                    tx = mc->x;
                    ty = mc->y + 1;
                    break;
                }
                nx[ncnt] = tx;
                ny[ncnt] = ty;
                ++ncnt;
            }
            for (int i = 0; i < ncnt; ++i) {
                int tx = nx[i], ty = ny[i];
                auto &ci = cellInfo_[ty * w + tx];
                if (ci.blocked) {
                    continue;
                }
                auto currRange = mc->ranges + 1;
                auto ite = selCells.find(std::make_pair(tx, ty));
                if (ite == selCells.end()) {
                    auto &mcell = selCells[std::make_pair(tx, ty)];
                    mcell.x = tx;
                    mcell.y = ty;
                    mcell.moves = -1;
                    mcell.ranges = currRange;
                    mcell.rangeParent = mc;
                    if (currRange < ranges) {
                        sortedAttackable.push_back(&mcell);
                        std::push_heap(sortedAttackable.begin(), sortedAttackable.end(), CompareRangeCells());
                    }
                }
            }
        }
    }
}

class DirectionSelMessageBox: public MessageBox {
public:
    using MessageBox::MessageBox;

    void setDirectionHandler(const std::function<void(Map::Direction)> &func) {
        directionHandler_ = func;
    }
    void handleKeyInput(Key key) override {
        switch (key) {
        case KeyUp:
            directionHandler_(Map::DirUp);
            delete this;
            break;
        case KeyLeft:
            directionHandler_(Map::DirLeft);
            delete this;
            break;
        case KeyRight:
            directionHandler_(Map::DirRight);
            delete this;
            break;
        case KeyDown:
            directionHandler_(Map::DirDown);
            delete this;
            break;
        case KeyCancel: {
            auto fn = std::move(closeHandler_);
            delete this;
            if (fn) { fn(); }
            break;
        }
        default:
            break;
        }
    }

private:
    std::function<void(Map::Direction)> directionHandler_;
};

bool Warfield::tryUseSkill(int index) {
    auto *ch = charQueue_.back();
    if (index < 0) {
        actIndex_ = -1;
        actId_ = index;
        actLevel_ = 0;
        attackTimesLeft_ = 1;
        int steps;
        switch (index) {
        case -3:
            steps = ch->info.poison / 15;
            break;
        case -2:
            steps = ch->info.depoison / 15;
            break;
        case -1:
            steps = ch->info.medic / 15;
            break;
        default:
            steps = 1;
            break;
        }
        maskSelectableArea(0, steps);
        stage_ = AttackSelecting;
        drawDirty_ = true;
        return true;
    }
    const auto *skill = mem::gSaveData.skillInfo[std::max<std::int16_t>(ch->info.skillId[index], 0)];
    if (!skill) { return false; }
    auto skillLevel = std::clamp<std::int16_t>(ch->info.skillLevel[index] / 100, 0, 9);
    skillLevel = mem::calcRealSkillLevel(skill->reqMp, skillLevel, ch->info.mp);
    if (skillLevel < 0) { return false; }
    actIndex_ = index;
    actId_ = ch->info.skillId[index];
    attackTimesLeft_ = ch->info.doubleAttack ? 2 : 1;
    actLevel_ = skillLevel;
    switch (skill->attackAreaType) {
    case 1: {
        auto msgBox = new DirectionSelMessageBox(this, 0, 0, gWindow->width(), gWindow->height());
        msgBox->popup({GETTEXT(92)});
        msgBox->setCloseHandler([this]() {
            playerMenu();
        });
        msgBox->setDirectionHandler([this, ch](Map::Direction direction) {
            ch->direction = direction;
            startActAction();
        });
        return true;
    }
    case 2:
        startActAction();
        return true;
    default:
        maskSelectableArea(0, skill->selRange[actLevel_]);
        stage_ = AttackSelecting;
        drawDirty_ = true;
        return true;
    }
}

void Warfield::startActAction() {
    popupNumbers_.clear();
    if (actId_ < 0) {
        auto *target = cellInfo_[cursorY_ * mapWidth_ + cursorX_].charInfo;
        if (!target) {
            playerMenu();
            return;
        }
        auto *ch = charQueue_.back();
        std::int16_t result;
        std::uint8_t r, g, b;
        auto *ttf = renderer_->ttf();
        bool popup;
        switch (actId_) {
        case -3:
            effectId_ = data::PoisonEffectID;
            popup = target && ch->side != target->side;
            result = popup ? mem::actPoison(&ch->info, &target->info, 2) : 0;
            popup = popup && result != 0;
            r = 96; g = 176; b = 64;
            break;
        case -2:
            effectId_ = data::DepoisonEffectID;
            popup = target && ch->side == target->side;
            result = popup ? mem::actDepoison(&ch->info, &target->info, 2) : 0;
            r = 104; g = 192; b = 232;
            break;
        case -1:
            effectId_ = data::MedicEffectID;
            popup = target && ch->side == target->side;
            result = popup ? mem::actMedic(&ch->info, &target->info, 4) : 0;
            r = 236; g = 200; b = 40;
            break;
        default: {
            const auto *itemInfo = mem::gSaveData.itemInfo[actIndex_];
            effectId_ = itemInfo ? itemInfo->throwingEffectId : data::PoisonEffectID;
            popup = target && ch->side != target->side;
            bool dead = false;
            result = popup ? mem::actThrow(&ch->info, &target->info, actIndex_, 0, dead) : 0;
            if (popup) {
                mem::gBag.remove(actIndex_, 1);
            }
            popup = popup && result != 0;
            if (dead) {
                recalcKnowledge();
            }
            r = 232; g = 32; b = 44;
            break;
        }
        }
        if (popup) {
            if (result != 0) { ch->exp += std::abs(result); }
            auto txt = fmt::format(L"{:+}", result);
            popupNumbers_.emplace_back(PopupNumber{txt, cursorX_, cursorY_, r, g, b});
        }
        stage_ = Acting;
        if (cameraX_ != cursorX_ || cameraY_ != cursorY_) {
            ch->direction = calcDirection(cameraX_, cameraY_, cursorX_, cursorY_);
        }
        fightTex_ = ch->info.headId >= 0 && ch->info.headId < fightTexData_.size()
            ? &fightTexData_[ch->info.headId] : nullptr;
        fightTexCount_ = ch->info.frame[0];
        fightTexIdx_ = fightTexCount_ * int(ch->direction);
        fightTexCount_ += fightTexIdx_;
        effectTexIdx_ = -ch->info.frameDelay[0];
        fightFrame_ = -ch->info.frameSoundDelay[0];
        return;
    }
    const auto *skillInfo = mem::gSaveData.skillInfo[actId_];
    if (skillInfo) {
        bool levelup = false;
        effectId_ = skillInfo->effectId;
        auto skillType = skillInfo->skillType;
        stage_ = Acting;
        auto *ch = charQueue_.back();
        if ((skillInfo->attackAreaType == 0 || skillInfo->attackAreaType == 3)
            && (cameraX_ != cursorX_ || cameraY_ != cursorY_)) {
            ch->direction = calcDirection(cameraX_, cameraY_, cursorX_, cursorY_);
        }
        fightTex_ = ch->info.headId >= 0 && ch->info.headId < fightTexData_.size()
                    ? &fightTexData_[ch->info.headId] : nullptr;
        fightTexIdx_ = 0;
        for (std::int16_t i = 0; i < skillType; ++i) {
            fightTexIdx_ += 4 * ch->info.frame[i];
        }
        fightTexCount_ = ch->info.frame[skillType];
        fightTexIdx_ += fightTexCount_ * int(ch->direction);
        fightTexCount_ += fightTexIdx_;
        effectTexIdx_ = -ch->info.frameDelay[skillType];
        fightFrame_ = -ch->info.frameSoundDelay[skillType];

        switch (skillInfo->attackAreaType) {
        case 1: {
            auto sx = cameraX_, sy = cameraY_;
            int r = skillInfo->selRange[actLevel_];
            for (int i = r; i; --i) {
                switch (ch->direction) {
                case Map::DirUp:
                    if (sy >= i) { makeDamage(ch, sx, sy - i, i); }
                    break;
                case Map::DirRight:
                    if (sx + i < mapWidth_) { makeDamage(ch, sx + i, sy, i); }
                    break;
                case Map::DirLeft:
                    if (sx >= i) { makeDamage(ch, sx - i, sy, i); }
                    break;
                case Map::DirDown:
                    if (sy + i < mapHeight_) { makeDamage(ch, sx, sy + i, i); }
                    break;
                default:
                    break;
                }
            }
            break;
        }
        case 2: {
            auto sx = cameraX_, sy = cameraY_;
            int r = skillInfo->selRange[actLevel_];
            for (int i = r; i; --i) {
                if (sy >= i) { makeDamage(ch, sx, sy - i, i); }
                if (sx + i < mapWidth_) { makeDamage(ch, sx + i, sy, i); }
                if (sx >= i) { makeDamage(ch, sx - i, sy, i); }
                if (sy + i < mapHeight_) { makeDamage(ch, sx, sy + i, i); }
            }
            break;
        }
        case 3: {
            auto sx = cursorX_, sy = cursorY_;
            int r = skillInfo->area[actLevel_];
            int baseDistance = std::abs(sx - cameraX_) + std::abs(sy - cameraY_);
            for (int j = -r; j <= r; ++j) {
                auto ry = sy + j;
                if (ry < 0 || ry >= mapHeight_) { continue; }
                for (int i = -r; i <= r; ++i) {
                    auto rx = sx + i;
                    if (rx < 0 || rx >= mapWidth_) { continue; }
                    makeDamage(ch, rx, ry, baseDistance + std::abs(i) + std::abs(j));
                }
            }
            break;
        }
        default: {
            int x = cursorX_, y = cursorY_;
            makeDamage(ch, x, y, std::abs(x - cameraX_) + std::abs(y - cameraY_));
            break;
        }
        }
        mem::postDamage(&ch->info, actIndex_, attackTimesLeft_ == 1 ? 3 : 0, skillLevelup_);
        if (skillLevelup_) {
            actLevel_ = std::clamp<std::int16_t>(ch->info.skillLevel[actIndex_] / 100, 0, 9);
        }
    } else {
        endTurn();
    }
}

void Warfield::makeDamage(Warfield::CharInfo *ch, int x, int y, int distance) {
    auto *info = cellInfo_[y * mapWidth_ + x].charInfo;
    if (!info || info->side == ch->side) { return; }
    auto &enemyInfo = info->info;
    std::int16_t dmg, ps;
    bool dead = false;
    bool wasDead = enemyInfo.hp <= 0;
    if (mem::actDamage(&ch->info, &enemyInfo, knowledge_[0], knowledge_[1],
                       distance, actIndex_, actLevel_, dmg, ps, dead)) {
        if (!wasDead && dead) {
            ch->exp += dmg * 2 / 3;
            recalcKnowledge();
        } else {
            ch->exp += dmg / 3;
        }
        auto *ttf = renderer_->ttf();
        if (dmg < 0) {
            auto txt = fmt::format(L"{:+}", dmg);
            popupNumbers_.emplace_back(PopupNumber{txt, x, y, 112, 12, 112});
        } else {
            auto txt = fmt::format(L"{:+}", -dmg);
            popupNumbers_.emplace_back(PopupNumber{txt, x, y, 232, 32, 44});
        }
    }
}

void Warfield::doRest() {
    auto *ch = charQueue_.back();
    mem::actRest(&ch->info);
    endTurn();
}

void Warfield::endTurn() {
    charQueue_.pop_back();
    int aliveCount[2] = {0, 0};
    for (auto &ci: chars_) {
        if (ci.info.hp > 0) {
            ++aliveCount[ci.side];
        } else if (ci.x > 0) {
            cellInfo_[ci.x + ci.y * mapWidth_].charInfo = nullptr;
            ci.x = ci.y = -1;
            drawDirty_ = true;
        }
    }
    if (aliveCount[1] == 0) {
        won_ = true;
        endWar();
        return;
    }
    if (aliveCount[0] == 0) {
        won_ = false;
        endWar();
        return;
    }
    stage_ = Idle;
}

void Warfield::endWar() {
    removeAllChildren();
    std::vector<CharInfo*> alives;
    for (auto &ci: chars_) {
        if (ci.side != 0) { continue; }
        auto *charInfo = mem::gSaveData.charInfo[ci.id];
        if (!charInfo) { continue; }
        charInfo->hp = std::max<std::int16_t>(1, ci.info.hp);
        charInfo->mp = ci.info.mp;
        charInfo->poisoned = ci.info.poisoned;
        charInfo->hurt = ci.info.hurt;
        charInfo->stamina = ci.info.stamina;
        for (int i = 0; i < data::LearnSkillCount; ++i) {
            if (ci.info.skillId[i] <= 0) { continue; }
            charInfo->skillLevel[i] = ci.info.skillLevel[i];
        }
        if (ci.info.hp > 0) { alives.push_back(&ci); }
    }
    const auto *info = data::gWarfieldData.info(warId_);
    auto wexp = info != nullptr ? info->exp : 0;
    std::vector<std::pair<int, std::wstring>> messages = { {0, GETTEXT(won_ ? 93 : 94) } };
    if (won_ || getExpOnLose_) {
        for (auto *ch: alives) {
            ch->exp += wexp / int(alives.size());
            auto *charInfo = mem::gSaveData.charInfo[ch->id];
            if (!charInfo) { continue; }
            auto name = GETCHARNAME(ch->id);
            messages.emplace_back(std::make_pair(0, fmt::format(GETTEXT(95), name, ch->exp)));
            bool canLearn = false, makingItem = false;
            std::int16_t skillId = 0;
            int skillIndex = -1, skillLevel = 0;
            const mem::ItemData *itemInfo = nullptr;
            if (charInfo->learningItem >= 0) {
                itemInfo = mem::gSaveData.itemInfo[charInfo->learningItem];
                if (itemInfo) {
                    makingItem = itemInfo->makeItem[0] >= 0;
                    canLearn = true;
                    skillId = itemInfo->skillId;
                    if (skillId > 0) {
                        for (int i = 0; i < data::LearnSkillCount; ++i) {
                            if (skillIndex < 0 && charInfo->skillId[i] <= 0) {
                                skillIndex = i;
                                continue;
                            }
                            if (charInfo->skillId[i] == skillId) {
                                skillIndex = i;
                                skillLevel = std::clamp<std::int16_t>(charInfo->skillLevel[i] / 100, 0, 9);
                                if (skillLevel >= 9) {
                                    canLearn = false;
                                }
                                break;
                            }
                        }
                    }
                }
            }
            int exp, exp2;
            if (charInfo->level >= data::LevelMax) {
                exp = 0;
                exp2 = ch->exp;
            } else {
                if (canLearn) {
                    exp = exp2 = ch->exp / 2;
                } else {
                    exp = ch->exp;
                    exp2 = 0;
                }
            }
            if (exp) {
                charInfo->exp = std::clamp<int>(int(charInfo->exp) + exp, 0, data::ExpMax);
                std::uint16_t expReq;
                bool levelup = false;
                while ((expReq = mem::getExpForLevelUp(charInfo->level)) > 0 && charInfo->exp >= expReq) {
                    levelup = true;
                    mem::actLevelup(charInfo);
                }
                if (levelup) {
                    messages.emplace_back(std::make_pair(0, fmt::format(GETTEXT(96), name)));
                }
            }
            if (exp2 && canLearn) {
                charInfo->expForItem = std::clamp<int>(int(charInfo->expForItem) + exp2, 0, data::ExpMax);
                int newlevel = skillLevel;
                bool levelup = false;
                for (;;) {
                    auto expReq = mem::getExpForSkillLearn(charInfo->learningItem, newlevel, charInfo->potential);
                    if (expReq <= 0 || charInfo->expForItem < expReq) {
                        break;
                    }
                    levelup = true;
                    charInfo->expForItem -= expReq;
                    if (itemInfo) {
                        std::map<mem::PropType, std::int16_t> changes;
                        mem::applyItemChanges(charInfo, itemInfo, changes);
                        const mem::SkillData *skillInfo;
                        if (skillId >= 0 && (skillInfo = mem::gSaveData.skillInfo[skillId]) != nullptr) {
                            auto addMp = skillInfo->addMp[newlevel];
                            if (addMp) {
                                charInfo->maxMp = std::clamp<std::int16_t>(
                                    charInfo->maxMp + util::gRandom(1, addMp / 2), 0, data::MpMax);
                            }
                        }
                    }
                    if (skillIndex >= 0) {
                        if (charInfo->skillId[skillIndex] <= 0) {
                            charInfo->skillId[skillIndex] = skillId;
                            charInfo->skillLevel[skillIndex] = 0;
                        } else {
                            newlevel = charInfo->skillLevel[skillIndex] / 100 + 1;
                            charInfo->skillLevel[skillIndex] = newlevel * 100;
                        }
                    }
                }
                if (levelup) {
                    messages.emplace_back(std::make_pair(0, fmt::format(GETTEXT(97), name,
                                                                        GETITEMNAME(charInfo->learningItem))));
                    if (newlevel > 0) {
                        messages.emplace_back(std::make_pair(1, fmt::format(GETTEXT(98), GETSKILLNAME(skillId),
                                                                            newlevel + 1)));
                    }
                }
            }
            if (makingItem) {
                charInfo->expForMakeItem += ch->exp;
                if (charInfo->expForMakeItem >= itemInfo->reqExpForMakeItem && mem::gBag[itemInfo->reqMaterial] > 0) {
                    int count = 0;
                    while (count < data::MakeItemCount) {
                        if (itemInfo->makeItem[count] < 0) { break; }
                        ++count;
                    }
                    charInfo->expForMakeItem = 0;
                    mem::gBag.remove(itemInfo->reqMaterial, 1);
                    auto index = util::gRandom(count);
                    mem::gBag.add(itemInfo->makeItem[index], itemInfo->makeItemCount[index]);
                    messages.emplace_back(std::make_pair(0, fmt::format(GETTEXT(99),
                                                                        name, GETITEMNAME(itemInfo->makeItem[index]))));
                }
            }
        }
    }
    stage_ = Finished;
    popupFinishMessages(std::move(messages), 0);
    delete statusPanel_;
    statusPanel_ = nullptr;
}

void Warfield::popupFinishMessages(std::vector<std::pair<int, std::wstring>> messages, int index) {
    int y = height_ / 3;
    auto *msgBox = new MessageBox(this, 0, y, width_, 60);
    msgBox->popup({messages[index].second}, MessageBox::PressToCloseThis);
    ++index;
    auto *lastMsgBox = msgBox;
    while (index < messages.size() && messages[index].first > 0) {
        auto *msgBox2 = new MessageBox(msgBox, 0, y + 60 * messages[index].first, width_, 60);
        msgBox2->popup({messages[index].second}, MessageBox::PressToCloseParent);
        lastMsgBox = msgBox2;
        ++index;
    }
    lastMsgBox->setCloseHandler([this, messages = std::move(messages), index]() {
        if (index < messages.size()) {
            popupFinishMessages(messages, index);
        } else {
            cleanup();
            gWindow->endWar(won_, !won_ && deadOnLose_);
        }
    });
}

}
