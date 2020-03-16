//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2015 Joerg Henrichs
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

#include "network/network_config.hpp"
#include "config/stk_config.hpp"
#include "config/user_config.hpp"
#include "input/device_manager.hpp"
#include "modes/world.hpp"
#include "network/network.hpp"
#include "network/network_string.hpp"
#include "network/rewind_manager.hpp"
#include "network/server_config.hpp"
#include "network/socket_address.hpp"
#include "network/stk_host.hpp"
#include "network/stk_ipv6.hpp"
#include "online/xml_request.hpp"
#include "states_screens/main_menu_screen.hpp"
#include "states_screens/online/networking_lobby.hpp"
#include "states_screens/online/online_lan.hpp"
#include "states_screens/online/online_profile_servers.hpp"
#include "states_screens/online/online_screen.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/string_utils.hpp"
#include "utils/time.hpp"
#include "utils/utf8/unchecked.h"

#ifdef WIN32
#  include <windns.h>
#  include <ws2tcpip.h>
#ifndef __MINGW32__
#  pragma comment(lib, "dnsapi.lib")
#endif
#else
#  include <arpa/nameser.h>
#  include <arpa/nameser_compat.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <resolv.h>
#endif

#ifdef ANDROID
#include "../../../lib/irrlicht/source/Irrlicht/CIrrDeviceAndroid.h"
#include "graphics/irr_driver.hpp"
#endif

NetworkConfig *NetworkConfig::m_network_config[PT_COUNT];

/** \class NetworkConfig
 *  This class is the interface between STK and the online code, particularly
 *  STKHost. It stores all online related properties (e.g. if this is a server
 *  or a host, name of the server, maximum number of players, ip address, ...).
 *  They can either be set from the GUI code, or via the command line (for a
 *  stand-alone server).
 *  When STKHost is created, it takes all necessary information from this
 *  instance.
 */
// ============================================================================
/** Constructor.
 */
NetworkConfig::NetworkConfig()
{
    m_ip_type               = IP_NONE;
    m_network_type          = NETWORK_NONE;
    m_auto_connect          = false;
    m_is_server             = false;
    m_is_public_server      = false;
    m_done_adding_network_players = false;
    m_cur_user_id           = 0;
    m_cur_user_token        = "";
    m_client_port = 0;
    m_joined_server_version = 0;
    m_network_ai_instance = false;
    m_state_frequency = 10;
    m_nat64_prefix_data.fill(-1);
    m_num_fixed_ai = 0;
}   // NetworkConfig

// ----------------------------------------------------------------------------
/** Separated from constructor because this needs to be run after user config
 *  is load.
 */
void NetworkConfig::initClientPort()
{
    m_client_port = UserConfigParams::m_random_client_port ?
        0 : stk_config->m_client_port;
}   // initClientPort

// ----------------------------------------------------------------------------
/** Set that this is not a networked game.
 */
void NetworkConfig::unsetNetworking()
{
    clearServerCapabilities();
    m_network_type = NETWORK_NONE;
    ServerConfig::m_private_server_password = "";
}   // unsetNetworking

// ----------------------------------------------------------------------------
void NetworkConfig::setUserDetails(std::shared_ptr<Online::XMLRequest> r,
                                   const std::string& name)
{
    assert(!m_cur_user_token.empty());
    r->setApiURL(Online::API::USER_PATH, name);
    r->addParameter("userid", m_cur_user_id);
    r->addParameter("token", m_cur_user_token);
}   // setUserDetails

// ----------------------------------------------------------------------------
void NetworkConfig::setServerDetails(std::shared_ptr<Online::XMLRequest> r,
                                     const std::string& name)
{
    assert(!m_cur_user_token.empty());
    r->setApiURL(Online::API::SERVER_PATH, name);
    r->addParameter("userid", m_cur_user_id);
    r->addParameter("token", m_cur_user_token);
}   // setServerDetails

// ----------------------------------------------------------------------------
std::vector<GUIEngine::Screen*>
    NetworkConfig::getResetScreens(bool lobby) const
{
    if (lobby)
    {
        if (isWAN())
        {
            return
                {
                    MainMenuScreen::getInstance(),
                    OnlineScreen::getInstance(),
                    OnlineProfileServers::getInstance(),
                    NetworkingLobby::getInstance(),
                    nullptr
                };
        }
        else
        {
            return
                {
                    MainMenuScreen::getInstance(),
                    OnlineScreen::getInstance(),
                    OnlineLanScreen::getInstance(),
                    NetworkingLobby::getInstance(),
                    nullptr
                };
        }
    }
    else
    {
        if (isWAN())
        {
            return
                {
                    MainMenuScreen::getInstance(),
                    OnlineScreen::getInstance(),
                    OnlineProfileServers::getInstance(),
                    nullptr
                };
        }
        else
        {
            return
                {
                    MainMenuScreen::getInstance(),
                    OnlineScreen::getInstance(),
                    OnlineLanScreen::getInstance(),
                    nullptr
                };
        }
    }
}   // getResetScreens

// ----------------------------------------------------------------------------
/** Called before (re)starting network race, must be used before adding
 *  split screen players. */
void NetworkConfig::clearActivePlayersForClient() const
{
    if (!isClient())
        return;
    StateManager::get()->resetActivePlayers();
    if (input_manager)
    {
        input_manager->getDeviceManager()->setAssignMode(NO_ASSIGN);
        input_manager->getDeviceManager()->setSinglePlayer(NULL);
        input_manager->setMasterPlayerOnly(false);
        input_manager->getDeviceManager()->clearLatestUsedDevice();
    }
}   // clearActivePlayersForClient

// ----------------------------------------------------------------------------
/** True when client needs to round the bodies phyiscal info for current
 *  ticks, server doesn't as it will be done implictly in save state. */
bool NetworkConfig::roundValuesNow() const
{
    return isNetworking() && !isServer() && RewindManager::get()
        ->shouldSaveState(World::getWorld()->getTicksSinceStart());
}   // roundValuesNow

// ----------------------------------------------------------------------------
/** Use stun servers to detect current ip type.
 */
void NetworkConfig::detectIPType()
{
    if (UserConfigParams::m_default_ip_type != IP_NONE)
    {
        int ip_type = UserConfigParams::m_default_ip_type;
        m_nat64_prefix.clear();
        m_nat64_prefix_data.fill(-1);
        m_ip_type.store((IPType)ip_type);
        return;
    }
#ifdef ENABLE_IPV6
    ENetAddress eaddr = {};
    // We don't need to result of stun, just to check if the socket can be
    // used in ipv4 or ipv6
    uint8_t stun_tansaction_id[16] = {};
    BareNetworkString s = STKHost::getStunRequest(stun_tansaction_id);
    setIPv6Socket(0);
    auto ipv4 = std::unique_ptr<Network>(new Network(1, 1, 0, 0, &eaddr));
    setIPv6Socket(1);
    auto ipv6 = std::unique_ptr<Network>(new Network(1, 1, 0, 0, &eaddr));
    setIPv6Socket(0);

    auto& stunv4_map = UserConfigParams::m_stun_servers_v4;
    for (auto& s : getStunList(true/*ipv4*/))
    {
        if (stunv4_map.find(s) == stunv4_map.end())
            stunv4_map[s] = 0;
    }
    if (stunv4_map.empty())
        return;
    auto ipv4_it = stunv4_map.begin();
    int adv = StkTime::getMonoTimeMs() % stunv4_map.size();
    std::advance(ipv4_it, adv);

    auto& stunv6_map = UserConfigParams::m_stun_servers;
    for (auto& s : getStunList(false/*ipv4*/))
    {
        if (stunv6_map.find(s) == stunv6_map.end())
            stunv6_map[s] = 0;
    }
    if (stunv6_map.empty())
        return;
    auto ipv6_it = stunv6_map.begin();
    adv = StkTime::getMonoTimeMs() % stunv6_map.size();
    std::advance(ipv6_it, adv);

    SocketAddress::g_ignore_error_message = true;
    SocketAddress stun_v4(ipv4_it->first, 0/*port specified in addr*/,
        AF_INET);
    bool sent_ipv4 = false;
    if (!stun_v4.isUnset() && stun_v4.getFamily() == AF_INET)
    {
        sendto(ipv4->getENetHost()->socket, s.getData(), s.size(), 0,
            stun_v4.getSockaddr(), stun_v4.getSocklen());
        sent_ipv4 = true;
    }

    SocketAddress stun_v6(ipv6_it->first, 0/*port specified in addr*/,
        AF_INET6);
    bool sent_ipv6 = false;
    if (!stun_v6.isUnset() && stun_v6.getFamily() == AF_INET6)
    {
        sendto(ipv6->getENetHost()->socket, s.getData(), s.size(), 0,
            stun_v6.getSockaddr(), stun_v6.getSocklen());
        sent_ipv6 = true;
    }
    SocketAddress::g_ignore_error_message = false;

    bool has_ipv4 = false;
    bool has_ipv6 = false;

    ENetSocketSet socket_set;
    ENET_SOCKETSET_EMPTY(socket_set);
    ENET_SOCKETSET_ADD(socket_set, ipv4->getENetHost()->socket);
    if (sent_ipv4)
    {
        // 1.5 second timeout
        has_ipv4 = enet_socketset_select(
            ipv4->getENetHost()->socket, &socket_set, NULL, 1500) > 0;
    }

    ENET_SOCKETSET_EMPTY(socket_set);
    ENET_SOCKETSET_ADD(socket_set, ipv6->getENetHost()->socket);
    if (sent_ipv6 && enet_socketset_select(
        ipv6->getENetHost()->socket, &socket_set, NULL, 1500) > 0)
    {
        has_ipv6 = true;
        // For non dual stack IPv6 we try to get a NAT64 prefix to connect
        // to IPv4 only servers
        if (!has_ipv4)
        {
            // Detect NAT64 prefix by using ipv4only.arpa (RFC 7050)
            m_nat64_prefix.clear();
            m_nat64_prefix_data.fill(-1);
            SocketAddress nat64("ipv4only.arpa", 0/*port*/, AF_INET6);
            if (nat64.getFamily() == AF_INET6)
            {
                // Remove last 4 bytes which is IPv4 format
                struct sockaddr_in6* in6 =
                    (struct sockaddr_in6*)nat64.getSockaddr();
                uint8_t* byte = &(in6->sin6_addr.s6_addr[0]);
                byte[12] = 0;
                byte[13] = 0;
                byte[14] = 0;
                byte[15] = 0;
                m_nat64_prefix_data[0] = ((uint32_t)(byte[0]) << 8) | byte[1];
                m_nat64_prefix_data[1] = ((uint32_t)(byte[2]) << 8) | byte[3];
                m_nat64_prefix_data[2] = ((uint32_t)(byte[4]) << 8) | byte[5];
                m_nat64_prefix_data[3] = ((uint32_t)(byte[6]) << 8) | byte[7];
                m_nat64_prefix_data[4] = ((uint32_t)(byte[8]) << 8) | byte[9];
                m_nat64_prefix_data[5] = ((uint32_t)(byte[10]) << 8) | byte[11];
                m_nat64_prefix_data[6] = 0;
                m_nat64_prefix_data[7] = 0;
                m_nat64_prefix = getIPV6ReadableFromIn6(in6);
            }
        }
    }

    if (has_ipv4 && has_ipv6)
    {
        Log::info("NetworkConfig", "System is dual stack network.");
        m_nat64_prefix.clear();
        m_nat64_prefix_data.fill(-1);
        m_ip_type = IP_DUAL_STACK;
    }
    else if (has_ipv4)
    {
        Log::info("NetworkConfig", "System is IPv4 only.");
        m_nat64_prefix.clear();
        m_nat64_prefix_data.fill(-1);
        m_ip_type = IP_V4;
    }
    else if (has_ipv6)
    {
        Log::info("NetworkConfig", "System is IPv6 only.");
        if (m_nat64_prefix.empty())
            m_ip_type = IP_V6;
    }
    else
    {
        Log::error("NetworkConfig", "Cannot detect network type using stun, "
            "using previously detected type: %d", (int)m_ip_type.load());
    }
    if (has_ipv6)
    {
        if (!has_ipv4 && m_nat64_prefix.empty())
        {
            Log::warn("NetworkConfig", "NAT64 prefix not found, "
                "you may not be able to join any IPv4 only servers.");
        }
        if (!m_nat64_prefix.empty())
        {
            m_ip_type = IP_V6_NAT64;
            Log::info("NetworkConfig",
                "NAT64 prefix is %s.", m_nat64_prefix.c_str());
        }
    }
#else
    m_ip_type = IP_V4;
#endif
}   // detectIPType

// ----------------------------------------------------------------------------
void NetworkConfig::fillStunList(std::vector<std::string>& l,
                                 const std::string& dns)
{
#if defined(WIN32)
    PDNS_RECORD dns_record = NULL;
    DnsQuery(StringUtils::utf8ToWide(dns).c_str(), DNS_TYPE_SRV,
        DNS_QUERY_STANDARD, NULL, &dns_record, NULL);
    if (dns_record)
    {
        for (PDNS_RECORD curr = dns_record; curr; curr = curr->pNext)
        {
            if (curr->wType == DNS_TYPE_SRV)
            {
                l.push_back(
                    StringUtils::wideToUtf8(curr->Data.SRV.pNameTarget) +
                    ":" + StringUtils::toString(curr->Data.SRV.wPort));
            }
        }
        DnsRecordListFree(dns_record, DnsFreeRecordListDeep);
    }

#elif defined(ANDROID)
    CIrrDeviceAndroid* dev =
        dynamic_cast<CIrrDeviceAndroid*>(irr_driver->getDevice());
    if (!dev)
        return;
    android_app* android = dev->getAndroid();
    if (!android)
        return;

    bool was_detached = false;
    JNIEnv* env = NULL;

    jint status = android->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED)
    {
        JavaVMAttachArgs args;
        args.version = JNI_VERSION_1_6;
        args.name = "NativeThread";
        args.group = NULL;

        status = android->activity->vm->AttachCurrentThread(&env, &args);
        was_detached = true;
    }
    if (status != JNI_OK)
    {
        Log::error("NetworkConfig",
            "Cannot attach current thread in getDNSSrvRecords.");
        return;
    }

    jobject native_activity = android->activity->clazz;
    jclass class_native_activity = env->GetObjectClass(native_activity);

    if (class_native_activity == NULL)
    {
        Log::error("NetworkConfig",
            "getDNSSrvRecords unable to find object class.");
        if (was_detached)
        {
            android->activity->vm->DetachCurrentThread();
        }
        return;
    }

    jmethodID method_id = env->GetMethodID(class_native_activity,
        "getDNSSrvRecords", "(Ljava/lang/String;)[Ljava/lang/String;");

    if (method_id == NULL)
    {
        Log::error("NetworkConfig",
            "getDNSSrvRecords unable to find method id.");
        if (was_detached)
        {
            android->activity->vm->DetachCurrentThread();
        }
        return;
    }

    std::vector<uint16_t> jstr_data;
    utf8::unchecked::utf8to16(
        dns.c_str(), dns.c_str() + dns.size(), std::back_inserter(jstr_data));
    jstring text =
        env->NewString((const jchar*)jstr_data.data(), jstr_data.size());
    if (text == NULL)
    {
        Log::error("NetworkConfig",
            "Failed to create text for domain name.");
        if (was_detached)
        {
            android->activity->vm->DetachCurrentThread();
        }
        return;
    }

    jobjectArray arr =
        (jobjectArray)env->CallObjectMethod(native_activity, method_id, text);
    if (arr == NULL)
    {
        Log::error("NetworkConfig", "No array is created.");
        if (was_detached)
        {
            android->activity->vm->DetachCurrentThread();
        }
        return;
    }
    int len = env->GetArrayLength(arr);
    for (int i = 0; i < len; i++)
    {
        jstring jstr = (jstring)(env->GetObjectArrayElement(arr, i));
        if (!jstr)
            continue;
        const uint16_t* utf16_text =
            (const uint16_t*)env->GetStringChars(jstr, NULL);
        if (utf16_text == NULL)
            continue;
        const size_t str_len = env->GetStringLength(jstr);
        std::string tmp;
        utf8::unchecked::utf16to8(
            utf16_text, utf16_text + str_len, std::back_inserter(tmp));
        l.push_back(tmp);
        env->ReleaseStringChars(jstr, utf16_text);
    }
    if (was_detached)
    {
        android->activity->vm->DetachCurrentThread();
    }

#else
#define SRV_PORT (RRFIXEDSZ+4)
#define SRV_SERVER (RRFIXEDSZ+6)
#define SRV_FIXEDSZ (RRFIXEDSZ+6)

    unsigned char response[512] = {};
    int response_len = res_query(dns.c_str(), C_IN, T_SRV, response, 512);
    if (response_len > 0)
    {
        HEADER* header = (HEADER*)response;
        unsigned char* start = response + NS_HFIXEDSZ;

        if ((header->tc) || (response_len < NS_HFIXEDSZ))
            return;

        if (header->rcode >= 1 && header->rcode <= 5)
            return;

        int ancount = ntohs(header->ancount);
        int qdcount = ntohs(header->qdcount);
        if (ancount == 0)
            return;

        if (ancount > NS_PACKETSZ)
            return;

        for (int count = qdcount; count > 0; count--)
        {
            int str_len = dn_skipname(start, response + response_len);
            start += str_len + NS_QFIXEDSZ;
        }

        std::vector<unsigned char*> srv;
        for (int count = ancount; count > 0; count--)
        {
            int str_len = dn_skipname(start, response + response_len);
            start += str_len;
            srv.push_back(start);
            start += SRV_FIXEDSZ;
            start += dn_skipname(start, response + response_len);
        }

        for (unsigned i = 0; i < srv.size(); i++)
        {
            char server_name[512] = {};
            if (ns_name_ntop(srv[i] + SRV_SERVER, server_name, 512) < 0)
                continue;
            uint16_t port = ns_get16(srv[i] + SRV_PORT);
            l.push_back(std::string(server_name) + ":" +
                StringUtils::toString(port));
        }
    }
#endif
}   // fillStunList

// ----------------------------------------------------------------------------
const std::vector<std::string>& NetworkConfig::getStunList(bool ipv4)
{
    static std::vector<std::string> ipv4_list;
    static std::vector<std::string> ipv6_list;
    if (ipv4)
    {
        if (ipv4_list.empty())
            NetworkConfig::fillStunList(ipv4_list, stk_config->m_stun_ipv4);
        return ipv4_list;
    }
    else
    {
        if (ipv6_list.empty())
            NetworkConfig::fillStunList(ipv6_list, stk_config->m_stun_ipv6);
        return ipv6_list;
    }
}   // getStunList
