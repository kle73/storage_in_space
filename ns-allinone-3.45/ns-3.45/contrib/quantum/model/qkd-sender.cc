/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-sender.h"

#include "ns3/address-utils.h"
#include "ns3/callback.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/symmetric-encryption.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"

#include <cryptopp/cryptlib.h>
#include <cryptopp/osrng.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QkdSecureSender");
NS_OBJECT_ENSURE_REGISTERED(QkdSecureSender);

TypeId
QkdSecureSender::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QkdSecureSender")
                            .SetParent<SourceApplication>()
                            .SetGroupName("Applications")
                            .AddConstructor<QkdSecureSender>()
                            .AddAttribute("RemoteKMA",
                                          "The address of the Key Manager Application (KMA) to "
                                          "which the QKD "
                                          "sender will send packets.",
                                          AddressValue(),
                                          MakeAddressAccessor(&QkdSecureSender::SetRemoteKMA),
                                          MakeAddressChecker());
    // Add trace sources and attributes here
    return tid;
}

QkdSecureSender::QkdSecureSender()
    : m_state{NOT_STARTED},
      m_socket{nullptr},
      m_socketKMA{nullptr},
      m_ksid{0},
      m_key{},
      m_peerPort{},
      m_kmaPeerPort{}
{
    NS_LOG_FUNCTION(this);
}

QkdSecureSender::~QkdSecureSender()
{
    NS_LOG_FUNCTION(this);
}

void
QkdSecureSender::SetRemote(const Address& addr)
{
    NS_LOG_FUNCTION(this << addr);
    if (!addr.IsInvalid())
    {
        m_peer = addr;
    }
}

void
QkdSecureSender::SetRemoteKMA(const Address& addr)
{
    NS_LOG_FUNCTION(this << addr);
    if (!addr.IsInvalid())
    {
        m_kmaPeer = addr;
    }
}

Ptr<Socket>
QkdSecureSender::GetSocket() const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

Ptr<Socket>
QkdSecureSender::GetSocketKMA() const
{
    NS_LOG_FUNCTION(this);
    return m_socketKMA;
}

QkdSecureSender::QkdAppState_t
QkdSecureSender::GetState() const
{
    NS_LOG_FUNCTION(this);
    return m_state;
}

std::string
QkdSecureSender::GetStateString() const
{
    NS_LOG_FUNCTION(this);
    return GetStateString(m_state);
}

std::string
QkdSecureSender::GetStateString(QkdAppState_t state)
{
    NS_LOG_FUNCTION(state);
    switch (state)
    {
    case NOT_STARTED:
        return "NOT_STARTED";
    case CONNECTING_RECEIVER:
        return "CONNECTING_RECEIVER";
    case CONNECTING_KMA:
        return "CONNECTING_KMA";
    case REQUEST_OPEN_CONNECT:
        return "REQUEST_OPEN_CONNECT";
    case AWAIT_KSID:
        return "AWAIT_KSID";
    case SEND_KSID_TO_RECEIVER:
        return "SEND_KSID_TO_RECEIVER";
    case AWAIT_KSID_ACK:
        return "AWAIT_KSID_ACK";
    case REQUEST_GET_KEY:
        return "REQUEST_GET_KEY";
    case AWAIT_KEY:
        return "AWAIT_KEY";
    case CLOSE_CONNECT:
        return "CLOSE_CONNECT";
    case EXPECTING_ACK:
        return "EXPECTING_ACK";
    case ENCRYPTING_DATA:
        return "ENCRYPTING_DATA";
    case STOPPED:
        return "STOPPED";
    default:
        NS_FATAL_ERROR("Unknown state: " << state);
        return "UNKNOWN_STATE";
    }
}

void
QkdSecureSender::DoDispose()
{
    NS_LOG_FUNCTION(this);

    if (!Simulator::IsFinished())
    {
        StopApplication();
    }

    // Clean up the sockets
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    if (m_socketKMA)
    {
        m_socketKMA->Close();
        m_socketKMA = nullptr;
    }

    SourceApplication::DoDispose(); // Chain up to the parent class
}

void
QkdSecureSender::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_state == NOT_STARTED)
    {
        OpenConnectionReceiver();
    }
    else
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for StartApplication().");
    }
}

void
QkdSecureSender::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_state == STOPPED)
    {
        NS_LOG_DEBUG("QKD sender application already stopped.");
        return;
    }

    SwitchToState(STOPPED);
    CancelAllPendingEvents();
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->SetConnectCallback(MakeNullCallback<void, Ptr<Socket>>(),
                                     MakeNullCallback<void, Ptr<Socket>>());
        m_socket = nullptr;
    }
    if (m_socketKMA)
    {
        m_socketKMA->Close();
        m_socketKMA->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socketKMA->SetConnectCallback(MakeNullCallback<void, Ptr<Socket>>(),
                                        MakeNullCallback<void, Ptr<Socket>>());
        m_socketKMA = nullptr;
    }
}

void
QkdSecureSender::SwitchToState(QkdAppState_t newState)
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
QkdSecureSender::SetKsid(uint32_t ksid)
{
    NS_LOG_FUNCTION(this);
    m_ksid = ksid;
}

uint32_t
QkdSecureSender::GetKsid() const
{
    NS_LOG_FUNCTION(this);
    return m_ksid;
}

void
QkdSecureSender::SetKey(const std::string& key)
{
    NS_LOG_FUNCTION(this << key);
    m_key = key;
}

std::string
QkdSecureSender::GetKey() const
{
    NS_LOG_FUNCTION(this);
    return m_key;
}

void
QkdSecureSender::ConnectionSucceededReceiverCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    if (m_state != CONNECTING_RECEIVER)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString()
                                        << " for ConnectionSucceededReceiverCallback().");
    }
    NS_ASSERT_MSG(m_socket == socket, "Invalid socket in ConnectionSucceededReceiverCallback().");
    m_connectionEstablishedReceiverTrace(this);
    socket->SetRecvCallback(MakeCallback(&QkdSecureSender::ReceivedDataReceiverCallback, this));
    SwitchToState(CONNECTING_KMA);
    // Proceed with sending the open connect request to the receiver
    Simulator::ScheduleNow(&QkdSecureSender::OpenConnectionKMA, this);
}

void
QkdSecureSender::ConnectionFailedReceiverCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    if (m_state == CONNECTING_RECEIVER)
    {
        NS_LOG_ERROR("Sender failed to connect to remote address " << m_peer);
    }
    else
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ConnectionFailed().");
    }
}

void
QkdSecureSender::NormalCloseReceiverCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    CancelAllPendingEvents();
    if (socket->GetErrno() != Socket::ERROR_NOTERROR)
    {
        NS_LOG_ERROR("Socket closed with error: " << socket->GetErrno());
    }
    m_socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                MakeNullCallback<void, Ptr<Socket>>());
    m_connectionClosedReceiverTrace(this);
}

void
QkdSecureSender::ErrorCloseReceiverCallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    CancelAllPendingEvents();
    if (socket->GetErrno() != Socket::ERROR_NOTERROR)
    {
        NS_LOG_ERROR("Socket closed with error: " << socket->GetErrno());
    }

    m_connectionClosedReceiverTrace(this);
}

void
QkdSecureSender::ReceivedDataReceiverCallback(Ptr<Socket> socket)
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
        case AWAIT_KSID_ACK:
            if (header.GetHeaderType() == QkdAppHeaderType::QKD_APP_SEND_KSID)
            {
                SwitchToState(REQUEST_GET_KEY);
                Simulator::Schedule(Minutes(7),
                                    &QkdSecureSender::RequestGetKey,
                                    this); // Schedule the request for the key after 7 minutes
                // This is to allow a key to be generated and added to the buffer
            }
            else
            {
                NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
            }
            break;
        case EXPECTING_ACK:
            ReceiveEncryptedData(packet, header);
            break;
        default:
            NS_FATAL_ERROR("Received data in unexpected state: " << GetStateString()
                                                                 << " header: " << header);
            break;
        }
    }
}

void
QkdSecureSender::ConnectionSucceededKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    if (m_state != CONNECTING_KMA)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString()
                                        << " for ConnectionSucceededKMACallback().");
    }
    NS_ASSERT_MSG(m_socketKMA == socket, "Invalid socket in ConnectionSucceededKMACallback().");
    m_connectionEstablishedKMATrace(this);
    socket->SetRecvCallback(MakeCallback(&QkdSecureSender::ReceivedDataKMACallback, this));
    SwitchToState(REQUEST_OPEN_CONNECT);
    // Proceed with sending the open connect request to the KMA
    m_requestOpenConnectEvent = Simulator::ScheduleNow(&QkdSecureSender::RequestOpenConnect, this);
}

void
QkdSecureSender::ConnectionFailedKMACallback(Ptr<Socket> socket)
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
QkdSecureSender::NormalCloseKMACallback(Ptr<Socket> socket)
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
QkdSecureSender::ErrorCloseKMACallback(Ptr<Socket> socket)
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
QkdSecureSender::ReceivedDataKMACallback(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    if (m_state != AWAIT_KSID && m_state != AWAIT_KEY && m_state != CLOSE_CONNECT)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ReceivedDataKMACallback().");
    }

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

        NS_LOG_DEBUG("Received header type: " << header.GetHeaderType()
                                              << ", KSID: " << header.GetKSID()
                                              << ", Source: " << header.GetSource()
                                              << ", Destination: " << header.GetDestination());

        m_rxKeyManagerAppTrace(packet, from);

        switch (m_state)
        {
        case AWAIT_KSID:
            if (header.GetHeaderType() == QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT)
            {
                ReceiveKeySessionId(header);
            }
            else
            {
                NS_FATAL_ERROR("Received unexpected header type: "
                               << header.GetHeaderType() << " instead of expected header type: "
                               << QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT);
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
                NS_LOG_DEBUG("Received close connect response from KMA.");
                m_connectionClosedKMATrace(this);
                SwitchToState(STOPPED);
            }
            else
            {
                NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
            }
            break;
        default:
            NS_FATAL_ERROR("Received data in unexpected state: " << GetStateString());
            break;
        }
    }
}

void
QkdSecureSender::RequestOpenConnect()
{
    NS_LOG_FUNCTION(this);
    if (m_state != REQUEST_OPEN_CONNECT)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for RequestOpenConnect().");
    }

    NS_ABORT_MSG_IF(m_socketKMA == nullptr, "Socket to KMA is not initialized");

    QkdAppHeader header;
    header.SetKSID(0); // KSID is not set yet, will be set after KMA responds
    header.SetHeaderType(QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT);
    header.SetSource(m_local);
    header.SetDestination(m_peer);

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
        SwitchToState(AWAIT_KSID);
        NS_LOG_DEBUG("Open connect request sent successfully to KMA.");
    }
}

void
QkdSecureSender::RequestGetKey()
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
QkdSecureSender::RequestClose()
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
    header.SetSource(m_local);
    header.SetDestination(m_peer);

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
QkdSecureSender::ReceiveKeySessionId(QkdAppHeader& header)
{
    NS_LOG_FUNCTION(this << header);
    if (m_state != AWAIT_KSID)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for ReceiveKeySessionId().");
    }

    if (header.GetHeaderType() != QkdAppHeaderType::QKD_KEY_MANAGER_OPEN_CONNECT)
    {
        NS_FATAL_ERROR("Received unexpected header type: " << header.GetHeaderType());
    }
    SetKsid(header.GetKSID());
    NS_LOG_DEBUG("Received Key Session ID: " << static_cast<int>(m_ksid));

    SwitchToState(SEND_KSID_TO_RECEIVER);
    // Proceed with sending the Key Session ID to the receiver
    Simulator::ScheduleNow(&QkdSecureSender::SendKeySessionId, this);
}

void
QkdSecureSender::ReceiveKey(Ptr<Packet> packet, QkdAppHeader& header)
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

    SwitchToState(ENCRYPTING_DATA);
    // Proceed with sending data to the receiver
    Simulator::ScheduleNow(&QkdSecureSender::SendEncryptedData, this, std::string("Hello There!"));
}

void
QkdSecureSender::SendEncryptedData(const std::string& data)
{
    NS_LOG_FUNCTION(this << data);
    if (m_state != ENCRYPTING_DATA)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for SendEncryptedData().");
    }

    NS_ABORT_MSG_IF(m_socket == nullptr, "Socket to receiver is not initialized");

    // Encrypt the data using the key
    // For simplicity, we will just log the data and key here
    NS_LOG_DEBUG("Encrypting data: " << data << " with key: " << m_key);

    // Generate IV
    CryptoPP::AutoSeededRandomPool rng;
    std::string iv(CryptoPP::AES::BLOCKSIZE, 0);
    rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(iv.data()), iv.size());
    NS_LOG_DEBUG("Generated IV: " << iv);

    QkdAppHeader header;
    header.SetKSID(m_ksid);
    header.SetIV(iv);
    header.SetHeaderType(QkdAppHeaderType::QKD_APP_IV);
    header.SetEncryptionAlgo(SymmetricEncryptionAlgo::AES_128_GCM);

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
    const auto actualSize = m_socket->Send(packet);

    NS_LOG_DEBUG(this << " Sent encrypted data to receiver with size " << packetSize
                      << ", actual size sent: " << actualSize);

    if (actualSize != static_cast<int>(packetSize))
    {
        NS_LOG_ERROR("Failed to send the entire packet to receiver. Expected size: "
                     << packetSize << ", actual size sent: " << actualSize);
    }
    else
    {
        SwitchToState(EXPECTING_ACK);
        NS_LOG_DEBUG("Encrypted data sent successfully to receiver.");
    }
}

void
QkdSecureSender::SendKeySessionId()
{
    NS_LOG_FUNCTION(this);
    if (m_state != SEND_KSID_TO_RECEIVER)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for SendKeySessionId().");
    }

    NS_ABORT_MSG_IF(m_socket == nullptr, "Socket to receiver is not initialized");

    QkdAppHeader header;
    header.SetHeaderType(QkdAppHeaderType::QKD_APP_SEND_KSID);
    header.SetKSID(m_ksid);

    auto packet = Create<Packet>();
    packet->AddHeader(header);
    const auto packetSize = packet->GetSize();
    const auto actualSize = m_socket->Send(packet);
    NS_LOG_DEBUG(this << " Sent Key Session ID to receiver with size " << packetSize
                      << ", actual size sent: " << actualSize);
    if (actualSize != static_cast<int>(packetSize))
    {
        NS_LOG_ERROR("Failed to send the entire packet to receiver. Expected size: "
                     << packetSize << ", actual size sent: " << actualSize);
    }
    else
    {
        SwitchToState(AWAIT_KSID_ACK);
        NS_LOG_DEBUG("Key Session ID sent successfully to receiver.");
    }
}

void
QkdSecureSender::ReceiveEncryptedData(Ptr<Packet> packet, QkdAppHeader& header)
{
    NS_LOG_FUNCTION(this << packet << header);
    if (m_state != EXPECTING_ACK)
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
                                      SymmetricEncryptionAlgo::AES_128_GCM,
                                      m_key.substr(0, 16), // AES-128 requires a 16-byte key
                                      header.GetIV());

    NS_LOG_DEBUG("Decrypted Data: " << decryptedData);

    // Process the decrypted data as needed
    // Close the secure connection after processing
    SwitchToState(CLOSE_CONNECT);
    Simulator::ScheduleNow(&QkdSecureSender::RequestClose, this);
    NS_LOG_DEBUG("Encrypted data received and processed successfully.");
}

void
QkdSecureSender::OpenConnectionReceiver()
{
    NS_LOG_FUNCTION(this);
    if (m_state != NOT_STARTED)
    {
        NS_FATAL_ERROR("Invalid state " << GetStateString() << " for OpenConnectionReceiver().");
    }

    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    NS_ABORT_MSG_IF(m_peer.IsInvalid(), "Remote address not properly set");
    if (!m_local.IsInvalid())
    {
        NS_ABORT_MSG_IF((Inet6SocketAddress::IsMatchingType(m_peer) &&
                         InetSocketAddress::IsMatchingType(m_local)) ||
                            (InetSocketAddress::IsMatchingType(m_peer) &&
                             Inet6SocketAddress::IsMatchingType(m_local)),
                        "Incompatible peer and local address IP version");
    }
    if (InetSocketAddress::IsMatchingType(m_peer))
    {
        const auto ret [[maybe_unused]] =
            m_local.IsInvalid() ? m_socket->Bind() : m_socket->Bind(m_local);
        NS_LOG_DEBUG(this << " Bind() return value= " << ret
                          << " GetErrNo= " << m_socket->GetErrno() << ".");

        const auto ipv4 = InetSocketAddress::ConvertFrom(m_peer).GetIpv4();
        const auto port = InetSocketAddress::ConvertFrom(m_peer).GetPort();
        NS_LOG_INFO(this << " Connecting to " << ipv4 << " port " << port << " / " << m_peer
                         << ".");
        // m_socket->SetIpTos(m_tos);
    }
    else if (Inet6SocketAddress::IsMatchingType(m_peer))
    {
        const auto ret [[maybe_unused]] =
            m_local.IsInvalid() ? m_socket->Bind6() : m_socket->Bind(m_local);
        NS_LOG_DEBUG(this << " Bind6() return value= " << ret
                          << " GetErrNo= " << m_socket->GetErrno() << ".");

        const auto ipv6 = Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6();
        const auto port = Inet6SocketAddress::ConvertFrom(m_peer).GetPort();
        NS_LOG_INFO(this << " Connecting to " << ipv6 << " port " << port << " / " << m_peer
                         << ".");
    }
    else
    {
        NS_ASSERT_MSG(false, "Incompatible address type: " << m_peer);
    }

    const auto ret [[maybe_unused]] = m_socket->Connect(m_peer);
    NS_LOG_DEBUG(this << " Connect() return value= " << ret << " GetErrNo= " << m_socket->GetErrno()
                      << ".");

    NS_ASSERT_MSG(m_socket, "Failed to create socket");

    SwitchToState(CONNECTING_RECEIVER);

    m_socket->SetConnectCallback(
        MakeCallback(&QkdSecureSender::ConnectionSucceededReceiverCallback, this),
        MakeCallback(&QkdSecureSender::ConnectionFailedReceiverCallback, this));
    m_socket->SetCloseCallbacks(MakeCallback(&QkdSecureSender::NormalCloseReceiverCallback, this),
                                MakeCallback(&QkdSecureSender::ErrorCloseReceiverCallback, this));
    m_socket->SetRecvCallback(MakeCallback(&QkdSecureSender::ReceivedDataReceiverCallback, this));
    NS_LOG_DEBUG(this << " Opened connection to receiver at " << m_peer << " with socket "
                      << m_socket);
}

void
QkdSecureSender::OpenConnectionKMA()
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
        m_socketKMA->SetIpTos(m_tos);
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
    m_socketKMA->SetConnectCallback(
        MakeCallback(&QkdSecureSender::ConnectionSucceededKMACallback, this),
        MakeCallback(&QkdSecureSender::ConnectionFailedKMACallback, this));
    m_socketKMA->SetCloseCallbacks(MakeCallback(&QkdSecureSender::NormalCloseKMACallback, this),
                                   MakeCallback(&QkdSecureSender::ErrorCloseKMACallback, this));
    m_socketKMA->SetRecvCallback(MakeCallback(&QkdSecureSender::ReceivedDataKMACallback, this));
    NS_LOG_DEBUG(this << " Opened connection to KMA at " << m_kmaPeer << " with socket "
                      << m_socketKMA);
}

void
QkdSecureSender::CancelAllPendingEvents()
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
