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

#pragma once

#include "serializable.hh"

namespace hojy::mem {

enum {
    SubMapWidth = 64,
    SubMapHeight = 64,
    SubMapLayerCount = 6,
    SubMapEventCount = 200,
};

#pragma pack(push, 1)
struct SubMapData {
    std::int16_t id;
    char name[10];
    std::int16_t exitMusic, enterMusic;
    std::int16_t jumpSubMap, enterCondition;
    std::int16_t globalEnterX1, globalEnterY1, globalEnterX2, globalEnterY2;
    std::int16_t enterX, enterY;
    std::int16_t exitX[3], exitY[3];
    std::int16_t jumpX, jumpY, jumpReturnX, jumpReturnY;
} ATTR_PACKED;

struct SubMapEvent {
    std::int16_t blocked, index, event[3], currTex, endTex, begTex, texDelay, x, y;
};

struct SubMapLayerData {
    std::int16_t data[SubMapLayerCount][SubMapWidth * SubMapHeight];
};

struct SubMapEventData {
    SubMapEvent events[SubMapEventCount];
};
#pragma pack(pop)

using SubMapInfo = SerializableStructVec<SubMapData>;
using SubMapLayerInfo = SerializableStruct<SubMapLayerData>;
using SubMapEventInfo = SerializableStruct<SubMapEventData>;

}