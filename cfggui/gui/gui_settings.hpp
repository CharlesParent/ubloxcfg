/* ************************************************************************************************/ // clang-format off
// flipflip's cfggui
//
// Copyright (c) 2021 Philippe Kehl (flipflip at oinkzwurgl dot org),
// https://oinkzwurgl.org/hacking/ubloxcfg
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

#ifndef __GUI_SETTINGS_HPP__
#define __GUI_SETTINGS_HPP__

#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <cstdint>

#include "ff_epoch.h"
#include "ff_conffile.hpp"
#include "mapparams.hpp"
#include "imgui.h"
#include "implot.h"

#include "gui_settings_colours.hpp"

/* ****************************************************************************************************************** */

#define _SETTINGS_COLOUR_ENUM(_str, _enum, _col) _enum,

class GuiSettings
{
    public:

        // ---- Main control instance -----

        GuiSettings();
       ~GuiSettings();

        void LoadConf(const std::string &file);
        void SaveConf(const std::string &file);

        bool UpdateFonts();
        bool UpdateSizes();
        void DrawSettingsEditor();

        // ---- Globally available functions and variables
        //      (read-only, resp. set/updated at run-time by the control instance ) -----

        // Generic settings storage
        static void SetValue(const std::string &key, const bool     value);
        static void SetValue(const std::string &key, const int      value);
        static void SetValue(const std::string &key, const uint32_t value);
        static void SetValue(const std::string &key, const float    value);
        static void SetValue(const std::string &key, const double   value);
        static void SetValue(const std::string &key, const std::string &value);
        static void SetValueMult(const std::string &key, const std::vector<std::string> &list, const int maxNum);
        static void SetValueList(const std::string &key, const std::vector<std::string> &list, const std::string &sep, const int maxNum);
        static void GetValue(const std::string &key, bool     &value, const bool     def);
        static void GetValue(const std::string &key, int      &value, const int      def);
        static void GetValue(const std::string &key, uint32_t &value, const uint32_t def);
        static void GetValue(const std::string &key, float    &value, const float    def);
        static void GetValue(const std::string &key, double   &value, const double   def);
        static void GetValue(const std::string &key, std::string &value, const std::string &def);
        static std::string GetValue(const std::string &key);
        static std::vector<std::string> GetValueMult(const std::string &key, const int maxNum);
        static std::vector<std::string> GetValueList(const std::string &key, const std::string &sep, const int maxNum);

        // List of recent things
        static constexpr int MAX_RECENT = 20;
        static constexpr const char *RECENT_RECEIVERS     = "Receiver";
        static constexpr const char *RECENT_LOGFILES      = "Logfile";
        static constexpr const char *RECENT_NTRIP_CASTERS = "NtripCaster";
        static void LoadRecentItems(const std::string &name);
        static void SaveRecentItems(const std::string &name);
        static void AddRecentItem(const std::string &name, const std::string &input);
        static void ClearRecentItems(const std::string &name);
        static const std::vector<std::string> &GetRecentItems(const std::string &name);

        // Font
        static float     fontSize;    // Current font size (default font)
        static float     lineHeight;  // Current line hight (default font)
        static ImFont   *fontMono;    // Default monospace font
        static ImFont   *fontSans;    // Alternative sans-serif font
        static ImFont   *fontBold;    // Alternative sans-serif font (bold)
        static ImFont   *fontOblique; // Alternative sans-serif font (italics)
        static FfVec2f    charSize; // Char size (of a fontMono character)
        static FfVec2f    iconSize; // Size for ImGui::Button() with just an icon (ICON_FK_...)

        // Colours
        enum Colour_e : int { _DUMMY = -1, GUI_SETTINGS_COLOURS(_SETTINGS_COLOUR_ENUM) _NUM_COLOURS };
        static ImU32 colours[_NUM_COLOURS]; // See GUI_SETTINGS_COLOURS (and GUI_COLOUR_NONE), use GUI_COLOUR() macro to access
        static ImVec4 colours4[_NUM_COLOURS];
        static ImU32 GetFixColour(const EPOCH_t *epoch); // GNSS fix colour
        static const ImVec4 &GetFixColour4(const EPOCH_t *epoch); // GNSS fix colour

        // Paths
        static std::string cachePath;

        // Maps
        static std::vector<MapParams> maps;

        // Helpers
        static const ImGuiStyle   *style;     // Shortcut to ImGui styles
        static const ImPlotStyle  *plotStyle; // Shortcut to ImPlot styles

    private:

        // For DrawSettingsEditor(), LoadConf(), SaveConf()
        static const char * const COLOUR_LABELS[_NUM_COLOURS];
        static const char * const COLOUR_NAMES[_NUM_COLOURS];
        static ImU32              COLOUR_DEFAULTS[_NUM_COLOURS];

        static constexpr float    FONT_SIZE_DEF = 13.0;
        static constexpr float    FONT_SIZE_MIN = 10.0;
        static constexpr float    FONT_SIZE_MAX = 30.0;

        bool                      _fontDirty;
        bool                      _sizesDirty;
        float                     _widgetOffs;

        static std::map<std::string, std::vector<std::string>> _recentItems;

        // Freetype2 config
        uint32_t                  _ftBuilderFlags;
        float                     _ftRasterizerMultiply;
        static constexpr uint32_t FT_BUILDER_FLAGS_DEF = (1 << 2); // ImGuiFreeTypeBuilderFlags_ForceAutoHint
        static constexpr float    FT_RASTERIZER_MULTIPLY_DEF = 1.0f;
        static constexpr float    FT_RASTERIZER_MULTIPLY_MIN = 0.2f;
        static constexpr float    FT_RASTERIZER_MULTIPLY_MAX = 2.0f;

        static Ff::ConfFile       _confFile;
        bool                      _clearSettingsOnExit;
};

#define GUI_COLOUR(which) GuiSettings::colours[GuiSettings::which]
#define GUI_COLOUR4(which) GuiSettings::colours4[GuiSettings::which]
#define GUI_COLOUR_NONE   IM_COL32(0x00, 0x00, 0x00, 0x00)


/* ****************************************************************************************************************** */
#endif // __GUI_SETTINGS_HPP__
