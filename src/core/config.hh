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

#include <string>
#include <vector>

namespace hojy::core {

class Config {
public:
    bool load(const std::string &filename);
    [[nodiscard]] bool saveOptions(const std::string &filename) const;
    bool postLoad();
    void fixOnTextLoaded();

    [[nodiscard]] std::string dataFilePath(const std::string &filename) const;

    [[nodiscard]] std::string musicFilePath(const std::string &filename) const;
    [[nodiscard]] std::string soundFilePath(const std::string &filename) const;
    [[nodiscard]] std::string saveFilePath(const std::string &filename) const;

    [[nodiscard]] const std::vector<std::string> &dataPath() const { return dataPath_; }
    [[nodiscard]] const std::vector<std::string> &fonts() const { return fonts_; }
    [[nodiscard]] const std::string &musicPath() const { return musicPath_; }
    [[nodiscard]] const std::string &soundPath() const { return soundPath_; }
    [[nodiscard]] const std::string &savePath() const { return savePath_; }

    [[nodiscard]] bool shipLogicEnabled() const { return shipLogicEnabled_; }

    [[nodiscard]] int windowWidth() const { return windowWidth_; }
    [[nodiscard]] int windowHeight() const { return windowHeight_; }

    [[nodiscard]] bool simplifiedChinese() const { return simplifiedChinese_; }
    [[nodiscard]] bool showPotential() const { return showPotential_; }
    [[nodiscard]] bool showMapMiniPanel() const { return showMapMiniPanel_; }
    void setShowMapMiniPanel(bool show) { showMapMiniPanel_ = show; }
    [[nodiscard]] bool showMinimap() const { return showMinimap_; }
    void setShowMinimap(bool show) { showMinimap_ = show; }
    [[nodiscard]] std::pair<int, int> scale() const { return scale_; }
    [[nodiscard]] float animationSpeed() const { return animationSpeed_; }
    [[nodiscard]] float fadeSpeed() const { return fadeSpeed_; }
    [[nodiscard]] int windowBorder() const { return windowBorder_; }
    [[nodiscard]] bool noNameInput() const { return noNameInput_; }
    [[nodiscard]] const std::wstring &defaultName() const { return defaultName_; }

    [[nodiscard]] bool showFPS() const { return showFPS_; }
    [[nodiscard]] int limitFPS() const { return limitFPS_; }

    [[nodiscard]] int sampleRate() const { return sampleRate_; }
    [[nodiscard]] int sampleFormat() const { return sampleFormat_; }

    [[nodiscard]] int musicVolume() const { return musicVolume_; }
    void setMusicVolume(int volume) { musicVolume_ = volume; }
    [[nodiscard]] int soundVolume() const { return soundVolume_; }
    void setSoundVolume(int volume) { soundVolume_ = volume; }

private:
    std::vector<std::string> dataPath_, fonts_;
    std::string musicPath_, soundPath_, savePath_, prePath_;
    bool shipLogicEnabled_ = true;
    int windowWidth_ = 640, windowHeight_ = 480;
    bool simplifiedChinese_ = false;
    bool showPotential_ = false;
    bool showMapMiniPanel_ = true;
    bool showMinimap_ = true;
    std::pair<int, int> scale_ = {2, 1};
    float animationSpeed_ = 1.f;
    float fadeSpeed_ = 1.f;
    int windowBorder_ = 8;
    bool noNameInput_ = false;
    std::wstring defaultName_;
    bool showFPS_ = false;
    int limitFPS_ = 0;
    int sampleRate_ = 0;
    int sampleFormat_ = 0;
    int musicVolume_ = 5;
    int soundVolume_ = 5;
};

extern Config config;

}// namespace hojy::core
