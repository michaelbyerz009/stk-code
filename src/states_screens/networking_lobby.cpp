//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013-2015 Glenn De Jonghe
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "states_screens/networking_lobby.hpp"

#include <string>

#include "config/user_config.hpp"
#include "config/player_manager.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/CGUISpriteBank.hpp"
#include "guiengine/scalable_font.hpp"
#include "guiengine/widgets/CGUIEditBox.hpp"
#include "guiengine/widgets/button_widget.hpp"
#include "guiengine/widgets/icon_button_widget.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/list_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "input/device_manager.hpp"
#include "input/input_manager.hpp"
#include "io/file_manager.hpp"
#include "network/network_config.hpp"
#include "network/protocols/connect_to_server.hpp"
#include "network/protocols/lobby_protocol.hpp"
#include "network/server.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"
#include "states_screens/state_manager.hpp"
#include "states_screens/dialogs/network_user_dialog.hpp"
#include "utils/translation.hpp"

#include <utfwrapping.h>

using namespace Online;
using namespace GUIEngine;

/** This is the lobby screen that is shown on all clients, but not on the
 *  server. It shows currently connected clients, and allows the 'master'
 *  client (i.e. the stk instance that created the server) to control the
 *  server. This especially means that it can initialise the actual start
 *  of the racing.
 *  This class is responsible for creating the ActivePlayers data structure
 *  for all local players (the ActivePlayer maps device to player, i.e.
 *  controls which device is used by which player). Note that a server
 *  does not create an instance of this class and will create the ActivePlayer
 *  data structure in LobbyProtocol::loadWorld().
 */
// ----------------------------------------------------------------------------
NetworkingLobby::NetworkingLobby() : Screen("online/networking_lobby.stkgui")
{
    m_server_info_height = 0;

    m_back_widget = NULL;
    m_header = NULL;
    m_text_bubble = NULL;
    m_timeout_message = NULL;
    m_exit_widget = NULL;
    m_start_button = NULL;
    m_player_list = NULL;
    m_chat_box = NULL;
    m_send_button = NULL;
    m_icon_bank = NULL;
}   // NetworkingLobby

// ----------------------------------------------------------------------------

void NetworkingLobby::loadedFromFile()
{
    m_header = getWidget<LabelWidget>("lobby-text");
    assert(m_header != NULL);

    m_back_widget = getWidget<IconButtonWidget>("back");
    assert(m_back_widget != NULL);

    m_start_button = getWidget<IconButtonWidget>("start");
    assert(m_start_button!= NULL);

    m_text_bubble = getWidget<LabelWidget>("text");
    assert(m_text_bubble != NULL);

    m_timeout_message = getWidget<LabelWidget>("timeout-message");
    assert(m_timeout_message != NULL);

    m_chat_box = getWidget<TextBoxWidget>("chat");
    assert(m_chat_box != NULL);

    m_send_button = getWidget<ButtonWidget>("send");
    assert(m_send_button != NULL);

    m_player_list = getWidget<ListWidget>("players");
    assert(m_player_list!= NULL);

    m_exit_widget = getWidget<IconButtonWidget>("exit");
    assert(m_exit_widget != NULL);

    m_icon_bank = new irr::gui::STKModifiedSpriteBank(GUIEngine::getGUIEnv());
    video::ITexture* icon_1 = irr_driver->getTexture
        (file_manager->getAsset(FileManager::GUI, "crown.png"));
    video::ITexture* icon_2 = irr_driver->getTexture
        (file_manager->getAsset(FileManager::GUI, "difficulty_medium.png"));
    video::ITexture* icon_3 = irr_driver->getTexture
        (file_manager->getAsset(FileManager::GUI, "main_help.png"));
    m_icon_bank->addTextureAsSprite(icon_1);
    m_icon_bank->addTextureAsSprite(icon_2);
    m_icon_bank->addTextureAsSprite(icon_3);
    const int screen_width = irr_driver->getFrameSize().Width;
    m_icon_bank->setScale(screen_width > 1280 ? 0.4f : 0.25f);
}   // loadedFromFile

// ---------------------------------------------------------------------------
void NetworkingLobby::beforeAddingWidget()
{
} // beforeAddingWidget

// ----------------------------------------------------------------------------
/** This function is a callback from the parent class, and is called each time
 *  this screen is shown to initialise the screen. This class is responsible
 *  for creating the active players and binding them to the right device.
 */
void NetworkingLobby::init()
{
    Screen::init();

    m_cur_starting_timer = m_start_threshold = m_start_timeout =
        m_server_max_player = std::numeric_limits<float>::max();
    m_timeout_message->setVisible(false);

    //I18N: In the networking lobby
    m_header->setText(_("Lobby"), false);
    m_server_info_height = GUIEngine::getFont()->getDimension(L"X").Height;
    m_start_button->setVisible(false);
    m_state = LS_CONNECTING;
    getWidget("chat")->setVisible(false);
    getWidget("chat")->setActive(false);
    getWidget("send")->setVisible(false);
    getWidget("send")->setActive(false);

    // Connect to server now if we have saved players and not disconnected
    if (!LobbyProtocol::get<LobbyProtocol>() &&
        !NetworkConfig::get()->getNetworkPlayers().empty())
        std::make_shared<ConnectToServer>(m_joined_server)->requestStart();

    if (NetworkConfig::get()->getNetworkPlayers().empty())
    {
        m_state = LS_ADD_PLAYERS;
    }
    else if (NetworkConfig::get()->isClient() &&
        UserConfigParams::m_lobby_chat)
    {
        m_chat_box->clearListeners();
        m_chat_box->addListener(this);
        getWidget("chat")->setVisible(true);
        getWidget("chat")->setActive(true);
        getWidget("send")->setVisible(true);
        getWidget("send")->setActive(true);
    }

}   // init

// ----------------------------------------------------------------------------
void NetworkingLobby::addMoreServerInfo(core::stringw info)
{
    const unsigned box_width = m_text_bubble->getDimension().Width;
    while (GUIEngine::getFont()->getDimension(info.c_str()).Width > box_width)
    {
        core::stringw brokentext = info;
        while (brokentext.size() > 0)
        {
            brokentext.erase(brokentext.size() - 1);
            if (GUIEngine::getFont()->getDimension(brokentext.c_str()).Width <
                box_width && gui::breakable(brokentext.lastChar()))
                break;
        }
        if (brokentext.size() == 0)
            break;
        m_server_info.push_back(brokentext);
        info =
            info.subString(brokentext.size(), info.size() - brokentext.size());
    }
    if (info.size() > 0)
    {
        m_server_info.push_back(info);
    }
    while ((int)m_server_info.size() * m_server_info_height >
        m_text_bubble->getDimension().Height)
    {
        m_server_info.erase(m_server_info.begin());
    }
}   // addMoreServerInfo

// ----------------------------------------------------------------------------
void NetworkingLobby::onUpdate(float delta)
{
    if (NetworkConfig::get()->isServer())
        return;
    if (m_timeout_message->isVisible() && m_player_list)
    {
        float cur_player = (float)(m_player_list->getItemCount());
        if (cur_player > m_server_max_player * m_start_threshold &&
            m_cur_starting_timer == std::numeric_limits<float>::max())
        {
            m_cur_starting_timer = m_start_timeout;
        }
        else if (cur_player < m_server_max_player * m_start_threshold)
        {
            m_cur_starting_timer = std::numeric_limits<float>::max();
            m_timeout_message->setText(L"", true);
        }

        if (m_cur_starting_timer != std::numeric_limits<float>::max())
        {
            m_cur_starting_timer -= delta;
            if (m_cur_starting_timer < 0.0f)
                m_cur_starting_timer = 0.0f;
            //I18N: In the networking lobby, display the starting timeout
            //for owner-less server
            core::stringw msg = _P("Game will start after %d second.",
               "Game will start after %d seconds.", (int)m_cur_starting_timer);
            m_timeout_message->setText(msg, true);
        }
    }

    if (m_state == LS_ADD_PLAYERS)
    {
        m_text_bubble->setText(_("Everyone:\nPress the 'Select' button to "
                                          "join the game"), true);
        m_start_button->setVisible(false);
        m_exit_widget->setVisible(false);
        if (!GUIEngine::ModalDialog::isADialogActive())
        {
            input_manager->getDeviceManager()->setAssignMode(DETECT_NEW);
            input_manager->getDeviceManager()->mapFireToSelect(true);
        }
        return;
    }

    m_start_button->setVisible(false);
    m_exit_widget->setVisible(true);
    auto lp = LobbyProtocol::get<LobbyProtocol>();
    if (!lp || !lp->waitingForPlayers())
    {
        core::stringw connect_msg;
        if (m_joined_server)
        {
            connect_msg = StringUtils::loadingDots(
                _("Connecting to server %s", m_joined_server->getName()));
        }
        else
        {
            connect_msg =
                StringUtils::loadingDots(_("Finding a quick play server"));
        }
        m_text_bubble->setText(connect_msg, true);
        m_start_button->setVisible(false);
    }
    else
    {
        if (m_server_peer.expired() && STKHost::existHost())
            m_server_peer = STKHost::get()->getServerPeerForClient();
        core::stringw total_msg;
        for (auto& string : m_server_info)
        {
            total_msg += string;
            total_msg += L"\n";
        }
        m_text_bubble->setText(total_msg, true);
    }

    if (STKHost::get()->isAuthorisedToControl())
    {
        m_start_button->setVisible(true);
    }
    //I18N: In the networking lobby, display ping when connected
    const uint32_t ping = getServerPing();
    if (ping != 0)
        m_header->setText(_("Lobby (ping: %dms)", ping), false);

}   // onUpdate

// ----------------------------------------------------------------------------
uint32_t NetworkingLobby::getServerPing() const
{
    if (auto p = m_server_peer.lock())
    {
        return p->getPing();
    }
    return 0;
}   // getServerPing

// ----------------------------------------------------------------------------
void NetworkingLobby::sendChat(irr::core::stringw text)
{
    text = text.trim().removeChars(L"\n\r");
    if (text.size() > 0)
    {
        NetworkString chat(PROTOCOL_LOBBY_ROOM);
        chat.addUInt8(LobbyProtocol::LE_CHAT);

        core::stringw name;
        PlayerProfile* player = PlayerManager::getCurrentPlayer();
        if (PlayerManager::getCurrentOnlineState() ==
            PlayerProfile::OS_SIGNED_IN)
            name = PlayerManager::getCurrentOnlineUserName();
        else
            name = player->getName();
        chat.encodeString16(name + L": " + text);

        STKHost::get()->sendToServer(&chat, true);
    }
}   // sendChat

// ----------------------------------------------------------------------------
void NetworkingLobby::eventCallback(Widget* widget, const std::string& name,
                                    const int playerID)
{
    if (name == m_back_widget->m_properties[PROP_ID])
    {
        StateManager::get()->escapePressed();
        return;
    }
    else if (name == m_player_list->m_properties[GUIEngine::PROP_ID])
    {
        auto host_online_ids = StringUtils::splitToUInt
            (m_player_list->getSelectionInternalName(), '_');
        if (host_online_ids.size() != 2)
        {
            return;
        }
        new NetworkUserDialog(host_online_ids[0], host_online_ids[1],
            m_player_list->getSelectionLabel());
    }   // click on a user
    else if (name == m_send_button->m_properties[PROP_ID])
    {
        sendChat(m_chat_box->getText());
        m_chat_box->setText("");
    }   // send chat message

    RibbonWidget* ribbon = dynamic_cast<RibbonWidget*>(widget);
    if (ribbon == NULL) return;
    const std::string &selection =
                     ribbon->getSelectionIDString(PLAYER_ID_GAME_MASTER);

    if (selection == m_exit_widget->m_properties[PROP_ID])
    {
        StateManager::get()->escapePressed();
    }
    else if (selection == m_start_button->m_properties[PROP_ID])
    {
        // Send a message to the server to start
        NetworkString start(PROTOCOL_LOBBY_ROOM);
        start.addUInt8(LobbyProtocol::LE_REQUEST_BEGIN);
        STKHost::get()->sendToServer(&start, true);
    }
}   // eventCallback

// ----------------------------------------------------------------------------
void NetworkingLobby::unloaded()
{
    delete m_icon_bank;
    m_icon_bank = NULL;
}   // unloaded

// ----------------------------------------------------------------------------
void NetworkingLobby::tearDown()
{
    m_joined_server.reset();
    // Server has a dummy network lobby too
    if (!NetworkConfig::get()->isClient())
        return;
    input_manager->getDeviceManager()->mapFireToSelect(false);
}   // tearDown

// ----------------------------------------------------------------------------
bool NetworkingLobby::onEscapePressed()
{
    m_joined_server.reset();
    input_manager->getDeviceManager()->mapFireToSelect(false);
    STKHost::get()->shutdown();
    return true; // close the screen
}   // onEscapePressed

// ----------------------------------------------------------------------------
void NetworkingLobby::updatePlayers(const std::vector<std::tuple<uint32_t,
                                    uint32_t, core::stringw, int> >& p)
{
    // In GUI-less server this function will be called without proper
    // initialisation
    if (!m_player_list)
        return;
    m_player_list->clear();

    if (p.empty())
        return;

    irr::gui::STKModifiedSpriteBank* icon_bank = m_icon_bank;
    for (auto& q : p)
    {
        if (icon_bank)
        {
            m_player_list->setIcons(icon_bank);
            icon_bank = NULL;
        }
        const std::string internal_name =
            StringUtils::toString(std::get<0>(q)) + "_" +
            StringUtils::toString(std::get<1>(q));
        m_player_list->addItem(internal_name, std::get<2>(q), std::get<3>(q));
    }
}   // updatePlayers

// ----------------------------------------------------------------------------
void NetworkingLobby::addSplitscreenPlayer(irr::core::stringw name)
{
    if (!m_player_list)
        return;
    m_player_list->setIcons(m_icon_bank);
    m_player_list->addItem(StringUtils::wideToUtf8(name), name, 1);
}   // addSplitscreenPlayer

// ----------------------------------------------------------------------------
void NetworkingLobby::finishAddingPlayers()
{
    m_state = LS_CONNECTING;
    std::make_shared<ConnectToServer>(m_joined_server)->requestStart();
    m_start_button->setVisible(false);
    if (UserConfigParams::m_lobby_chat)
    {
        m_chat_box->clearListeners();
        m_chat_box->addListener(this);
        getWidget("chat")->setVisible(true);
        getWidget("chat")->setActive(true);
        getWidget("send")->setVisible(true);
        getWidget("send")->setActive(true);
    }
}   // finishAddingPlayers

// ----------------------------------------------------------------------------
void NetworkingLobby::cleanAddedPlayers()
{
    if (!m_player_list)
        return;
    m_player_list->clear();
}   // cleanAddedPlayers

// ----------------------------------------------------------------------------
void NetworkingLobby::initAutoStartTimer(bool grand_prix_started,
                                         float start_threshold,
                                         float start_timeout,
                                         unsigned server_max_player)
{
    if (start_threshold == 0.0f || start_timeout == 0.0f)
        return;

    m_timeout_message->setVisible(true);
    m_start_threshold = grand_prix_started ? 0.0f : start_threshold;
    m_start_timeout = start_timeout;
    m_server_max_player = (float)server_max_player;
}   // initAutoStartTimer
