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

// #include <cstdint>
// #include <cstring>
// #include <functional>

// #include "ff_stuff.h"
// #include "ff_ubx.h"
// #include "ff_port.h"
// #include "ff_trafo.h"
// #include "ff_cpp.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "IconsForkAwesome.h"

// #include "platform.hpp"

#include "gui_win_input_logfile.hpp"

/* ****************************************************************************************************************** */

GuiWinInputLogfile::GuiWinInputLogfile(const std::string &name) :
    GuiWinInput(name),
    _fileDialog{_winName + "OpenLogFileDialog"}
{
    DEBUG("GuiWinInputLogfile(%s)", _winName.c_str());

    _winIniPos = POS_NW;
    _winSize   = { 80, 25 };
    SetTitle("Logfile X");

    _dataWinCaps = DataWinDef::Cap_e::PASSIVE;

    _logfile = std::make_shared<Logfile>(name, _database);
    _logfile->SetDataCb( std::bind(&GuiWinInputLogfile::_ProcessData, this, std::placeholders::_1) );

    _limitPlaySpeed = true;
    _playSpeed = _logfile->GetPlaySpeed();

    if (_recentLogs.empty())
    {
        for (int n = 1; n <= MAX_RECENT_LOGS; n++)
        {
            const std::string key = _winName + ".RecentLog" + std::to_string(n);
            const std::string val = _winSettings->GetValue(key);
            if (!val.empty())
            {
                _recentLogs.push_back(val);
            }
        }
    }

    _ClearData();
};

// ---------------------------------------------------------------------------------------------------------------------

GuiWinInputLogfile::~GuiWinInputLogfile()
{
    DEBUG("~GuiWinInputLogfile(%s)", _winName.c_str());

    int ix = 0;
    for (const auto &path: _recentLogs)
    {
        const std::string key = _winName + ".RecentLog" + std::to_string(++ix);
        _winSettings->SetValue(key, path);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::Loop(const uint32_t &frame, const double &now)
{
    UNUSED(frame);

    // Pump receiver events and dispatch
    _logfile->Loop(now);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_ProcessData(const Data &data)
{
    GuiWinInput::_ProcessData(data);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_ClearData()
{
    GuiWinInput::_ClearData();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_AddDataWindow(std::unique_ptr<GuiWinData> dataWin)
{
    dataWin->SetLogfile(_logfile);
    _dataWindows.push_back(std::move(dataWin));
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_DrawActionButtons()
{
    GuiWinInput::_DrawActionButtons();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_DrawControls()
{
    const bool canOpen  = _logfile->CanOpen();
    const bool canClose = _logfile->CanClose();
    const bool canPlay  = _logfile->CanPlay();
    const bool canStop  = _logfile->CanStop();
    const bool canPause = _logfile->CanPause();
    const bool canStep  = _logfile->CanStep();
    const bool canSeek  = _logfile->CanSeek();

    // Open log
    {
        ImGui::BeginDisabled(!canOpen);

        if (ImGui::Button(ICON_FK_FOLDER_OPEN "##Open", _winSettings->iconButtonSize))
        {
            if (!_fileDialog.IsInit())
            {
                _fileDialog.InitDialog(GuiWinFileDialog::FILE_OPEN);
                _fileDialog.SetTitle(_winTitle + " - Open logfile...");
            }
            else
            {
                _fileDialog.Focus();
            }
        }

        Gui::ItemTooltip("Open logfile");

        ImGui::SameLine(0, 0);

        if (ImGui::BeginCombo("##RecentLogs", NULL, ImGuiComboFlags_HeightLarge | ImGuiComboFlags_NoPreview))
        {
            std::lock_guard<std::mutex> lock(_recentLogsMutex);
            if (_recentLogs.empty())
            {
                ImGui::TextUnformatted("No recent logs");
            }
            for (auto iter = _recentLogs.crbegin(); iter != _recentLogs.crend(); iter++)
            {
                if (ImGui::Selectable((*iter).c_str()))
                {
                    _logfile->Open(*iter);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::EndDisabled();

        // Handle file dialog, and log open
        if (_fileDialog.IsInit())
        {
            if (_fileDialog.DrawDialog())
            {
                const auto path = _fileDialog.GetPath();
                if (!path.empty() && _logfile->Open(path))
                {
                    _AddRecentLog(path);
                }
            }
        }
    }

    ImGui::SameLine();

    // Close log
    {
        ImGui::BeginDisabled(!canClose);
        if (ImGui::Button(ICON_FK_EJECT "##Close", _winSettings->iconButtonSize))
        {
            _logfile->Close();
        }
        ImGui::EndDisabled();
        Gui::ItemTooltip("Close logfile");
    }

    ImGui::SameLine();

    // Play
    {
        ImGui::BeginDisabled(!canPlay);
        if (ImGui::Button(ICON_FK_PLAY "##Play", _winSettings->iconButtonSize))
        {
            _logfile->Play();
        }
        Gui::ItemTooltip("Play");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Pause
    {
        ImGui::BeginDisabled(!canPause);
        if (ImGui::Button(ICON_FK_PAUSE "##Pause", _winSettings->iconButtonSize))
        {
            _logfile->Pause();
        }
        Gui::ItemTooltip("Pause");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Stop
    {
        ImGui::BeginDisabled(!canStop);
        if (ImGui::Button(ICON_FK_STOP "##Stop", _winSettings->iconButtonSize))
        {
            _logfile->Stop();
            _ClearData();
        }
        Gui::ItemTooltip("Stop");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Step epoch
    {
        ImGui::BeginDisabled(!canStep);
        if (ImGui::Button(ICON_FK_FORWARD "##StepEpoch", _winSettings->iconButtonSize))
        {
            _logfile->StepEpoch();
        }
        Gui::ItemTooltip("Step epoch");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Step message
    {
        ImGui::BeginDisabled(!canStep);
        if (ImGui::Button(ICON_FK_STEP_FORWARD "##StepMsg", _winSettings->iconButtonSize))
        {
            _logfile->StepMsg();
        }
        Gui::ItemTooltip("Step message");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Play speed
    {
        if (ImGui::Checkbox("##SpeedLimit", &_limitPlaySpeed))
        {
            _logfile->SetPlaySpeed(_limitPlaySpeed ? _playSpeed : Logfile::PLAYSPEED_INF);
        }
        Gui::ItemTooltip("Limit play speed");

        ImGui::SameLine(0,0);

        ImGui::BeginDisabled(!_limitPlaySpeed);
        ImGui::SetNextItemWidth(_winSettings->charSize.x * 6);
        if (ImGui::DragFloat("##Speed", &_playSpeed, 0.1f, Logfile::PLAYSPEED_MIN, Logfile::PLAYSPEED_MAX,
            _limitPlaySpeed ? "%.1f" : "inf", ImGuiSliderFlags_AlwaysClamp))
        /*if (!ImGui::IsItemActive()) { Gui::ItemTooltip("Play speed [epochs/s]"); }
        if (ImGui::IsItemDeactivatedAfterEdit())*/
        {
            _logfile->SetPlaySpeed(_playSpeed);
        }
        const char * const presets[] = { "0.1", "0.5", "1.0", "2.0", "5.0", "10.0", "20.0", "50.0", "100.0" };
        ImGui::EndDisabled();
        Gui::ItemTooltip("Play speed [epochs/s]");

        ImGui::SameLine(0, 0);

        if (ImGui::BeginCombo("##SpeedChoice", NULL, ImGuiComboFlags_HeightLarge | ImGuiComboFlags_NoPreview))
        {
            for (int ix = 0; ix < NUMOF(presets); ix++)
            {
                if (ImGui::Selectable(presets[ix]))
                {
                    _playSpeed = std::strtof(presets[ix], NULL);
                    _logfile->SetPlaySpeed(_playSpeed);
                    _limitPlaySpeed = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    // Progress indicator
    if (canClose) // i.e. log must be opened
    {
        float progress = _logfile->GetPlayPos() * 100.0;
        ImGui::SetNextItemWidth(-1);
        ImGui::BeginDisabled(!canSeek);
        if (ImGui::SliderFloat("##PlayProgress", &progress, 0.0, 100.0, "%.3f%%", ImGuiSliderFlags_AlwaysClamp))
        {
            _seekProgress = progress; // Store away because...
        }
        if (!ImGui::IsItemActive()) { Gui::ItemTooltip("Play position (progress) [%filesize]"); }
        if (ImGui::IsItemDeactivated() && // ...this fires one frame later
            ( std::abs( _logfile->GetPlayPos() - _seekProgress ) > 1e-5 ) )
        {
            _logfile->Seek(_seekProgress);
        }
        ImGui::EndDisabled();
    }
    else
    {
        ImGui::InvisibleButton("##PlayProgress", _winSettings->iconButtonSize);
    }
    ImGui::Separator();
}


// ---------------------------------------------------------------------------------------------------------------------

/*static*/ std::vector<std::string> GuiWinInputLogfile::_recentLogs {};
/*static*/ std::mutex GuiWinInputLogfile::_recentLogsMutex {};

/*static*/ void GuiWinInputLogfile::_AddRecentLog(const std::string &path)
{
    std::lock_guard<std::mutex> lock(_recentLogsMutex);
    auto existing = std::find(_recentLogs.begin(), _recentLogs.end(), path);
    if (existing != _recentLogs.end())
    {
        _recentLogs.erase(existing);
    }
    _recentLogs.push_back(path);
    while (_recentLogs.size() > MAX_RECENT_LOGS)
    {
        _recentLogs.erase( _recentLogs.begin() );
    }
    UNUSED(path);
}

/* ****************************************************************************************************************** */
