//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013-2015 SuperTuxKart-Team
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

#include "network/network.hpp"

#include "config/user_config.hpp"
#include "io/file_manager.hpp"
#include "network/network_string.hpp"
#include "network/transport_address.hpp"
#include "utils/time.hpp"

#include <string.h>
#if defined(WIN32)
#  include "ws2tcpip.h"
#  define inet_ntop InetNtop
#else
#  include <arpa/inet.h>
#  include <errno.h>
#  include <sys/socket.h>
#endif
#include <pthread.h>
#include <signal.h>

Synchronised<FILE*>Network::m_log_file = NULL;

// ============================================================================
/** Constructor that just initialises this object (esp. opening the packet
 *  log file), but it does not start a listener thread.
 *  \param peer_count : The maximum number of peers.
 *  \param channel_limit : The maximum number of channels per peer.
 *  \param max_incoming_bandwidth : The maximum incoming bandwidth.
 *  \param max_outgoing_bandwidth : The maximum outgoing bandwidth.
 */
Network::Network(int peer_count, int channel_limit,
                 uint32_t max_incoming_bandwidth,
                 uint32_t max_outgoing_bandwidth,
                 ENetAddress* address)
{
    m_host = enet_host_create(address, peer_count, channel_limit, 0, 0);
    if (!m_host)
    {
        Log::fatal("Network", "An error occurred while trying to create an "
                   "ENet client host.");
    }
}   // Network

// ----------------------------------------------------------------------------
/** Destructor. Stops the listening thread, closes the packet log file and
 *  destroys the enet host.
 */
Network::~Network()
{
    if (m_host)
    {
        enet_host_destroy(m_host);
    }
}   // ~Network

// ----------------------------------------------------------------------------
ENetPeer *Network::connectTo(const TransportAddress &address)
{
    const ENetAddress enet_address = address.toEnetAddress();
    return enet_host_connect(m_host, &enet_address, 2, 0);
}   // connectTo

// ----------------------------------------------------------------------------
/** \brief Sends a packet whithout ENet adding its headers.
 *  This function is used in particular to achieve the STUN protocol.
 *  \param data : Data to send.
 *  \param length : Length of the sent data.
 *  \param dst : Destination of the packet.
 */
void Network::sendRawPacket(uint8_t* data, int length,
                            const TransportAddress& dst)
{
    struct sockaddr_in to;
    int to_len = sizeof(to);
    memset(&to,0,to_len);

    to.sin_family = AF_INET;
    to.sin_port = htons(dst.getPort());
    to.sin_addr.s_addr = htonl(dst.getIP());

    sendto(m_host->socket, (char*)data, length, 0,(sockaddr*)&to, to_len);
    Log::verbose("Network", "Raw packet sent to %s",
                 dst.toString().c_str());
    Network::logPacket(NetworkString(std::string((char*)(data), length)),
                       false);
}   // sendRawPacket

// ----------------------------------------------------------------------------
/** \brief Receives a packet directly from the network interface.
 *  Receive a packet whithout ENet processing it and returns the
 *  sender's ip address and port in the TransportAddress structure.
 *  \param sender : Stores the transport address of the sender of the
 *                  received packet.
 *  \return A string containing the data of the received packet.
 */
uint8_t* Network::receiveRawPacket(TransportAddress* sender)
{
    const int LEN = 2048;
    // max size needed normally (only used for stun)
    uint8_t* buffer = new uint8_t[LEN]; 
    memset(buffer, 0, LEN);

    socklen_t from_len;
    struct sockaddr_in addr;

    from_len = sizeof(addr);
    int len = recvfrom(m_host->socket, (char*)buffer, LEN, 0,
                       (struct sockaddr*)(&addr), &from_len   );

    int i = 0;
     // wait to receive the message because enet sockets are non-blocking
    while(len == -1) // nothing received
    {
        StkTime::sleep(1); // wait 1 millisecond between two checks
        i++;
        len = recvfrom(m_host->socket, (char*)buffer, LEN, 0,
                       (struct sockaddr*)(&addr), &from_len   );
    }
    if (len == SOCKET_ERROR)
    {
        Log::error("STKHost",
                   "Problem with the socket. Please contact the dev team.");
    }
    // we received the data
    sender->setIP( ntohl((uint32_t)(addr.sin_addr.s_addr)) );
    sender->setPort( ntohs(addr.sin_port) );

    if (addr.sin_family == AF_INET)
    {
        Log::info("STKHost", "IPv4 Address of the sender was %s",
                  sender->toString().c_str());
    }
    return buffer;
}   // receiveRawPacket(TransportAddress* sender)

// ----------------------------------------------------------------------------
/** \brief Receives a packet directly from the network interface and
 *  filter its address.
 *  Receive a packet whithout ENet processing it. Checks that the
 *  sender of the packet is the one that corresponds to the sender
 *  parameter. Does not check the port right now.
 *  \param sender : Transport address of the original sender of the
 *                  wanted packet.
 *  \param max_tries : Number of times we try to read data from the
 *                  socket. This is aproximately the time we wait in
 *                  milliseconds. -1 means eternal tries.
 *  \return A string containing the data of the received packet
 *          matching the sender's ip address.
 */
uint8_t* Network::receiveRawPacket(const TransportAddress& sender,
                                   int max_tries)
{
    const int LEN = 2048;
    uint8_t* buffer = new uint8_t[LEN];
    memset(buffer, 0, LEN);

    socklen_t from_len;
    struct sockaddr_in addr;

    from_len = sizeof(addr);
    int len = recvfrom(m_host->socket, (char*)buffer, LEN, 0,
                       (struct sockaddr*)(&addr), &from_len    );

    int count = 0;
     // wait to receive the message because enet sockets are non-blocking
    while(len < 0 || addr.sin_addr.s_addr == sender.getIP())
    {
        count++;
        if (len>=0)
        {
            Log::info("STKHost", "Message received but the ip address didn't "
                                 "match the expected one.");
        }
        len = recvfrom(m_host->socket, (char*)buffer, LEN, 0, 
                       (struct sockaddr*)(&addr), &from_len);
        StkTime::sleep(1); // wait 1 millisecond between two checks
        if (count >= max_tries && max_tries != -1)
        {
            TransportAddress a(m_host->address);
            Log::verbose("STKHost", "No answer from the server on %s",
                         a.toString().c_str());
            delete [] buffer;
            return NULL;
        }
    }
    if (addr.sin_family == AF_INET)
    {
        TransportAddress a(ntohl(addr.sin_addr.s_addr));
        Log::info("STKHost", "IPv4 Address of the sender was %s",
                  a.toString(false).c_str());
    }
    Network::logPacket(NetworkString(std::string((char*)(buffer), len)), true);
    return buffer;
}   // receiveRawPacket(const TransportAddress& sender, int max_tries)

// ----------------------------------------------------------------------------
/** \brief Broadcasts a packet to all peers.
 *  \param data : Data to send.
 */
void Network::broadcastPacket(const NetworkString& data, bool reliable)
{
    ENetPacket* packet = enet_packet_create(data.getBytes(), data.size() + 1,
                                      reliable ? ENET_PACKET_FLAG_RELIABLE
                                               : ENET_PACKET_FLAG_UNSEQUENCED);
    enet_host_broadcast(m_host, 0, packet);
}   // broadcastPacket

// ----------------------------------------------------------------------------
void Network::openLog()
{
    m_log_file.setAtomic(NULL);
    if (UserConfigParams::m_packets_log_filename.toString() != "")
    {
        std::string s = file_manager
            ->getUserConfigFile(UserConfigParams::m_packets_log_filename);
        m_log_file.setAtomic(fopen(s.c_str(), "w+"));
    }
    if (!m_log_file.getData())
        Log::warn("STKHost", "Network packets won't be logged: no file.");
}   // openLog

// ----------------------------------------------------------------------------
/** \brief Log packets into a file
 *  \param ns : The data in the packet
 *  \param incoming : True if the packet comes from a peer.
 *  False if it's sent to a peer.
 */
void Network::logPacket(const NetworkString &ns, bool incoming)
{
    if (m_log_file.getData() == NULL) // read only access, no need to lock
        return;

    const char *arrow = incoming ? "<--" : "-->";

    m_log_file.lock();
    fprintf(m_log_file.getData(), "[%d\t]  %s  ",
            (int)(StkTime::getRealTime()), arrow);

    for (int i = 0; i < ns.size(); i++)
    {
        fprintf(m_log_file.getData(), "%d.", ns[i]);
    }
    fprintf(m_log_file.getData(), "\n");
    m_log_file.unlock();
}   // logPacket
// ----------------------------------------------------------------------------
void Network::closeLog()
{
    if (m_log_file.getData())
    {
        m_log_file.lock();
        fclose(m_log_file.getData());
        Log::warn("STKHost", "Packet logging file has been closed.");
        m_log_file.getData() = NULL;
        m_log_file.unlock();
    }
}   // closeLog