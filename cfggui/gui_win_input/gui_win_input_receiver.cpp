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

#include <functional>

#include "ff_port.h"
#include "ff_ubx.h"
#include "config.h"

#include "gui_inc.hpp"

#include "platform.hpp"

#include "gui_win_input_receiver.hpp"

/* ****************************************************************************************************************** */

GuiWinInputReceiver::GuiWinInputReceiver(const std::string &name) :
    GuiWinInput(name),
    _baudrate        { 0 },
    _stopSent        { false },
    _triggerConnect  { false },
    _focusPortInput  { false },
    _recordFileDialog{ _winName + "RecordFileDialog" },
    _recordSize      { 0 }
{
    DEBUG("GuiWinInputReceiver(%s)", _winName.c_str());

    WinSetTitle("Receiver X");

    _receiver = std::make_shared<InputReceiver>(name, _database);
    _receiver->SetDataCb( std::bind(&GuiWinInputReceiver::_ProcessData, this, std::placeholders::_1) );

    auto &recent = GuiSettings::GetRecentItems(GuiSettings::RECENT_RECEIVERS);
    if (!recent.empty())
    {
        _port = recent[0];
    }

    _ClearData();
};

// ---------------------------------------------------------------------------------------------------------------------

GuiWinInputReceiver::~GuiWinInputReceiver()
{
    DEBUG("~GuiWinInputReceiver(%s)", _winName.c_str());

    if (_receiver)
    {
        _receiver->Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWinInputReceiver::WinIsOpen()
{
    // Keep window open as long as receiver is still connected (during disconnect)
    return _winOpen || !_receiver->IsIdle();
}

// ---------------------------------------------------------------------------------------------------------------------

std::shared_ptr<InputReceiver> GuiWinInputReceiver::GetReceiver()
{
    return _receiver;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::Loop(const uint32_t &frame, const double &now)
{
    GuiWinInput::Loop(frame, now);

    // User requested to close window
    if (!_winOpen)
    {
        if (!_stopSent && !_receiver->IsIdle())
        {
            _receiver->Stop();
            _stopSent = true;
        }
    }

    // Expire data
    if (_epoch && !_receiver->IsIdle())
    {
        _epochAge = now - _epochTs;
        if (_epochAge > 5.0)
        {
            _epoch = nullptr;
        }
    }

    // Update recording
    if (_recordLog.IsOpen())
    {
        constexpr double sizeDeltaTime = 2.0;
        constexpr double msgDeltaTime = 0.5;

        if ( (now - _recordLastMsgTime) > msgDeltaTime)
        {
            _recordMessage = Ff::Sprintf("Stop recording\n%s,\n%.3f MiB, %.2f KiB/s",
                _recordFilePath.c_str(), (double)_recordSize * (1.0 / 1024.0 / 1024.0), _recordKiBs);
            _recordLastMsgTime = now;
            _recordButtonColor = _recordButtonColor == 0 ? (ImU32)GUI_COLOUR(C_RED) : 0;
        }
        if ( (now - _recordLastSizeTime) > sizeDeltaTime)
        {
            _recordKiBs = (double)(_recordSize - _recordLastSize) * (1.0 / (sizeDeltaTime * 1024.0));
            _recordLastSize = _recordSize;
            _recordLastSizeTime = now;
        }
    }

    // Pump receiver events and dispatch
    _receiver->Loop(now);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_ProcessData(const InputData &data)
{
    GuiWinInput::_ProcessData(data);

    switch (data.type)
    {
        case InputData::DATA_MSG:
            if (_recordLog.IsOpen())
            {
                if (_recordLog.Write(data.msg->data, data.msg->size))
                {
                    _recordSize += data.msg->size;
                }
                else
                {
                    _logWidget.AddLine(Ff::Sprintf("Failed writing %s: %s", _recordFilePath.c_str(),
                        _recordLog.GetError().c_str()), GUI_COLOUR(DEBUG_ERROR));
                    _recordLog.Close();
                }
            }
            break;
        case InputData::DATA_EPOCH:
            if (data.epoch->epoch.valid)
            {
                _epochTs = ImGui::GetTime();
            }
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_ClearData()
{
    GuiWinInput::_ClearData();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_AddDataWindow(std::unique_ptr<GuiWinData> dataWin)
{
    dataWin->SetReceiver(_receiver);
    _dataWindows.push_back(std::move(dataWin));
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_DrawActionButtons()
{
    GuiWinInput::_DrawActionButtons();

    Gui::VerticalSeparator();

    const struct
    {
        const char *button;
        RX_RESET_t  reset;
        const char *tooltip;
    } resets[] =
    {
        { ICON_FK_THERMOMETER_FULL,  RX_RESET_HOT,           "Hotstart\nKeep all navigation data, like u-center" },
        { ICON_FK_THERMOMETER_HALF,  RX_RESET_WARM,          "Warmstart\nDelete ephemerides, like u-center" },
        { ICON_FK_THERMOMETER_EMPTY, RX_RESET_COLD,          "Coldstart\nDelete all navigation data, like u-center" },

        { NULL,                      RX_RESET_SOFT,          "Controlled software reset (0x01)" },
        { NULL,                      RX_RESET_HARD,          "Hardware reset (watchdog) after shutdown (0x04)" },
        { NULL, RX_RESET_NONE, NULL },
        { NULL,                      RX_RESET_DEFAULT,       "Revert config to default, keep nav data" },
        { NULL,                      RX_RESET_FACTORY,       "Revert config to default and coldstart" },
        { NULL, RX_RESET_NONE, NULL },
        { NULL,                      RX_RESET_GNSS_STOP,     "Stop navigation" },
        { NULL,                      RX_RESET_GNSS_START,    "Start navigation" },
        { NULL,                      RX_RESET_GNSS_RESTART,  "Restart navigation" },
        { NULL, RX_RESET_NONE, NULL },
        { NULL,                      RX_RESET_SAFEBOOT,      "Safeboot" },
    };

    const bool disable = !_receiver->IsReady();
    ImGui::BeginDisabled(disable);

    // Individual buttons
    for (int ix = 0; ix < NUMOF(resets); ix++)
    {
        if (resets[ix].button != NULL)
        {
            if (ImGui::Button(resets[ix].button, GuiSettings::iconSize))
            {
                _receiver->Reset(resets[ix].reset);
            }
            Gui::ItemTooltip(resets[ix].tooltip);

            ImGui::SameLine();
        }
    }

    // Button with remaining reset commands
    if (ImGui::Button(ICON_FK_POWER_OFF "##Reset", GuiSettings::iconSize))
    {
        ImGui::OpenPopup("ResetCommands");
    }
    if (ImGui::BeginPopup("ResetCommands"))
    {
        for (int ix = 0; ix < NUMOF(resets); ix++)
        {
            if (resets[ix].button != NULL)
            {
                continue;
            }
            if (resets[ix].reset == RX_RESET_NONE)
            {
                ImGui::Separator();
            }
            else
            {
                if (ImGui::MenuItem(rxResetStr(resets[ix].reset)))
                {
                    _receiver->Reset(resets[ix].reset);
                }
                if ( (resets[ix].tooltip[0] != '\0'))
                {
                    Gui::ItemTooltip(resets[ix].tooltip);
                }
            }
        }
        ImGui::EndPopup();
    }
    Gui::ItemTooltip("Reset receiver");

    ImGui::EndDisabled();

    Gui::VerticalSeparator();

    //ImGui::BeginDisabled(disable);

    if (!_recordLog.IsOpen())
    {
        if (ImGui::Button(ICON_FK_CIRCLE "###Record", GuiSettings::iconSize))
        {
            if (!_recordFileDialog.IsInit())
            {
                auto &io = ImGui::GetIO();
                _recordFilePath = "";
                _recordFileDialog.InitDialog(GuiWinFileDialog::FILE_SAVE);
                _recordFileDialog.SetFilename( Ff::Strftime("log_%Y%m%d_%H%M") + (io.KeyCtrl ? ".ubx.gz" : ".ubx") );
                _recordFileDialog.WinSetTitle(_winTitle + " - Record logfile...");
                _recordFileDialog.SetFileFilter("\\.(ubx|raw|ubz|ubx\\.gz)", true);
            }
            else
            {
                _recordFileDialog.WinFocus();
            }
        }
        Gui::ItemTooltip("Record logfile\n(CTRL+click for compressed file)");
    }
    else
    {
        if (_recordButtonColor != 0) { ImGui::PushStyleColor(ImGuiCol_Text, _recordButtonColor); }
        if (ImGui::Button(ICON_FK_STOP "###Record", GuiSettings::iconSize))
        {
            _LogClose();
        }
        if (_recordButtonColor != 0) { ImGui::PopStyleColor(); }
        Gui::ItemTooltip(_recordMessage.c_str());
    }

    //ImGui::EndDisabled();

    if (_recordFileDialog.IsInit() && _recordFileDialog.DrawDialog())
    {
        _LogOpen(_recordFileDialog.GetPath());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_DrawControls()
{
    // Connect/disconnect/abort button
    {
        // Idle (i.e. disconnected)
        if (_receiver->IsIdle())
        {
            const bool disabled = _port.length() < 5;
            ImGui::BeginDisabled(disabled);
            if (ImGui::Button(ICON_FK_PLAY "###StartStop", GuiSettings::iconSize) || (!disabled && _triggerConnect))
            {
                _receiver->Start(_port, _baudrate);
            }
            ImGui::EndDisabled();
            Gui::ItemTooltip("Connect receiver");
        }
        // Ready (i.e. conncted and ready to receive commands)
        else if (_receiver->IsReady())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOUR(C_BRIGHTGREEN));
            if (ImGui::Button(ICON_FK_STOP "###StartStop", GuiSettings::iconSize))
            {
                _receiver->Stop();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Disconnect receiver");
        }
        // Busy (i.e. connected and busy doing something)
        else if (_receiver->IsBusy())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOUR(C_GREEN));
            if (ImGui::Button(ICON_FK_EJECT "###StartStop", GuiSettings::iconSize))
            {
                _receiver->Stop();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Force disconnect receiver");
        }
    }

    ImGui::SameLine();

    // Baudrate
    {
        const bool disabled = _receiver->IsBusy();
        ImGui::BeginDisabled(disabled);
        if (ImGui::Button(ICON_FK_TACHOMETER "##Baudrate", GuiSettings::iconSize))
        {
            ImGui::OpenPopup("Baudrate");
        }
        Gui::ItemTooltip(_baudrate > 0 ?
            Ff::Sprintf("Baudrate (currently: %d)", (int)_baudrate).c_str() : "Baudrate (currently: autobaud)");
        if (ImGui::BeginPopup("Baudrate"))
        {
            const int baudrates[] = { PORT_BAUDRATES };
            ImGui::TextUnformatted("Baudrate");
            int baudrate = _baudrate;
            bool update = false;
            if (ImGui::RadioButton("Auto", &baudrate, 0))
            {
                update = true;
            }
            for (int ix = 0; ix < NUMOF(baudrates); ix++)
            {
                char tmp[10];
                snprintf(tmp, sizeof(tmp), "%6d", baudrates[ix]);
                if (ImGui::RadioButton(tmp, &baudrate, baudrates[ix]))
                {
                    update = true;
                }
            }
            _baudrate = baudrate;
            if (update && _receiver->IsReady())
            {
                _receiver->SetBaudrate(baudrate);
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Port input
    {
        const bool disabled = !_receiver->IsIdle();
        ImGui::BeginDisabled(disabled);
        ImGui::PushItemWidth(-(GuiSettings::iconSize.x + GuiSettings::style->ItemSpacing.x));
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue /*| ImGuiInputTextFlags_AutoSelectAll*/;
        if (_focusPortInput)
        {
            ImGui::SetKeyboardFocusHere();
            _focusPortInput = false;
            flags |= ImGuiInputTextFlags_AutoSelectAll;
        }
        if (ImGui::InputTextWithHint("##Port", "Port", &_port, flags))
        {
            _triggerConnect = true;
            GuiSettings::AddRecentItem(GuiSettings::RECENT_RECEIVERS, _port);
        }
        else
        {
            _triggerConnect = false;
        }
        ImGui::PopItemWidth();
        //ImGui::SetItemDefaultFocus();
        Gui::ItemTooltip("Port, e.g.:\n[ser://]<device>\ntcp://<addr>:<port>\ntelnet://<addr>:<port>");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Ports button/menu
    {
        const bool disabled = !_receiver->IsIdle();
        ImGui::BeginDisabled(disabled);
        if (ImGui::Button(ICON_FK_STAR "##Recent", GuiSettings::iconSize))
        {
            ImGui::OpenPopup("Recent");
        }
        Gui::ItemTooltip("Recent and detected ports");
        if (ImGui::BeginPopup("Recent"))
        {
            Gui::TextTitle("Recently used ports");

            const std::string *selectedPort = nullptr;
            const auto &recent = GuiSettings::GetRecentItems(GuiSettings::RECENT_RECEIVERS);
            for (auto &port: recent)
            {
                if (ImGui::Selectable(port.c_str()))
                {
                    selectedPort = &port;
                }
            }
            if (!recent.empty())
            {
                ImGui::Separator();
                if (ImGui::Selectable("Clear recent ports"))
                {
                    GuiSettings::ClearRecentItems(GuiSettings::RECENT_RECEIVERS);
                }
            }

            ImGui::Separator();

            Gui::TextTitle("Detected ports");

            ImGui::PushID(1234);
            auto ports = Platform::EnumeratePorts(true);
            for (auto &p: ports)
            {
                if (ImGui::Selectable(p.port.c_str()))
                {
                    selectedPort = &p.port;
                }
                if (!p.desc.empty())
                {
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOUR(TEXT_DIM));
                    ImGui::TextUnformatted(p.desc.c_str());
                    ImGui::PopStyleColor();
                }
            }
            ImGui::PopID();
            if (ports.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOUR(TEXT_DIM));
                ImGui::TextUnformatted("No ports detected");
                ImGui::PopStyleColor();
            }

            ImGui::EndPopup();

            if (selectedPort)
            {
                _port = *selectedPort;
                _baudrate = 0;
                _focusPortInput = true;
                GuiSettings::AddRecentItem(GuiSettings::RECENT_RECEIVERS, *selectedPort);
            }
        }
        ImGui::EndDisabled();
    }

    ImGui::Separator();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_LogOpen(const std::string &path)
{
    if (!path.empty())
    {
        _recordFilePath = path;
        _recordSize = 0;
        _recordLastSize = 0;
        _recordLastMsgTime = 0.0;
        _recordLastSizeTime = 0.0;
        _recordKiBs = 0.0;

        if (_recordLog.OpenWrite(path))
        {
            std::string marker = (_rxVerStr.empty() ? "unknown receiver" : _rxVerStr) + ", " +
                _port + "@" + std::to_string(_baudrate) + ", " + Ff::Strftime("%Y-%m-%d %H:%M:%S") + ", " +
                "cfggui " CONFIG_VERSION " (" CONFIG_GITHASH ")";
            Ff::UbxMessage msg(UBX_INF_CLSID, UBX_INF_TEST_MSGID, marker, false);
            _recordLog.Write(msg.raw);
        }
        else
        {
            _logWidget.AddLine(Ff::Sprintf("Failed recording to %s: %s", _recordFilePath.c_str(),
                _recordLog.GetError().c_str()), GUI_COLOUR(DEBUG_ERROR));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_LogClose()
{
    _recordLog.Close();
    _logWidget.AddLine("Recording stopped", GUI_COLOUR(DEBUG_NOTICE));
}

/* ****************************************************************************************************************** */
