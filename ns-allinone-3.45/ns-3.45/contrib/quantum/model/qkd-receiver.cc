/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-receiver.h"

#include "ns3/address-utils.h"
#include "ns3/callback.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/symmetric-encryption.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/tcp-socket.h"
#include "ns3/uinteger.h"

#include <cryptopp/cryptlib.h>
#include <cryptopp/osrng.h>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("QkdSecureReceiver");
NS_OBJECT_ENSURE_REGISTERED(QkdSecureReceiver);

TypeId
QkdSecureReceiver::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QkdSecureReceiver")
                            .SetParent<SinkApplication>()
                            .SetGroupName("Applications")
                            .AddConstructor<QkdSecureReceiver>()
                            .AddAttribute("RemoteKMA",
                                          "The address of the Key Manager Application (KMA) to "
                                          "which the QKD "
                                          "receiver will send packets.",
                                          AddressValue(),
                                          MakeAddressAccessor(&QkdSecureReceiver::SetRemoteKMA),
                                          MakeAddressChecker());
    // Add trace sources and attributes here
    return tid;
}

QkdSecureReceiver::QkdSecureReceiver()
    : SinkApplication(80),
      m_state{NOT_STARTED},
      m_socket{nullptr},
      m_socketReceiver{nullptr},
      m_socketKMA{nullptr},
      m_ksid{0},
      m_key{},
      m_peerPort{},
      m_kmaPeerPort{}
{
    NS_LOG_FUNCTION(this);
}

QkdSecureReceiver::~QkdSecureReceiver()
{
    NS_LOG_FUNCTION(this);
}

void
QkdSecureReceiver::SetLocal(const Address& addr)
{
    NS_LOG_FUNCTION(this << addr);
    if (!addr.IsInvalid())
    {
        m_local = addr;
        if (Ipv4Address::IsMatchingType(m_local) || Ipv6Address::IsMatchingType(m_local))
        {
            m_local = addressUtils::ConvertToSocketAddress(m_local, m_port);
        }
    }
    else
    {
        NS_LOG_ERROR("Invalid local address provided to QkdSecureReceiver.");
    }
}

void
QkdSecureReceiver::SetRemoteKMA(const Address& addr)
{
    NS_LOG_FUNCTION(this << addr);
    if (!addr.IsInvalid())
    {
        m_kmaPeer = addr;
    }
    else
    {
        NS_LOG_ERROR("Invalid remote address provided to QkdSecureReceiver.");
    }
}

Ptr<Socket>
QkdSecureReceiver::GetSocketKMA() const
{
    NS_LOG_FUNCTION(this);
    return m_socketKMA;
}

QkdSecureReceiver::QkdAppState_t
QkdSecureReceiver::GetState() const
{
    NS_LOG_FUNCTION(this);
    return m_state;
}

std::string
QkdSecureReceiver::GetStateString() const
{
    NS_LOG_FUNCTION(this);
    return GetStateString(m_state);
}

std::string
QkdSecureReceiver::GetStateString(QkdAppState_t state)
{
    switch (state)
    {
    case NOT_STARTED:
        return "NOT_STARTED";
    case STARTED:
        return "STARTED";
    case AWAIT_KSID_FROM_SENDER:
        return "AWAIT_KSID_FROM_SENDER";
    case CONNECTING_KMA:
        return "CONNECTING_KMA";
    case REQUEST_OPEN_CONNECT:
        return "REQUEST_OPEN_CONNECT";
    case AWAIT_KSID_FROM_KMA:
        return "AWAIT_KSID_FROM_KMA";
    case REQUEST_GET_KEY:
        return "REQUEST_GET_KEY";
    case AWAIT_KEY:
        return "AWAIT_KEY";
    case CLOSE_CONNECT:
        return "CLOSE_CONNECT";
    case EXPECTING_REQUEST:
        return "EXPECTING_REQUEST";
    case ENCRYPTING_DATA:
        return "ENCRYPTING_DATA";
    case STOPPED:
        return "STOPPED";
    default:
        NS_LOG_ERROR("Unknown QKD receiver state: " << state);
        return "UNKNOWN_STATE";
    }
}

void
QkdSecureReceiver::DoDispose()
{
    NS_LOG_FUNCTION(this);

    if (!Simulator::IsFinished())
    {
        StopApplication();
    }

    if (m_socketKMA)
    {
        m_socketKMA->Close();
        m_socketKMA = nullptr;
    }

    if (m_socketReceiver)
    {
        m_socketReceiver->Close();
        m_socketReceiver = nullptr;
    }

    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }

    SinkApplication::DoDispose(); // Chain up to the parent class.
}

void
QkdSecureReceiver::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_state != NOT_STARTED)
    {
        NS_FATAL_ERROR("QKD receiver application already started.");
    }
    if (!m_socket)
    {
        const TypeId tcpSocketTid = TcpSocket::GetTypeId();
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());

        NS_ABORT_MSG_IF(m_local.IsInvalid(), "Local address not properly set");
        if (InetSocketAddress::IsMatchingType(m_local))
        {
            const auto ipv4 [[maybe_unused]] = InetSocketAddress::ConvertFrom(m_local).GetIpv4();
            NS_LOG_INFO(this << " Binding on " << ipv4 << " port " << m_port << " / " << m_local
                             << ".");
        }
        else if (Inet6SocketAddress::IsMatchingType(m_local))
        {
            const auto ipv6 [[maybe_unused]] = Inet6SocketAddress::ConvertFrom(m_local).GetIpv6();
            NS_LOG_INFO(this << " Binding on " << ipv6 << " port " << m_port << " / " << m_local
                             << ".");
        }
        else
        {
            NS_ABORT_MSG("Incompatible local address");
        }

        auto ret [[maybe_unused]] = m_socket->Bind(m_local);
        NS_LOG_DEBUG(this << " Bind() return value= " << ret
                          << " GetErrNo= " << m_socket->GetErrno() << ".");

        ret = m_socket->Listen();
        NS_LOG_DEBUG(this << " Listen() return value= " << ret
                          << " GetErrNo= " << m_socket->GetErrno() << ".");

        NS_ASSERT_MSG(m_socket, "Failed to create socket");
        m_socket->SetAcceptCallback(
            MakeCallback(&QkdSecureReceiver::ConnectionRequestCallback, this),
            MakeCallback(&QkdSecureReceiver::ConnectionCreatedCallback, this));

        m_socket->SetCloseCallbacks(MakeCallback(&QkdSecureReceiver::NormalCloseCallback, this),
                                    MakeCallback(&QkdSecureReceiver::ErrorCloseCallback, this));
        m_socket->SetRecvCallback(MakeCallback(&QkdSecureReceiver::ReceivedDataCallback, this));
    }

    SwitchToState(STARTED);
}

void
QkdSecureReceiver::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_state == NOT_STARTED)
    {
        NS_LOG_WARN("QKD receiver application already stopped.");
        return;
    }

    SwitchToState(STOPPED);

    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                    MakeNullCallback<void, Ptr<Socket>, const Address&>());
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                    MakeNullCallback<void, Ptr<Socket>>());
        m_socket = nullptr;
    }

    if (m_socketKMA)
    {
        m_socketKMA->Close();
        m_socketKMA->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socketKMA->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                       MakeNullCallback<void, Ptr<Socket>>());
        m_socketKMA->SetConnectCallback(MakeNullCallback<void, Ptr<Socket>>(),
                                        MakeNullCallback<void, Ptr<Socket>>());
        m_socketKMA = nullptr;
    }
}

void
QkdSecureReceiver::SwitchToState(QkdAppState_t newState)
{
    NS_LOG_FUNCTION(this);
    if (m_state != newState)
    {
        NS_LOG_DEBUG("Switching state from " << GetStateString(m_state) << " to "
                                             << GetStateString(newState));
        const std::string oldStateString = GetStateString(m_state);
        const std::string newStateString = GetStateString(newState);
        m_state = newState;
        // Notify observers of the state change
        m_stateTransitionTrace(oldStateString, newStateString);
    }
    else
    {
        NS_LOG_DEBUG("State remains " << GetStateString(m_state));
    }
}

void
QkdSecureReceiver::SetKsid(uint32_t ksid)
{
    NS_LOG_FUNCTION(this);
    m_ksid = ksid;
}

uint32_t
QkdSecureReceiver::GetKsid() const
{
    NS_LOG_FUNCTION(this);
    return m_ksid;
}

void
QkdSecureReceiver::SetKey(const std::string& key)
{
    NS_LOG_FUNCTION(this << key);
    m_key = key;
}

std::string
QkdSecureReceiver::GetKey() const
{
    NS_LOG_FUNCTION(this);
    return m_key;
}

bool
QkdSecureReceiver::ConnectionRequestCallback(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);

    if (m_state != STARTED)
    {
        NS_LOG_WARN("Received connection request in state " << GetStateString() << ", ignoring.");
        return false;
    }
    return true;
}

void
QkdSecureReceiver::ConnectionCreatedCallback(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);

    if (m_state != STARTED)
    {
        NS_LOG_WARN("Received connection created in state " << GetStateString() << ", ignoring.");
        return;
    }

    socket->SetCloseCallbacks(MakeCallback(&QkdSecureReceiver::NormalCloseCallback, this),
                              MakeCallback(&QkdSecureReceiver::ErrorCloseCallback, this));
    socket->SetRecvCallback(MakeCallback(&QkdSecureReceiver::ReceivedDataCallback, this));

    m_socketReceiver = socket;

    m_socketReceiver->GetPeerName(m_peer);

    SwitchToState(AWAIT_KSID_FROM_SENDER);
}

void
QkdSecureReceiver::NormalCloseCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    if (m_state == STOPPED)
    {
        NS_LOG_INFO("QKD receiver application closed normally.");
        return;
    }

    NS_LOG_WARN("Received normal close in state " << GetStateString() << ", switching to STOPPED.");
    SwitchToState(STOPPED);
}

void
QkdSecureReceiver::ErrorCloseCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    if (m_state == STOPPED)
    {
        NS_LOG_INFO("QKD receiver application closed with error.");
        return;
    }

    NS_LOG_ERROR("Received error close in state " << GetStateString() << ", switching to STOPPED.");
    SwitchToState(STOPPED);
}

void
QkdSecureReceiver::ReceivedDataCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address from;
    while (auto packet = socket->RecvFrom(from))
    {
        NS_LOG_DEBUG("Received packet from receiver: " << from);
        if (packet->GetSize() == 0)
        {
            NS_LOG_DEBUG("Received empty packet, stopping receive.");
            break;
        }

        QkdAppHeader header;
        packet->RemoveHeader(header);

        switch (m_state)
        {
        case AWAIT_KSID_FROM_SENDER:
            if (header.GetHeaderType() == QkdAppHeaderType::QKD_APP_SEND_KSID &&
                header.GetKSID() != 0)
            {
                SetKsid(header.GetKSID());
                Simulator::ScheduleNow(&QkdSecureReceiver::SendKeySessionIdAck, this);
            }
            else
            {
                NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
            }
            break;
        case EXPECTING_REQUEST:
            ReceiveEncryptedData(packet, header);
            break;
        default:
            NS_FATAL_ERROR("Received data in unexpected state: " << GetStateString()
                                                                 << ", header: " << header);
            break;
        }
    }
}

void
QkdSecureReceiver::ConnectionSucceededKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    if (m_state != CONNECTING_KMA)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString()
                                        << " for ConnectionSucceededKMACallback().");
    }
    NS_ASSERT_MSG(m_socketKMA == socket, "Invalid socket in ConnectionSucceededKMACallback().");
    m_connectionEstablishedKMATrace(this);
    socket->SetRecvCallback(MakeCallback(&QkdSecureReceiver::ReceivedDataKMACallback, this));
    SwitchToState(REQUEST_OPEN_CONNECT);
    // Proceed with sending the open connect request to the KMA
    m_requestOpenConnectEvent =
        Simulator::ScheduleNow(&QkdSecureReceiver::RequestOpenConnect, this);
}

void
QkdSecureReceiver::ConnectionFailedKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    if (m_state == CONNECTING_KMA)
    {
        NS_LOG_ERROR("Sender failed to connect to remote address " << m_kmaPeer);
    }
    else
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ConnectionFailed().");
    }
}

void
QkdSecureReceiver::NormalCloseKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    CancelAllPendingEvents();
    if (socket->GetErrno() != Socket::ERROR_NOTERROR)
    {
        NS_LOG_ERROR("Socket closed with error: " << socket->GetErrno());
    }
    m_socketKMA->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                   MakeNullCallback<void, Ptr<Socket>>());
    m_connectionClosedKMATrace(this);
}

void
QkdSecureReceiver::ErrorCloseKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    CancelAllPendingEvents();
    if (socket->GetErrno() != Socket::ERROR_NOTERROR)
    {
        NS_LOG_ERROR("Socket closed with error: " << socket->GetErrno());
    }

    m_connectionClosedKMATrace(this);
}

void
QkdSecureReceiver::ReceivedDataKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Address from;
    while (auto packet = socket->RecvFrom(from))
    {
        NS_LOG_DEBUG("Received packet from KMA: " << from);
        if (packet->GetSize() == 0)
        {
            NS_LOG_DEBUG("Received empty packet, stopping receive.");
            break;
        }

        QkdAppHeader header;
        packet->RemoveHeader(header);

        m_rxKeyManagerAppTrace(packet, from);

        switch (m_state)
        {
        case AWAIT_KSID_FROM_KMA:
            if (header.GetHeaderType() == QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT)
            {
                ReceiveKeySessionId(header);
            }
            else
            {
                NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
            }
            break;
        case AWAIT_KEY:
            if (header.GetHeaderType() == QkdAppHeaderType::QKD_KEY_MANAGER_GET_KEY)
            {
                ReceiveKey(packet, header);
            }
            else
            {
                NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
            }
            break;
        case CLOSE_CONNECT:
            if (header.GetHeaderType() == QkdAppHeaderType::QKD_KEY_MANAGER_CLOSE)
            {
                NS_LOG_DEBUG("Received close confirmation from KMA, switching to STOPPED "
                             "state.");
                SwitchToState(STOPPED);
            }
            else
            {
                NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
            }
            break;
        default:
            NS_FATAL_ERROR("Received data in unexpected state: " << GetStateString()
                                                                 << ", header: " << header);
            break;
        }
    }
}

void
QkdSecureReceiver::RequestOpenConnect()
{
    NS_LOG_FUNCTION(this);
    if (m_state != REQUEST_OPEN_CONNECT)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for RequestOpenConnect().");
    }

    NS_ABORT_MSG_IF(m_socketKMA == nullptr, "Socket to KMA is not initialized");

    QkdAppHeader header;
    header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT);
    header.SetKSID(m_ksid);
    header.SetSource(m_local);
    header.SetDestination(m_peer);

    NS_LOG_DEBUG("Requesting open connect to KMA with KSID: " << m_ksid << ", source: " << m_local
                                                              << ", destination: " << m_peer);

    auto packet = Create<Packet>();
    packet->AddHeader(header);
    const auto packetSize = packet->GetSize();
    const auto actualSize = m_socketKMA->Send(packet);
    NS_LOG_DEBUG(this << " Sent open connect request to KMA with size " << packetSize
                      << ", actual size sent: " << actualSize);
    if (actualSize != static_cast<int>(packetSize))
    {
        NS_LOG_ERROR("Failed to send the entire packet to KMA. Expected size: "
                     << packetSize << ", actual size sent: " << actualSize);
    }
    else
    {
        SwitchToState(AWAIT_KSID_FROM_KMA);
        NS_LOG_DEBUG("Open connect request sent successfully to KMA.");
    }
}

void
QkdSecureReceiver::RequestGetKey()
{
    NS_LOG_FUNCTION(this);
    if (m_state != REQUEST_GET_KEY)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for RequestGetKey().");
    }

    NS_ABORT_MSG_IF(m_socketKMA == nullptr, "Socket to KMA is not initialized");

    QkdAppHeader header;
    header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_GET_KEY);
    header.SetKSID(m_ksid);
    header.SetSource(m_local);
    header.SetDestination(m_peer);

    auto packet = Create<Packet>();
    packet->AddHeader(header);
    const auto packetSize = packet->GetSize();
    const auto actualSize = m_socketKMA->Send(packet);
    NS_LOG_DEBUG(this << " Sent get key request to KMA with size " << packetSize
                      << ", actual size sent: " << actualSize);
    if (actualSize != static_cast<int>(packetSize))
    {
        NS_LOG_ERROR("Failed to send the entire packet to KMA. Expected size: "
                     << packetSize << ", actual size sent: " << actualSize);
    }
    else
    {
        SwitchToState(AWAIT_KEY);
        NS_LOG_DEBUG("Get key request sent successfully to KMA.");
    }
}

void
QkdSecureReceiver::RequestClose()
{
    NS_LOG_FUNCTION(this);
    if (m_state != CLOSE_CONNECT)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for RequestClose().");
    }

    NS_ABORT_MSG_IF(m_socketKMA == nullptr, "Socket to KMA is not initialized");

    QkdAppHeader header;
    header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_CLOSE);
    header.SetKSID(m_ksid);

    auto packet = Create<Packet>();
    packet->AddHeader(header);
    const auto packetSize = packet->GetSize();
    const auto actualSize = m_socketKMA->Send(packet);
    NS_LOG_DEBUG(this << " Sent close connect request to KMA with size " << packetSize
                      << ", actual size sent: " << actualSize);
    if (actualSize != static_cast<int>(packetSize))
    {
        NS_LOG_ERROR("Failed to send the entire packet to KMA. Expected size: "
                     << packetSize << ", actual size sent: " << actualSize);
    }
}

void
QkdSecureReceiver::SendKeySessionIdAck()
{
    NS_LOG_FUNCTION(this);
    if (m_state != AWAIT_KSID_FROM_SENDER)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for SendKeySessionIdAck().");
    }

    NS_ABORT_MSG_IF(m_socketReceiver == nullptr, "Socket to receiver is not initialized");

    QkdAppHeader header;
    header.SetHeaderType(QkdAppHeaderType::QKD_APP_SEND_KSID);
    header.SetKSID(m_ksid);

    auto packet = Create<Packet>();
    packet->AddHeader(header);
    const auto packetSize = packet->GetSize();
    const auto actualSize = m_socketReceiver->Send(packet);
    NS_LOG_DEBUG(this << " Sent Key Session ID ACK to sender with size " << packetSize
                      << ", actual size sent: " << actualSize);
    if (actualSize != static_cast<int>(packetSize))
    {
        NS_LOG_ERROR("Failed to send the entire packet to sender. Expected size: "
                     << packetSize << ", actual size sent: " << actualSize);
    }
    SwitchToState(CONNECTING_KMA);
    Simulator::ScheduleNow(&QkdSecureReceiver::OpenConnectionKMA, this);
}

void
QkdSecureReceiver::ReceiveKeySessionId(QkdAppHeader& header)
{
    NS_LOG_FUNCTION(this << header);

    if (header.GetHeaderType() != QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT)
    {
        NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
    }
    SetKsid(header.GetKSID());
    NS_LOG_DEBUG("Received Key Session ID: " << static_cast<int>(m_ksid));

    SwitchToState(REQUEST_GET_KEY);
    // Proceed with sending the Key Session ID to the receiver
    Simulator::Schedule(Minutes(7),
                        &QkdSecureReceiver::RequestGetKey,
                        this); // The 7 minutes delay is to allow Key to be generated
}

void
QkdSecureReceiver::ReceiveKey(Ptr<Packet> packet, QkdAppHeader& header)
{
    NS_LOG_FUNCTION(this << packet << header);
    if (m_state != AWAIT_KEY)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ReceiveKey().");
    }

    QkdAppTrailer trailer;
    packet->RemoveTrailer(trailer);

    if (header.GetHeaderType() != QkdAppHeaderType::QKD_KEY_MANAGER_GET_KEY)
    {
        NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
    }

    std::string key;
    key = trailer.GetKeyOrData();
    if (key.empty())
    {
        NS_FATAL_ERROR("Received empty key from KMA.");
    }
    SetKey(key);
    NS_LOG_DEBUG("Received Key: " << key);

    SwitchToState(EXPECTING_REQUEST);
}

void
QkdSecureReceiver::SendEncryptedData(const std::string& data)
{
    NS_LOG_FUNCTION(this << data);
    if (m_state != ENCRYPTING_DATA)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for SendEncryptedData().");
    }

    NS_ABORT_MSG_IF(m_socketReceiver == nullptr, "Socket to receiver is not initialized");

    // Encrypt the data using the key
    // For simplicity, we will just log the data and key here
    NS_LOG_DEBUG("Encrypting data: " << data << " with key: " << m_key);

    // Generate IV
    CryptoPP::AutoSeededRandomPool rng;
    std::string iv(CryptoPP::AES::BLOCKSIZE, 0);
    rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(iv.data()), iv.size());
    NS_LOG_DEBUG("Generated IV: " << iv);

    QkdAppHeader header;
    header.SetHeaderType(QkdAppHeaderType::QKD_APP_IV);
    header.SetKSID(m_ksid);
    header.SetEncryptionAlgo(SymmetricEncryptionAlgo::AES_128_GCM);
    header.SetIV(iv);

    NS_LOG_DEBUG("Header: " << header);

    // Encrypt the data
    std::string encryptedData;
    SymmetricEncryption encyptor;

    auto dataCopy = data;
    encryptedData =
        encyptor.encrypt(dataCopy, SymmetricEncryptionAlgo::AES_128_GCM, m_key.substr(0, 16), iv);

    QkdAppTrailer trailer;
    trailer.SetKeyOrData(encryptedData);

    auto packet = Create<Packet>(data.size());
    packet->AddHeader(header);
    packet->AddTrailer(trailer);
    const auto packetSize = packet->GetSize();
    const auto actualSize = m_socketReceiver->Send(packet);
    NS_ASSERT(packet->GetSize() < 1500); // Typical MTU limit
    NS_LOG_DEBUG(this << " Sent encrypted data to receiver with size " << packetSize
                      << ", actual size sent: " << actualSize);

    if (actualSize != static_cast<int>(packetSize))
    {
        NS_LOG_ERROR("Failed to send the entire packet to receiver. Expected size: "
                     << packetSize << ", actual size sent: " << actualSize);
    }
    else
    {
        SwitchToState(CLOSE_CONNECT);
        // Proceed with sending the close request to the KMA
        Simulator::ScheduleNow(&QkdSecureReceiver::RequestClose, this);
        NS_LOG_DEBUG("Encrypted data sent successfully to receiver.");
    }
}

void
QkdSecureReceiver::ReceiveEncryptedData(Ptr<Packet> packet, QkdAppHeader& header)
{
    NS_LOG_FUNCTION(this << packet << header);
    if (m_state != EXPECTING_REQUEST)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ReceiveEncryptedData().");
    }

    QkdAppTrailer trailer;
    packet->RemoveTrailer(trailer);

    if (header.GetHeaderType() != QkdAppHeaderType::QKD_APP_IV)
    {
        NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
    }

    // Decrypt the data using the key
    std::string decryptedData;
    SymmetricEncryption decryptor;

    auto encryptedData = trailer.GetKeyOrData();
    decryptedData = decryptor.decrypt(encryptedData,
                                      header.GetEncryptionAlgo(),
                                      m_key.substr(0, 16),
                                      header.GetIV());

    NS_LOG_DEBUG("Decrypted Data: " << decryptedData);

    // Process the decrypted data as needed
    std::string msg = "Hello from Receiver!";
    SwitchToState(ENCRYPTING_DATA);
    Simulator::ScheduleNow(&QkdSecureReceiver::SendEncryptedData, this, msg);
}

void
QkdSecureReceiver::OpenConnectionKMA()
{
    NS_LOG_FUNCTION(this);
    if (m_state != CONNECTING_KMA)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for OpenConnectionKMA().");
    }

    m_socketKMA = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    NS_ABORT_MSG_IF(m_kmaPeer.IsInvalid(), "Remote KMA address not properly set");
    if (!m_local.IsInvalid())
    {
        NS_ABORT_MSG_IF((Inet6SocketAddress::IsMatchingType(m_kmaPeer) &&
                         InetSocketAddress::IsMatchingType(m_local)) ||
                            (InetSocketAddress::IsMatchingType(m_kmaPeer) &&
                             Inet6SocketAddress::IsMatchingType(m_local)),
                        "Incompatible KMA peer and local address IP version");
    }
    if (InetSocketAddress::IsMatchingType(m_kmaPeer))
    {
        const auto ret [[maybe_unused]] =
            m_local.IsInvalid() ? m_socketKMA->Bind() : m_socketKMA->Bind(m_local);
        NS_LOG_DEBUG(this << " Bind() return value= " << ret
                          << " GetErrNo= " << m_socketKMA->GetErrno() << ".");

        const auto ipv4 = InetSocketAddress::ConvertFrom(m_kmaPeer).GetIpv4();
        const auto port = InetSocketAddress::ConvertFrom(m_kmaPeer).GetPort();
        NS_LOG_INFO(this << " Connecting to KMA at " << ipv4 << " port " << port << " / "
                         << m_kmaPeer << ".");
    }
    else if (Inet6SocketAddress::IsMatchingType(m_kmaPeer))
    {
        const auto ret [[maybe_unused]] =
            m_local.IsInvalid() ? m_socketKMA->Bind6() : m_socketKMA->Bind(m_local);
        NS_LOG_DEBUG(this << " Bind6() return value= " << ret
                          << " GetErrNo= " << m_socketKMA->GetErrno() << ".");

        const auto ipv6 = Inet6SocketAddress::ConvertFrom(m_kmaPeer).GetIpv6();
        const auto port = Inet6SocketAddress::ConvertFrom(m_kmaPeer).GetPort();
        NS_LOG_INFO(this << " Connecting to KMA at " << ipv6 << " port " << port << " / "
                         << m_kmaPeer << ".");
    }
    else
    {
        NS_ASSERT_MSG(false, "Incompatible KMA address type: " << m_kmaPeer);
    }
    const auto ret [[maybe_unused]] = m_socketKMA->Connect(m_kmaPeer);
    NS_LOG_DEBUG(this << " Connect() return value= " << ret
                      << " GetErrNo= " << m_socketKMA->GetErrno() << ".");
    NS_ASSERT_MSG(m_socketKMA, "Failed to create KMA socket");
    SwitchToState(CONNECTING_KMA);
    m_socketKMA->SetConnectCallback(
        MakeCallback(&QkdSecureReceiver::ConnectionSucceededKMACallback, this),
        MakeCallback(&QkdSecureReceiver::ConnectionFailedKMACallback, this));
    m_socketKMA->SetCloseCallbacks(MakeCallback(&QkdSecureReceiver::NormalCloseKMACallback, this),
                                   MakeCallback(&QkdSecureReceiver::ErrorCloseKMACallback, this));
    m_socketKMA->SetRecvCallback(MakeCallback(&QkdSecureReceiver::ReceivedDataKMACallback, this));
    NS_LOG_DEBUG(this << " Opened connection to KMA at " << m_kmaPeer << " with socket "
                      << m_socketKMA);
}

void
QkdSecureReceiver::CancelAllPendingEvents()
{
    NS_LOG_FUNCTION(this);
    if (!Simulator::IsExpired(m_requestOpenConnectEvent))
    {
        NS_LOG_DEBUG("Cancelling pending open connect request event.");
        Simulator::Cancel(m_requestOpenConnectEvent);
    }
    if (!Simulator::IsExpired(m_requestGetKeyEvent))
    {
        NS_LOG_DEBUG("Cancelling pending get key request event.");
        Simulator::Cancel(m_requestGetKeyEvent);
    }
    if (!Simulator::IsExpired(m_requestCloseEvent))
    {
        NS_LOG_DEBUG("Cancelling pending close request event.");
        Simulator::Cancel(m_requestCloseEvent);
    }
    if (!Simulator::IsExpired(m_sendEncryptedDataEvent))
    {
        NS_LOG_DEBUG("Cancelling pending send encrypted data event.");
        Simulator::Cancel(m_sendEncryptedDataEvent);
    }
}

} // namespace ns3
