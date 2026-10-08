/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-key-manager-application.h"

#include "qkd-app-header.h"
#include "qkd-app-trailer.h"

#include "ns3/address-utils.h"
#include "ns3/callback.h"
#include "ns3/config.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/qkd-data-collector.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/tcp-socket.h"
#include "ns3/traced-callback.h"
#include "ns3/uinteger.h"

NS_LOG_COMPONENT_DEFINE("QkdKeyManagerApplication");

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(QkdKeyManagerApplication);

QkdKeyManagerApplication::QkdKeyManagerApplication()
    : SinkApplication(443),
      m_state(NOT_STARTED),
      m_appTxBuffer(Create<QkdAppTxBuffer>()),
      m_peerKmaTxBuffer(Create<QkdPeerKmaTxBuffer>()),
      m_peerKmaAddresses(std::vector<Address>())
{
    NS_LOG_FUNCTION(this);
}

QkdKeyManagerApplication::~QkdKeyManagerApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_appTxBuffer)
    {
        m_appTxBuffer->CloseAllSockets();
    }
    if (m_peerKmaTxBuffer)
    {
        m_peerKmaTxBuffer->CloseAllSockets();
    }
    if (m_qkdKeyManager)
    {
        // m_qkdKeyManager->Stop(); // Stop the QKD Key Manager before disposing
        m_qkdKeyManager->Unref();
    }
}

TypeId
QkdKeyManagerApplication::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QkdKeyManagerApplication")
                            .SetParent<SinkApplication>()
                            .AddConstructor<QkdKeyManagerApplication>()
                            // Add Attributes and Trace Sources here
                            .SetGroupName("Quantum");
    return tid;
}

void
QkdKeyManagerApplication::SetLocal(const Address& addr)
{
    NS_LOG_FUNCTION(this << addr);
    if (!addr.IsInvalid())
    {
        m_local = addr;
        if (m_port != INVALID_PORT &&
            (Ipv4Address::IsMatchingType(m_local) || Ipv6Address::IsMatchingType(m_local)))
        {
            // Convert the address to a socket address with the specified port
            NS_LOG_DEBUG("Setting local address: " << m_local << " with port: " << m_port);
            {
                m_local = addressUtils::ConvertToSocketAddress(m_local, m_port);
            }
        }
        else if (!m_local.IsInvalid())
        {
            NS_LOG_DEBUG("Setting local address: " << m_local << " without port.");
        }
        else
        {
            NS_LOG_ERROR("Invalid local address provided to SetLocal");
        }
    }
}

void
QkdKeyManagerApplication::SetDevicesAndPeerKMAs(const std::vector<Address>& peerKmaAddresses,
                                                std::vector<Ptr<QkdDevice>> devices)
{
    NS_LOG_FUNCTION(this << peerKmaAddresses << devices);
    if (peerKmaAddresses.empty())
    {
        NS_LOG_ERROR("No Peer KMAs provided. Cannot set Peer KMAs.");
        return;
    }

    // Set the peer KMA addresses
    m_peerKmaAddresses = peerKmaAddresses;

    // Set the QKD devices associated with these Peer KMAs
    m_qkdKeyManager = CreateObject<QkdKeyManager>(devices);
}

std::vector<Ptr<QkdDevice>>
QkdKeyManagerApplication::GetQkdDevices() const
{
    NS_LOG_FUNCTION(this);

    // Search for QkdDevice objects in the node
    std::vector<Ptr<QkdDevice>> deviceList;
    for (uint32_t i = 0; i < GetNode()->GetNDevices(); ++i)
    {
        Ptr<NetDevice> device = GetNode()->GetDevice(i);
        if (!device)
        {
            NS_LOG_WARN("No NetDevice found at index " << i << " in the node.");
            continue;
        }

        // Iterate over the aggregates of the NetDevice to find QkdDevice
        if (GetNode()->GetDevice(i)->GetTypeId() == QkdDevice::GetTypeId())
        {
            Ptr<QkdDevice> qkdDevice = DynamicCast<QkdDevice>(GetNode()->GetDevice(i));
            if (qkdDevice)
            {
                NS_LOG_DEBUG("Found QkdDevice at index " << i);
                deviceList.push_back(qkdDevice);
            }
            else
            {
                NS_LOG_WARN("No QkdDevice found in NetDevice at index " << i);
            }
        }
        else
        {
            NS_LOG_DEBUG("NetDevice at index " << i << " is not a QkdDevice.");
        }
    }

    return deviceList;
}

Ptr<Socket>
QkdKeyManagerApplication::GetSocket() const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

QkdKeyManagerApplication::QkdKeyManagerAppState_t
QkdKeyManagerApplication::GetState() const
{
    NS_LOG_FUNCTION(this);
    return m_state;
}

std::string
QkdKeyManagerApplication::GetStateString() const
{
    NS_LOG_FUNCTION(this);
    return GetStateString(m_state);
}

std::string
QkdKeyManagerApplication::GetStateString(QkdKeyManagerAppState_t state)
{
    NS_LOG_FUNCTION(state);
    switch (state)
    {
    case NOT_STARTED:
        return "NOT_STARTED";
    case STARTED:
        return "STARTED";
    case STOPPED:
        return "STOPPED";
    default:
        NS_FATAL_ERROR("Unknown state");
        return "FATAL_ERROR";
    }
}

void
QkdKeyManagerApplication::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_appTxBuffer->CloseAllSockets();
    m_peerKmaTxBuffer->CloseAllSockets();
    if (m_qkdKeyManager)
    {
        // m_qkdKeyManager->Stop(); // Stop the QKD Key Manager before disposing
    }

    if (!Simulator::IsFinished())
    {
        StopApplication(); // Ensure the application is stopped before disposing
    }
    m_socket = nullptr; // Clear the socket pointer
    m_qkdKeyManager = nullptr;
    m_appTxBuffer = nullptr;
    m_peerKmaTxBuffer = nullptr;
    SinkApplication::DoDispose(); // Chain up to the parent class
}

void
QkdKeyManagerApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);
    // Get list of QKD devices in the node
    if (!m_qkdKeyManager)
    {
        std::vector<Ptr<QkdDevice>> deviceList = GetQkdDevices();
        if (deviceList.empty())
        {
            NS_LOG_ERROR("No QKD devices found in the node. QkdKeyManagerApplication cannot "
                         "be created.");
            return;
        }

        m_qkdKeyManager = CreateObject<QkdKeyManager>(deviceList);
    }
    NS_ASSERT_MSG(m_qkdKeyManager, "QKD Key Manager is not initialized.");
    // m_qkdKeyManager->Start();

    if (m_state != NOT_STARTED)
    {
        NS_LOG_ERROR("Application already started or stopped. Cannot start again.");
    }

    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());

        NS_ABORT_MSG_IF(m_local.IsInvalid(), "Local address is invalid. Cannot bind socket.");
        if (InetSocketAddress::IsMatchingType(m_local))
        {
            const auto [[maybe_unused]] ipv4 = InetSocketAddress::ConvertFrom(m_local).GetIpv4();
            NS_LOG_INFO(this << " Binding on " << ipv4 << " port " << m_port << " / " << m_local
                             << ".");
        }
        else if (Inet6SocketAddress::IsMatchingType(m_local))
        {
            const auto [[maybe_unused]] ipv6 = Inet6SocketAddress::ConvertFrom(m_local).GetIpv6();
            NS_LOG_INFO(this << " Binding on " << ipv6 << " port " << m_port << " / " << m_local
                             << ".");
        }
        else
        {
            NS_ABORT_MSG("Incompatible local address");
        }

        auto ret [[maybe_unused]] = m_socket->Bind(m_local);
        NS_LOG_DEBUG(this << " Bind() return value= " << ret
                          << ", GetErrNo()= " << m_socket->GetErrno() << ".");
        ret = m_socket->Listen();
        NS_LOG_DEBUG(this << " Listen() return value= " << ret
                          << ", GetErrNo()= " << m_socket->GetErrno() << ".");

        NS_ASSERT_MSG(m_socket, "Socket creation failed. Cannot start application.");

        m_socket->SetAcceptCallback(
            MakeCallback(&QkdKeyManagerApplication::ConnectionRequestCallback, this),
            MakeCallback(&QkdKeyManagerApplication::ConnectionSucceededCallback, this));
        m_socket->SetCloseCallbacks(
            MakeCallback(&QkdKeyManagerApplication::NormalCloseCallback, this),
            MakeCallback(&QkdKeyManagerApplication::ErrorCloseCallback, this));
        m_socket->SetRecvCallback(
            MakeCallback(&QkdKeyManagerApplication::ReceivedDataCallback, this));
    }
    SwitchToState(STARTED);
}

void
QkdKeyManagerApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_state == NOT_STARTED)
    {
        NS_LOG_WARN("Application not started. Nothing to stop.");
        return;
    }
    // m_qkdKeyManager->Stop(); // Stop the QKD Key Manager
    SwitchToState(STOPPED);

    // Close all accepted sockets
    m_appTxBuffer->CloseAllSockets();
    m_peerKmaTxBuffer->CloseAllSockets();

    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                    MakeNullCallback<void, Ptr<Socket>, const Address&>());
        m_socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                    MakeNullCallback<void, Ptr<Socket>>());

        m_socket = nullptr; // Clear the socket pointer
    }
}

bool
QkdKeyManagerApplication::ConnectionRequestCallback(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    if (std::find(m_peerKmaAddresses.begin(), m_peerKmaAddresses.end(), from) !=
        m_peerKmaAddresses.end())
    {
        if (InetSocketAddress::IsMatchingType(from))
        {
            // IPv4: use Ipv4InterfaceAddress for subnet comparison
            Ipv4Address fromAddr = InetSocketAddress::ConvertFrom(from).GetIpv4();
            Ipv4Address localAddr = InetSocketAddress::ConvertFrom(m_local).GetIpv4();
            Ipv4Mask mask("255.255.255.0");
            Ipv4InterfaceAddress ifaceAddr(localAddr, mask);

            return !ifaceAddr.IsInSameSubnet(fromAddr);
        }
        else if (Inet6SocketAddress::IsMatchingType(from))
        {
            // IPv6: use Ipv6PrefixChecker for subnet comparison
            Ipv6Address fromAddr = Inet6SocketAddress::ConvertFrom(from).GetIpv6();
            Ipv6Address localAddr = Inet6SocketAddress::ConvertFrom(m_local).GetIpv6();
            Ipv6Prefix prefix(64);
            Ipv6InterfaceAddress ifaceAddr = Ipv6InterfaceAddress(localAddr, prefix);

            return !ifaceAddr.IsInSameSubnet(fromAddr);
        }
        else
        {
            NS_LOG_ERROR("Unsupported address type for peer KMA connection request.");
            return false; // Reject unknown address types
        }
    }
    else
    {
        if (InetSocketAddress::IsMatchingType(from))
        {
            // IPv4: use Ipv4InterfaceAddress for subnet comparison
            Ipv4Address fromAddr = InetSocketAddress::ConvertFrom(from).GetIpv4();
            Ipv4Address localAddr = InetSocketAddress::ConvertFrom(m_local).GetIpv4();
            Ipv4Mask mask("255.255.255.0");
            Ipv4InterfaceAddress ifaceAddr(localAddr, mask);

            return ifaceAddr.IsInSameSubnet(fromAddr);
        }
        else if (Inet6SocketAddress::IsMatchingType(from))
        {
            // IPv6: use Ipv6PrefixChecker for subnet comparison
            Ipv6Address fromAddr = Inet6SocketAddress::ConvertFrom(from).GetIpv6();
            Ipv6Address localAddr = Inet6SocketAddress::ConvertFrom(m_local).GetIpv6();
            Ipv6Prefix prefix(64);
            Ipv6InterfaceAddress ifaceAddr = Ipv6InterfaceAddress(localAddr, prefix);

            return ifaceAddr.IsInSameSubnet(fromAddr);
        }
        else
        {
            NS_LOG_ERROR("Unsupported address type for peer KMA connection request.");
            return false; // Reject unknown address types
        }
    }
}

void
QkdKeyManagerApplication::ConnectionSucceededCallback(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    // If from address is in Peer KMA addresses, add it to the peer KMA buffer
    if (std::find(m_peerKmaAddresses.begin(), m_peerKmaAddresses.end(), from) !=
        m_peerKmaAddresses.end())
    {
        socket->SetCloseCallbacks(
            MakeCallback(&QkdKeyManagerApplication::NormalCloseKMACallback, this),
            MakeCallback(&QkdKeyManagerApplication::ErrorCloseKMACallback, this));
        socket->SetRecvCallback(
            MakeCallback(&QkdKeyManagerApplication::ReceivedDataCallback, this));
        m_peerKmaTxBuffer->AddSocket(socket);
        NS_LOG_INFO("Connection established with peer KMA " << from << " on socket " << socket);
        return;
    }
    socket->SetCloseCallbacks(MakeCallback(&QkdKeyManagerApplication::NormalCloseCallback, this),
                              MakeCallback(&QkdKeyManagerApplication::ErrorCloseCallback, this));
    socket->SetRecvCallback(MakeCallback(&QkdKeyManagerApplication::ReceivedDataCallback, this));
    m_appTxBuffer->AddSocket(socket);
    NS_LOG_INFO("Connection established with " << from << " on socket " << socket);
}

void
QkdKeyManagerApplication::NormalCloseCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    if (socket == m_socket)
    {
        if (m_state == STARTED)
        {
            NS_FATAL_ERROR("NormalCloseCallback called on the main socket. This should not "
                           "happen "
                           "when the application is in STARTED state.");
        }
    }
    else if (m_appTxBuffer->IsSocketInBuffer(socket))
    {
        if (m_appTxBuffer->IsBufferEmpty(socket))
        {
            socket->ShutdownSend();
            m_appTxBuffer->CloseSocket(socket);
            m_appTxBuffer->RemoveSocket(socket);
            NS_LOG_INFO("Normal close on socket " << socket << " with no pending data.");
        }
        else
        {
            m_appTxBuffer->PrepareClose(socket);
        }
    }
    else
    {
        NS_LOG_WARN("Normal close on unknown socket " << socket);
    }
}

void
QkdKeyManagerApplication::ErrorCloseCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    if (socket == m_socket)
    {
        if (m_state == STARTED)
        {
            NS_FATAL_ERROR("ErrorCloseCallback called on the main socket. This should not "
                           "happen "
                           "when the application is in STARTED state.");
        }
    }
    else if (m_appTxBuffer->IsSocketInBuffer(socket))
    {
        m_appTxBuffer->CloseSocket(socket);
        NS_LOG_INFO("Error close on socket " << socket);
    }
    else
    {
        NS_LOG_WARN("Error close on unknown socket " << socket);
    }
}

void
QkdKeyManagerApplication::ReceivedDataCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");
    NS_ABORT_MSG_IF(!m_appTxBuffer->IsSocketInBuffer(socket),
                    "ReceivedDataCallback called on a socket not in the buffer.");
    if (m_state != STARTED)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ReceivedDataCallback().");
    }
    Address from;
    while (auto packet = socket->RecvFrom(from))
    {
        if (packet->GetSize() == 0)
        {
            NS_LOG_DEBUG("Received empty packet on socket " << socket);
            continue; // Ignore empty packets
        }

        NS_LOG_DEBUG("Received packet of size " << packet->GetSize() << " from " << from
                                                << " on socket " << socket);
        if (packet->GetSize() == 0)
        {
            NS_LOG_DEBUG("Received empty packet on socket " << socket);
            continue; // Ignore empty packets
        }
        QkdAppHeader header;
        packet->RemoveHeader(header);

        switch (header.GetHeaderType())
        {
        case QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT: {
            NS_LOG_DEBUG("Received QKD_KEY_MANAGER_OPEN_CONNECT header.");
            if (header.GetKSID())
            {
                NS_LOG_DEBUG("Received KSID: " << static_cast<int>(header.GetKSID()));
                ServeOpenConnectRequest(socket,
                                        header.GetSource(),
                                        header.GetDestination(),
                                        header.GetKSID());
            }
            else
            {
                ServeOpenConnectRequest(socket, header.GetSource(), header.GetDestination());
            }
            break;
        }
        case QkdAppHeaderType::QKD_KEY_MANAGER_GET_KEY: {
            NS_LOG_DEBUG("Received QKD_KEY_MANAGER_GET_KEY header.");
            if (header.GetKSID())
            {
                NS_LOG_DEBUG("Received KSID: " << static_cast<int>(header.GetKSID()));
                ServeGetKeyRequest(socket,
                                   header.GetKSID(),
                                   header.GetSource(),
                                   header.GetDestination());
            }
            else
            {
                NS_LOG_ERROR("Received QKD_KEY_MANAGER_GET_KEY without a valid KSID.");
            }
            break;
        }
        case QkdAppHeaderType::QKD_KEY_MANAGER_CLOSE: {
            NS_LOG_DEBUG("Received QKD_KEY_MANAGER_CLOSE_CONNECT header.");
            if (header.GetKSID())
            {
                NS_LOG_DEBUG("Received KSID: " << static_cast<int>(header.GetKSID()));
                ServeCloseConnectRequest(socket,
                                         header.GetKSID(),
                                         header.GetSource(),
                                         header.GetDestination());
            }
            else
            {
                NS_LOG_ERROR("Received QKD_KEY_MANAGER_CLOSE_CONNECT without a valid KSID.");
            }
            break;
        }
        default:
            NS_LOG_ERROR("Unknown header type received on socket: "
                         << static_cast<int>(header.GetHeaderType()));
            break;
        }
    }
}

void
QkdKeyManagerApplication::ConnectionSucceededKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    if (m_state != STARTED)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString()
                                        << " for ConnectionSucceededKMACallback().");
    }
    m_peerKmaTxBuffer->AddSocket(socket);
}

void
QkdKeyManagerApplication::ConnectionFailedKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    if (m_state == STARTED)
    {
        NS_LOG_ERROR("Sender failed to connect to remote address ");
    }
    else
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ConnectionFailed().");
    }
}

void
QkdKeyManagerApplication::NormalCloseKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    m_peerKmaTxBuffer->CloseSocket(socket);
    m_peerKmaTxBuffer->RemoveSocket(socket);
    if (socket->GetErrno() != Socket::ERROR_NOTERROR)
    {
        NS_LOG_ERROR("Socket closed with error: " << socket->GetErrno());
    }
    socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                              MakeNullCallback<void, Ptr<Socket>>());
}

void
QkdKeyManagerApplication::ErrorCloseKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    m_peerKmaTxBuffer->CloseSocket(socket);
    m_peerKmaTxBuffer->RemoveSocket(socket);
    if (socket->GetErrno() != Socket::ERROR_NOTERROR)
    {
        NS_LOG_ERROR("Socket closed with error: " << socket->GetErrno());
    }
}

void
QkdKeyManagerApplication::ReceivedDataKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");
    NS_ABORT_MSG_IF(!m_peerKmaTxBuffer->IsSocketInBuffer(socket),
                    "ReceivedDataKMACallback called on a socket not in the buffer.");
    if (m_state != STARTED)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ReceivedDataKMACallback().");
    }
    Address from;
    while (auto packet = socket->RecvFrom(from))
    {
        if (packet->GetSize() == 0)
        {
            NS_LOG_DEBUG("Received empty packet on KMA socket " << socket);
            continue; // Ignore empty packets
        }

        NS_LOG_DEBUG("Received packet of size " << packet->GetSize() << " from " << from
                                                << " on KMA socket " << socket);
        QkdAppHeader header;
        packet->RemoveHeader(header);

        switch (header.GetHeaderType())
        {
        case QkdAppHeaderType::QKD_KEY_MANAGER_NEW_APP: {
            NS_LOG_DEBUG("Received QKD_KEY_MANAGER_NEW_APP header.");
            uint32_t ksid = header.GetKSID();
            if (ksid)
            {
                NS_LOG_DEBUG("Received KSID: " << static_cast<int>(ksid));
                ReceiveNewApp(packet);
            }
            else
            {
                NS_LOG_ERROR("Received QKD_KEY_MANAGER_NEW_APP without a valid KSID.");
            }
            break;
        }

        case QkdAppHeaderType::QKD_KEY_MANAGER_CLOSE: {
            NS_LOG_DEBUG("Received QKD_KEY_MANAGER_CLOSE header.");
            uint32_t ksid = header.GetKSID();
            if (ksid)
            {
                NS_LOG_DEBUG("Received KSID: " << static_cast<int>(ksid));
                ReceiveCloseSession(packet);
            }
            else
            {
                NS_LOG_ERROR("Received QKD_KEY_MANAGER_CLOSE without a valid KSID.");
            }
            break;
        }
        default:
            NS_LOG_ERROR("Unknown header type received on KMA socket: "
                         << static_cast<int>(header.GetHeaderType()));
            break;
        }
    }
}

void
QkdKeyManagerApplication::ServeOpenConnectRequest(Ptr<Socket> socket,
                                                  const Address& from,
                                                  const Address& to)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    if (m_appTxBuffer->IsSocketInBuffer(socket))
    {
        uint32_t ksid = m_qkdKeyManager->OpenConnect(from, to);

        Simulator::ScheduleNow(&QkdKeyManagerApplication::SendNewApp, this, socket, from, to, ksid);

        QkdAppHeader header;
        header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT);
        header.SetKSID(ksid);
        header.SetSource(from);
        header.SetDestination(to);

        SendPacket(socket, header);
        NS_LOG_DEBUG("Header sent with KSID: " << ksid << " from " << from << " to " << to
                                               << "status" << header.GetHeaderType());
    }
    else
    {
        NS_LOG_WARN("Socket " << socket << " is not free. Cannot open connection.");
        return;
    }
}

void
QkdKeyManagerApplication::SendNewApp(Ptr<Socket> socket,
                                     const Address& from,
                                     const Address& to,
                                     uint32_t ksid)
{
    NS_LOG_FUNCTION(this << socket << from << to);

    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    Ptr<Socket> kmaSocket = m_peerKmaTxBuffer->GetSocketForAddress(from);
    if (!kmaSocket)
    {
        NS_LOG_ERROR("No KMA socket found for address " << from);
        return;
    }

    QkdAppHeader header;
    header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_NEW_APP);
    header.SetKSID(ksid);
    header.SetSource(from);
    header.SetDestination(to);

    SendPacket(kmaSocket, header);
}

void
QkdKeyManagerApplication::ServeOpenConnectRequest(Ptr<Socket> socket,
                                                  const Address& from,
                                                  const Address& to,
                                                  const uint32_t ksid)

{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    if (m_appTxBuffer->IsSocketInBuffer(socket))
    {
        m_qkdKeyManager->OpenConnect(ksid, from, to);

        QkdAppHeader header;
        header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT);
        header.SetKSID(ksid);
        header.SetSource(from);
        header.SetDestination(to);

        SendPacket(socket, header);
    }
    else
    {
        NS_LOG_WARN("Socket " << socket << " is not free. Cannot open connection.");
        return;
    }
}

void
QkdKeyManagerApplication::ServeGetKeyRequest(Ptr<Socket> socket,
                                             uint32_t ksid,
                                             const Address& sourceAddress,
                                             const Address& destinationAddress)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    if (m_appTxBuffer->IsSocketInBuffer(socket))
    {
        std::string key = m_qkdKeyManager->GetKey(ksid, sourceAddress, destinationAddress);

        QkdAppHeader header;
        header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_GET_KEY);
        header.SetKSID(ksid);
        header.SetSource(sourceAddress);
        header.SetDestination(destinationAddress);

        QkdAppTrailer trailer;
        trailer.SetKeyOrData(key);

        SendPacket(socket, header, trailer);
        NS_LOG_INFO("Sent key for KSID " << static_cast<int>(ksid) << " to socket " << socket);
    }
    else
    {
        NS_LOG_WARN("Socket " << socket << " is not free. Cannot get key.");
        return;
    }
}

void
QkdKeyManagerApplication::ServeCloseConnectRequest(Ptr<Socket> socket,
                                                   uint32_t ksid,
                                                   const Address& sourceAddress,
                                                   const Address& destinationAddress)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    if (m_appTxBuffer->IsSocketInBuffer(socket))
    {
        m_qkdKeyManager->DeleteKeySession(ksid, sourceAddress, destinationAddress);

        // Simulator::ScheduleNow(&QkdKeyManagerApplication::SendCloseSession,
        //                        this,
        //                        socket,
        //                        ksid,
        //                        sourceAddress,
        //                        destinationAddress);

        QkdAppHeader header;
        header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_CLOSE);
        header.SetKSID(ksid);
        header.SetSource(sourceAddress);
        header.SetDestination(destinationAddress);

        SendPacket(socket, header);
    }
    else
    {
        NS_LOG_WARN("Socket " << socket << " is not free. Cannot close connection.");
        return;
    }
}

void
QkdKeyManagerApplication::SendCloseSession(Ptr<Socket> socket,
                                           uint32_t ksid,
                                           const Address& sourceAddress,
                                           const Address& destinationAddress)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    Ptr<Socket> kmaSocket = m_peerKmaTxBuffer->GetSocketInSameSubnet(destinationAddress);
    if (!kmaSocket)
    {
        NS_LOG_ERROR("No KMA socket found for address " << destinationAddress);
        return;
    }

    QkdAppHeader header;
    header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_CLOSE);
    header.SetKSID(ksid);
    header.SetSource(sourceAddress);
    header.SetDestination(destinationAddress);

    SendPacket(kmaSocket, header);
}

void
QkdKeyManagerApplication::SendPacket(Ptr<Socket> socket,
                                     const QkdAppHeader& header,
                                     const QkdAppTrailer& trailer)
{
    NS_LOG_FUNCTION(this << socket << header << trailer);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(header);
    packet->AddTrailer(trailer);

    NS_LOG_DEBUG("Sending packet with header: " << header << " and trailer: " << trailer);
    socket->Send(packet);
}

void
QkdKeyManagerApplication::SendPacket(Ptr<Socket> socket, const QkdAppHeader& header)
{
    NS_LOG_FUNCTION(this << socket << header);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(header);
    NS_LOG_DEBUG("Sending packet with header: " << header);
    socket->Send(packet);
}

void
QkdKeyManagerApplication::ReceiveNewApp(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    QkdAppHeader header;
    packet->PeekHeader(header);

    if (header.GetHeaderType() != QkdAppHeaderType::QKD_KEY_MANAGER_NEW_APP)
    {
        NS_LOG_ERROR("Received packet with invalid header type: " << header.GetHeaderType());
        return;
    }

    uint32_t ksid = header.GetKSID();
    Address sourceAddress = header.GetSource();
    Address destinationAddress = header.GetDestination();

    NS_LOG_DEBUG("Received new app request for KSID: " << static_cast<int>(ksid)
                                                       << " from: " << sourceAddress
                                                       << " to: " << destinationAddress);

    m_qkdKeyManager->OpenConnect(ksid, destinationAddress,
                                 sourceAddress); // Invert in receiver
}

void
QkdKeyManagerApplication::ReceiveCloseSession(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    NS_ABORT_MSG_IF(!m_qkdKeyManager, "QKD Key Manager is not initialized.");

    QkdAppHeader header;
    packet->PeekHeader(header);

    if (header.GetHeaderType() != QkdAppHeaderType::QKD_KEY_MANAGER_CLOSE)
    {
        NS_LOG_ERROR("Received packet with invalid header type: " << header.GetHeaderType());
        return;
    }

    uint32_t ksid = header.GetKSID();
    Address sourceAddress = header.GetSource();
    Address destinationAddress = header.GetDestination();

    NS_LOG_DEBUG("Received close session request for KSID: " << static_cast<int>(ksid)
                                                             << " from: " << sourceAddress
                                                             << " to: " << destinationAddress);

    // m_qkdKeyManager->DeleteKeySession(ksid,
    //                                   destinationAddress,
    //                                   sourceAddress); // Invert in receiver
}

void
QkdKeyManagerApplication::ConnectToPeerKMAs()
{
    NS_LOG_FUNCTION(this);
    if (m_state != STARTED)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ConnectToPeerKMAs().");
    }

    for (const auto& kmaPeer : m_peerKmaAddresses)
    {
        if (!m_peerKmaTxBuffer->GetSocketForAddress(kmaPeer) || kmaPeer != m_local)
        {
            NS_LOG_DEBUG("Opening connection to peer KMA at " << kmaPeer);
            OpenConnectionPeerKMA(kmaPeer);
        }
        else
        {
            NS_LOG_DEBUG("Socket for peer KMA at " << kmaPeer << " already exists.");
        }
    }
}

void
QkdKeyManagerApplication::OpenConnectionPeerKMA(const Address& kmaPeer)
{
    NS_LOG_FUNCTION(this);
    if (m_state != STARTED)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for OpenConnectionPeerKMA().");
    }

    Ptr<Socket> socketKMA = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    NS_ABORT_MSG_IF(kmaPeer.IsInvalid(), "Remote KMA address not properly set");
    if (!m_local.IsInvalid())
    {
        NS_ABORT_MSG_IF(InetSocketAddress::IsMatchingType(m_local) ||

                            Inet6SocketAddress::IsMatchingType(m_local),
                        "Incompatible KMA peer and local address IP version");
    }
    if (InetSocketAddress::IsMatchingType(kmaPeer))
    {
        const auto ret [[maybe_unused]] =
            m_local.IsInvalid() ? socketKMA->Bind() : socketKMA->Bind(m_local);
        NS_LOG_DEBUG(this << " Bind() return value= " << ret
                          << " GetErrNo= " << socketKMA->GetErrno() << ".");

        const auto ipv4 = InetSocketAddress::ConvertFrom(kmaPeer).GetIpv4();
        const auto port = InetSocketAddress::ConvertFrom(kmaPeer).GetPort();
        NS_LOG_INFO(this << " Connecting to KMA at " << ipv4 << " port " << port << " / " << kmaPeer
                         << ".");
    }
    else if (Inet6SocketAddress::IsMatchingType(kmaPeer))
    {
        const auto ret [[maybe_unused]] =
            m_local.IsInvalid() ? socketKMA->Bind6() : socketKMA->Bind(m_local);
        NS_LOG_DEBUG(this << " Bind6() return value= " << ret
                          << " GetErrNo= " << socketKMA->GetErrno() << ".");

        const auto ipv6 = Inet6SocketAddress::ConvertFrom(kmaPeer).GetIpv6();
        const auto port = Inet6SocketAddress::ConvertFrom(kmaPeer).GetPort();
        NS_LOG_INFO(this << " Connecting to KMA at " << ipv6 << " port " << port << " / " << kmaPeer
                         << ".");
    }
    else
    {
        NS_ASSERT_MSG(false, "Incompatible KMA address type: " << kmaPeer);
    }
    const auto ret [[maybe_unused]] = socketKMA->Connect(kmaPeer);
    NS_LOG_DEBUG(this << " Connect() return value= " << ret
                      << " GetErrNo= " << socketKMA->GetErrno() << ".");
    NS_ASSERT_MSG(socketKMA, "Failed to create KMA socket");
    socketKMA->SetConnectCallback(
        MakeCallback(&QkdKeyManagerApplication::ConnectionSucceededKMACallback, this),
        MakeCallback(&QkdKeyManagerApplication::ConnectionFailedKMACallback, this));
    socketKMA->SetCloseCallbacks(
        MakeCallback(&QkdKeyManagerApplication::NormalCloseKMACallback, this),
        MakeCallback(&QkdKeyManagerApplication::ErrorCloseKMACallback, this));
    socketKMA->SetRecvCallback(
        MakeCallback(&QkdKeyManagerApplication::ReceivedDataKMACallback, this));
    NS_LOG_DEBUG(this << " Opened connection to KMA at " << kmaPeer << " with socket "
                      << socketKMA);
}

void
QkdKeyManagerApplication::SwitchToState(QkdKeyManagerAppState_t newState)
{
    NS_LOG_FUNCTION(this << GetStateString() << " -> " << GetStateString(newState));
    if (m_state != newState)
    {
        m_state = newState;
        NS_LOG_INFO("Application state changed to: " << GetStateString(m_state));
    }
    else
    {
        NS_LOG_WARN("Attempted to switch to the same state: " << GetStateString(m_state));
    }
}

/// For QkdAppTxBuffer
QkdAppTxBuffer::QkdAppTxBuffer()
{
    NS_LOG_FUNCTION(this);
}

bool
QkdAppTxBuffer::IsSocketInBuffer(Ptr<Socket> socket) const
{
    auto it = m_txBuffer.find(socket);
    if (it != m_txBuffer.end())
    {
        NS_LOG_DEBUG("Socket " << socket << " is in the buffer.");
        return true;
    }
    return false;
}

void
QkdAppTxBuffer::AddSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!socket, "Cannot add a null socket to the buffer.");
    if (IsSocketInBuffer(socket))
    {
        NS_LOG_WARN("Socket " << socket << " is already in the buffer. Skipping add.");
        return;
    }

    QkdTxBuffer_t txBuffer;
    socket->GetPeerName(txBuffer.address);
    txBuffer.socketState = QKD_SOCKET_STARTED;
    txBuffer.isClosing = false;
    txBuffer.socketType = QkdSocketType_t::QKD_APP_SOCKET;
    m_txBuffer[socket] = txBuffer;
}

void
QkdAppTxBuffer::CloseSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!IsSocketInBuffer(socket), "Socket not found in the buffer.");
    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket not found in the buffer.");
    if (!Simulator::IsExpired(it->second.nextServe))
    {
        NS_LOG_INFO("Closing socket " << socket << " with pending data.");
        Simulator::Cancel(it->second.nextServe);
    }

    it->first->Close();
    it->second.socketState = QKD_SOCKET_STATE_CLOSED;
    it->second.isClosing = true;
    it->first->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    it->first->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                 MakeNullCallback<void, Ptr<Socket>>());

    m_txBuffer.erase(it);
    NS_LOG_DEBUG("Closed socket " << socket << " and removed from buffer.");
}

void
QkdAppTxBuffer::RemoveSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket " << socket << " cannot be found.");

    if (!Simulator::IsExpired(it->second.nextServe))
    {
        NS_LOG_INFO(this << " Canceling a serving event which is due in "
                         << Simulator::GetDelayLeft(it->second.nextServe).As(Time::S) << ".");
        Simulator::Cancel(it->second.nextServe);
    }

    it->first->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                 MakeNullCallback<void, Ptr<Socket>>());
    it->first->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    it->first->SetSendCallback(MakeNullCallback<void, Ptr<Socket>, uint32_t>());

    m_txBuffer.erase(it);
}

void
QkdAppTxBuffer::CloseAllSockets()
{
    NS_LOG_FUNCTION(this);

    for (auto& [socket, buffer] : m_txBuffer)
    {
        if (!Simulator::IsExpired(buffer.nextServe))
        {
            NS_LOG_INFO(this << " Canceling a serving event which is due in "
                             << Simulator::GetDelayLeft(buffer.nextServe).As(Time::S) << ".");
            Simulator::Cancel(buffer.nextServe);
        }

        socket->Close();
        socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                  MakeNullCallback<void, Ptr<Socket>>());
        socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        socket->SetSendCallback(MakeNullCallback<void, Ptr<Socket>, uint32_t>());
    }

    m_txBuffer.clear();
}

bool
QkdAppTxBuffer::IsBufferEmpty(Ptr<Socket> socket) const
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!IsSocketInBuffer(socket), "Socket not found in the buffer.");
    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket not found in the buffer.");
    return it->second.isClosing;
}

QkdAppTxBuffer::QkdSocketState_t
QkdAppTxBuffer::GetSocketState(Ptr<Socket> socket) const
{
    const auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket not found in the buffer.");
    return it->second.socketState;
}

void
QkdAppTxBuffer::SetSocketState(Ptr<Socket> socket, QkdSocketState_t state)
{
    NS_LOG_FUNCTION(this << socket << state);
    NS_ABORT_MSG_IF(!IsSocketInBuffer(socket), "Socket not found in the buffer.");
    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket not found in the buffer.");
    it->second.socketState = state;
}

void
QkdAppTxBuffer::MapNextEvent(Ptr<Socket> socket, EventId event)
{
    NS_LOG_FUNCTION(this << socket);

    auto it = m_txBuffer.find(socket);
    NS_ABORT_MSG_IF(it == m_txBuffer.end(), "Socket not found in the buffer.");
    it->second.nextServe = event;
}

void
QkdAppTxBuffer::PrepareClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket " << socket << " cannot be found.");
    it->second.isClosing = true;
}

/// For QkdPeerKmaTxBuffer
QkdPeerKmaTxBuffer::QkdPeerKmaTxBuffer()
{
    NS_LOG_FUNCTION(this);
}

bool
QkdPeerKmaTxBuffer::IsSocketInBuffer(Ptr<Socket> socket) const
{
    NS_LOG_FUNCTION(this << socket);
    auto it = m_txBuffer.find(socket);
    if (it != m_txBuffer.end())
    {
        NS_LOG_DEBUG("Socket " << socket << " is in the buffer.");
        return true;
    }
    return false;
}

void
QkdPeerKmaTxBuffer::AddSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!socket, "Cannot add a null socket to the buffer.");
    if (IsSocketInBuffer(socket))
    {
        NS_LOG_WARN("Socket " << socket << " is already in the buffer. Skipping add.");
        return;
    }
    QkdTxBuffer_t txBuffer;
    socket->GetPeerName(txBuffer.address);
    txBuffer.socketState = QKD_SOCKET_STARTED;
    txBuffer.isClosing = false;
    txBuffer.socketType = QkdSocketType_t::QKD_PEER_KMA_SOCKET;
    m_txBuffer[socket] = txBuffer;
    NS_LOG_DEBUG("Added socket " << socket << " to the peer KMA buffer.");
}

void
QkdPeerKmaTxBuffer::CloseSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!IsSocketInBuffer(socket), "Socket not found in the buffer.");
    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket not found in the buffer.");
    if (!Simulator::IsExpired(it->second.nextServe))
    {
        NS_LOG_INFO("Closing socket " << socket << " with pending data.");
        Simulator::Cancel(it->second.nextServe);
    }
    it->first->Close();
    it->second.socketState = QKD_SOCKET_STATE_CLOSED;
    it->second.isClosing = true;
    it->first->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    it->first->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                 MakeNullCallback<void, Ptr<Socket>>());
    m_txBuffer.erase(it);
    NS_LOG_DEBUG("Closed socket " << socket << " and removed from peer KMA buffer.");
}

void
QkdPeerKmaTxBuffer::RemoveSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket " << socket << " cannot be found.");
    if (!Simulator::IsExpired(it->second.nextServe))
    {
        NS_LOG_INFO(this << " Canceling a serving event which is due in "
                         << Simulator::GetDelayLeft(it->second.nextServe).As(Time::S) << ".");
        Simulator::Cancel(it->second.nextServe);
    }
    it->first->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                 MakeNullCallback<void, Ptr<Socket>>());
    it->first->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    m_txBuffer.erase(it);
}

void
QkdPeerKmaTxBuffer::CloseAllSockets()
{
    NS_LOG_FUNCTION(this);
    for (auto& [socket, buffer] : m_txBuffer)
    {
        if (!Simulator::IsExpired(buffer.nextServe))
        {
            NS_LOG_INFO(this << " Canceling a serving event which is due in "
                             << Simulator::GetDelayLeft(buffer.nextServe).As(Time::S) << ".");
            Simulator::Cancel(buffer.nextServe);
        }
        socket->Close();
        socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                  MakeNullCallback<void, Ptr<Socket>>());
        socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
    m_txBuffer.clear();
}

bool
QkdPeerKmaTxBuffer::IsBufferEmpty(Ptr<Socket> socket) const
{
    NS_LOG_FUNCTION(this << socket);
    NS_ABORT_MSG_IF(!IsSocketInBuffer(socket), "Socket not found in the buffer.");
    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket not found in the buffer.");
    return it->second.isClosing;
}

QkdPeerKmaTxBuffer::QkdSocketState_t
QkdPeerKmaTxBuffer::GetSocketState(Ptr<Socket> socket) const
{
    NS_LOG_FUNCTION(this << socket);
    const auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket not found in the buffer.");
    return it->second.socketState;
}

void
QkdPeerKmaTxBuffer::SetSocketState(Ptr<Socket> socket, QkdSocketState_t state)
{
    NS_LOG_FUNCTION(this << socket << state);
    NS_ABORT_MSG_IF(!IsSocketInBuffer(socket), "Socket not found in the buffer.");
    auto it = m_txBuffer.find(socket);
    it->second.socketState = state;
}

void
QkdPeerKmaTxBuffer::MapNextEvent(Ptr<Socket> socket, EventId event)
{
    NS_LOG_FUNCTION(this << socket);
    auto it = m_txBuffer.find(socket);
    NS_ABORT_MSG_IF(it == m_txBuffer.end(), "Socket not found in the buffer.");
    it->second.nextServe = event;
}

void
QkdPeerKmaTxBuffer::PrepareClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    auto it = m_txBuffer.find(socket);
    NS_ASSERT_MSG(it != m_txBuffer.end(), "Socket " << socket << " cannot be found.");
    it->second.isClosing = true;
}

void
QkdPeerKmaTxBuffer::SetSocketAddress(Ptr<Socket> socket, const Address& address)
{
    NS_LOG_FUNCTION(this << socket << address);
    NS_ABORT_MSG_IF(!IsSocketInBuffer(socket), "Socket not found in the buffer.");
    m_peerKmaSockets[address] = socket;
}

Ptr<Socket>
QkdPeerKmaTxBuffer::GetSocketForAddress(const Address& address) const
{
    NS_LOG_FUNCTION(this << address);
    auto it = m_peerKmaSockets.find(address);
    if (it != m_peerKmaSockets.end())
    {
        NS_LOG_DEBUG("Found socket for address " << address);
        return it->second;
    }
    NS_LOG_DEBUG("No socket found for address " << address);
    return nullptr;
}

Ptr<Socket>
QkdPeerKmaTxBuffer::GetSocketInSameSubnet(const Address& address) const
{
    NS_LOG_FUNCTION(this << address);

    for (const auto& [storedAddr, socket] : m_peerKmaSockets)
    {
        if (InetSocketAddress::IsMatchingType(storedAddr) &&
            InetSocketAddress::IsMatchingType(address))
        {
            Ipv4Address fromAddr = InetSocketAddress::ConvertFrom(address).GetIpv4();
            Ipv4Address localAddr = InetSocketAddress::ConvertFrom(storedAddr).GetIpv4();
            Ipv4Mask mask("255.255.255.0");
            Ipv4InterfaceAddress ifaceAddr(localAddr, mask);

            if (ifaceAddr.IsInSameSubnet(fromAddr))
            {
                NS_LOG_DEBUG("Found IPv4 socket in the same subnet for address " << address);
                return socket;
            }
        }
        else if (Inet6SocketAddress::IsMatchingType(storedAddr) &&
                 Inet6SocketAddress::IsMatchingType(address))
        {
            Ipv6Address fromAddr = Inet6SocketAddress::ConvertFrom(address).GetIpv6();
            Ipv6Address localAddr = Inet6SocketAddress::ConvertFrom(storedAddr).GetIpv6();
            Ipv6Prefix prefix(64);
            Ipv6InterfaceAddress ifaceAddr(localAddr, prefix);

            if (ifaceAddr.IsInSameSubnet(fromAddr))
            {
                NS_LOG_DEBUG("Found IPv6 socket in the same subnet for address " << address);
                return socket;
            }
        }
    }

    NS_LOG_DEBUG("No socket found in the same subnet for address " << address);
    return nullptr;
}

} // namespace ns3
