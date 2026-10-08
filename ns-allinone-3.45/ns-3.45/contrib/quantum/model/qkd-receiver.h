/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 *
 */
#ifndef QKD_RECEIVER_H
#define QKD_RECEIVER_H

#include "qkd-app-header.h"
#include "qkd-app-trailer.h"

#include "ns3/address.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/sink-application.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"

#include <optional>

namespace ns3
{
/**
 * @ingroup quantum
 *
 * This application simulates a Quantum Key Distribution (QKD) receiver.
 *
 * The QKD receiver application is responsible for establishing a connection with a QKD sender
 * and the Qkd Key Manager Application (KMA) to receive quantum keys and encrypted data.
 *
 * It handles communication with the QKD sender and the KMA, manages the state of the QKD session,
 * and provides trace sources for various events such as connection establishment, key reception,
 * and encrypted data reception.
 */
class QkdSecureReceiver : public SinkApplication
{
  public:
    /**
     * Creates a new instance of QKD receiver application.
     */
    QkdSecureReceiver();

    /**
     * Destructor for the QKD receiver application.
     */
    ~QkdSecureReceiver() override;

    /**
     * Returns the object TypeId.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Sets the remote address of the Key Manager Application (KMA).
     * @param addr The remote address of the KMA to which the QKD receiver will send packets.
     */
    void SetRemoteKMA(const Address& addr);

    /**
     * Returns the socket associated with the QKD receiver and the Key Manager Application (KMA).
     * @return Pointer to the associated KMA socket.
     */
    Ptr<Socket> GetSocketKMA() const;

    /// Possible states of the QKD receiver application.
    enum QkdAppState_t
    {
        /// Before StartApplication() is invoked.
        NOT_STARTED = 0,
        /// After StartApplication() is invoked and before the connection to the KMA is established.
        STARTED,
        /// Awaiting KSID from Sender Application.
        AWAIT_KSID_FROM_SENDER,
        /// Sent the Key Manager Application a connection request and waiting for the KMA to accept
        /// it.
        CONNECTING_KMA,
        /// To send the KMA a request to start a secure session.
        REQUEST_OPEN_CONNECT,
        // Sent the KMA a request to start a secure session and waiting for the KMA to respond.
        AWAIT_KSID_FROM_KMA,
        /// To send the KMA a request to send a quantum key.
        REQUEST_GET_KEY,
        /// Sent the KMA a request to send a quantum key and waiting for the KMA to respond.
        AWAIT_KEY,
        /// Sent the KMA a request to close the secure session.
        CLOSE_CONNECT,
        /// Waiting for a request from QKD sender.
        EXPECTING_REQUEST,
        /// Encrypting data with the received quantum key and sending it to the receiver.
        ENCRYPTING_DATA,
        /// After StopApplication() is invoked.
        STOPPED
    };

    /**
     * Returns the current state of the QKD receiver application.
     * @return The current state of the QKD receiver application.
     */
    QkdAppState_t GetState() const;

    /**
     * Returns the current state of the QKD receiver application in string format.
     * @return The current state of the QKD receiver application in string format.
     */
    std::string GetStateString() const;

    /**
     * Returns the given state in string format.
     * @param state An arbitrary state of a QKD receiver application.
     * @return The given state equivalently expressed in string format.
     */
    static std::string GetStateString(QkdAppState_t state);

    /**
     * Common callback signature for 'ConnectionEstablished'and 'RxKeyManagerApp', and
     * 'RxEmbeddedObject' trace sources.
     * @param qkdReceiver Pointer to this instance of QkdSecureReceiver,
     *                               which is where the trace originated.
     */
    typedef void (*TracedCallback)(Ptr<const QkdSecureReceiver> qkdReceiver);

    /**
     * Callback signature for 'RxKey' trace sources.
     * @param qkdReceiver Pointer to this instance of QkdSecureReceiver,
     *                               which is where the trace originated.
     * @param time Elapse time from the start to the end of the request.
     * @param key The quantum key received from the receiver.
     * @param keySize The size of the quantum key in bits.
     */
    typedef void (*RxKMATracedCallback)(Ptr<const QkdSecureReceiver> qkdReceiver,
                                        const Time& time,
                                        const std::string& key,
                                        uint32_t keySize);

    /**
     * Callback signature for 'RxData' trace sources.
     * @param qkdReceiver Pointer to this instance of QkdSecureReceiver,
     *                              which is where the trace originated.
     * @param time Elapse time from the start to the end of the request.
     * @param data The data received from the receiver.
     * @param dataSize The size of the data in bytes.
     */
    typedef void (*RxDataTracedCallback)(Ptr<const QkdSecureReceiver> qkdReceiver,
                                         const Time& time,
                                         const std::string& data,
                                         uint32_t dataSize);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;
    void SetLocal(const Address& addr) override;

    /**
     * Sets the Key Session ID for the QKD receiver application.
     * @param keySessionId The Key Session ID to be set.
     */
    void SetKsid(uint32_t keySessionId);

    /**
     * Returns the Key Session ID for the QKD receiver application.
     * @return The Key Session ID.
     */
    uint32_t GetKsid() const;

    /**
     * Sets the quantum key for the QKD receiver application.
     * @param key The quantum key to be set.
     */
    void SetKey(const std::string& key);

    /**
     * Returns the quantum key for the QKD receiver application.
     * @return The quantum key.
     */
    std::string GetKey() const;

    // Socket Callbacks for Sender and KMA

    /**
     * Invoked when the receiver socket receives a connection request from the QKD sender.
     * @param socket Pointer to the socket where the event originates from.
     * @param from The address of the sender that initiated the connection request.
     * @returns True if the connection request is accepted, false otherwise.
     */
    bool ConnectionRequestCallback(Ptr<Socket> socket, const Address& from);

    /**
     * Invoked when a connection is established on m_socket to the receiver.
     * This triggers the OpenConnectionKMA() to connect to the Key Manager Application (KMA).
     * @param socket Pointer to the socket where the event originates from.
     * @param from The address of the sender that initiated the connection.
     */
    void ConnectionCreatedCallback(Ptr<Socket> socket, const Address& from);

    /**
     * Invoked when connect with the sender is terminated normally.
     * @param socket Pointer to the socket where the event originates from.
     */
    void NormalCloseCallback(Ptr<Socket> socket);

    /**
     * Invoked when connect with the sender is terminated abnormally.
     * Error will be logged, but simulation continues.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ErrorCloseCallback(Ptr<Socket> socket);

    /**
     * Invoked when m_socket receives some packet data from the sender.
     * Fires the 'RxEncryptedData' trace source and triggers ReceiveEncryptedData().
     * @param socket Pointer to the socket where the event originates from.
     */
    void ReceivedDataCallback(Ptr<Socket> socket);

    /**
     * Invoked when a connection is established on m_socket to the Key Manager Application (KMA).
     * This triggers the RequestOpenConnect() to the KMA.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ConnectionSucceededKMACallback(Ptr<Socket> socket);

    /**
     * Invoked when a connection fails on m_socket to the KMA.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ConnectionFailedKMACallback(Ptr<Socket> socket);

    /**
     * Invoked when connection between m_socket and the KMA is closed.
     * @param socket Pointer to the socket where the event originates from.
     */
    void NormalCloseKMACallback(Ptr<Socket> socket);

    /**
     * Invoked when connection between m_socket and the KMA is terminated abnormally.
     * Error will be logged, but simulation continues.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ErrorCloseKMACallback(Ptr<Socket> socket);

    /**
     * Invoked when m_socket receives some packet data from the KMA.
     * Fires the 'RxKeyManagerApp' trace source and triggers ReceiveKeySession() or ReceiveKey().
     * @param socket Pointer to the socket where the event originates from.
     */
    void ReceivedDataKMACallback(Ptr<Socket> socket);

    // Connection-related methods

    /**
     * Initialize m_socket to connect to the Key Manager Application (KMA) at m_kmaPeer
     * and set up callbacks to listen to its event. Invoked upon the start of the application.
     * This method is responsible for establishing the connection with the KMA.
     */
    void OpenConnectionKMA();

    // Transmission-related methods

    /** Sends a request to the Key Manager Application to start a secure session with
     * the receiver.
     * Fires the 'TxOpenConnectRequest' trace source.
     */
    void RequestOpenConnect();

    /**
     * Sends a request to the Key Manager Application to get a quantum key.
     * Fires the 'TxGetKeyRequest' trace source.
     */
    void RequestGetKey();

    /**
     * Sends a request to the Key Manager Application to close the secure session.
     * Fires the 'TxCloseConnectRequest' trace source.
     */
    void RequestClose();

    /**
     * Sends encrypted data to the sender.
     * Fires the 'TxEncryptedData' trace source.
     * @param data The data to be sent, encrypted with the quantum key.
     */
    void SendEncryptedData(const std::string& data);

    /**
     * Sends a Key Session ID Acknowledgment to the Sender Application.
     */
    void SendKeySessionIdAck();

    // Reception-related methods
    /**
     * Receives encrypted data from the sender.
     * Fires the 'RxEncryptedData' trace source.
     * @param packet The packet containing the encrypted data.
     * @param header The header of the packet containing the encrypted data.
     */
    void ReceiveEncryptedData(Ptr<Packet> packet, QkdAppHeader& header);

    /**
     * Receives a key session ID from the Key Manager Application.
     * Fires the 'RxKeySessionId' trace source.
     * @param packet The packet containing the key session ID.
     */
    void ReceiveKeySessionId(QkdAppHeader& header);

    /**
     * Receives a quantum key from the Key Manager Application.
     * Fires the 'RxKey' trace source.
     * @param packet The packet containing the quantum key.
     * @param header The header of the packet containing the quantum key.
     */
    void ReceiveKey(Ptr<Packet> packet, QkdAppHeader& header);

    // Other methods
    /**
     * Cancel all pending events related to the QKD receiver application.
     * This method is invoked when the application is stopped or when a connection
     * is terminated.
     */
    void CancelAllPendingEvents();

    /**
     * Change the state of the QKD receiver application.
     * Fires the 'StateTransition' trace source.
     * @param state The new state to transition to.
     */
    void SwitchToState(QkdAppState_t state);

    /// Variables
    QkdAppState_t m_state; //!< The current state of the QKD receiver application.
    Ptr<Socket> m_socket;  //!< The socket for sending and receiving packets to/from the sender.
    Ptr<Socket> m_socketReceiver; //!< The socket for receiving packets from the sender.
    Ptr<Socket> m_socketKMA;      //!< The socket for sending and receiving packets to/from the KMA.
    Address m_local;              //!< The local address of the receiver.
    Address m_peer;               //!< The remote address of the receiver.
    Address m_kmaPeer;            //!< The remote address of the Key Manager Application (KMA).
    uint32_t m_ksid;              //!< The Key Session ID received from the KMA.
    std::string m_key;            //!< The quantum key received from the KMA.

    // Optional variables
    std::optional<uint16_t> m_peerPort;    //!< The remote port of the receiver.
    std::optional<uint16_t> m_kmaPeerPort; //!< The remote port of the KMA.

    // Trace sources
    ns3::TracedCallback<Ptr<const QkdSecureReceiver>>
        m_connectionEstablishedReceiverTrace; //!< Trace source for connection establishment with
                                              //!< the receiver.
    ns3::TracedCallback<Ptr<const QkdSecureReceiver>>
        m_connectionEstablishedKMATrace; //!< Trace source for connection establishment with the
                                         //!< KMA.
    ns3::TracedCallback<Ptr<const QkdSecureReceiver>>
        m_connectionClosedReceiverTrace; //!< Trace source for connection closure with the receiver.
    ns3::TracedCallback<Ptr<const QkdSecureReceiver>>
        m_connectionClosedKMATrace; //!< Trace source for connection closure with the KMA.
    ns3::TracedCallback<Ptr<Packet>, const Address&>
        m_rxKeyManagerAppTrace; //!< Trace source for receiving data from the KMA.
    ns3::TracedCallback<Ptr<const QkdSecureReceiver>, const Time&, const std::string&, uint32_t>
        m_rxKeyTrace; //!< Trace source for receiving a quantum key from the KMA.
    ns3::TracedCallback<Ptr<const QkdSecureReceiver>, const Time&, const std::string&, uint32_t>
        m_rxDataTrace; //!< Trace source for receiving encrypted data from the receiver.
    ns3::TracedCallback<const std::string&, const std::string&>
        m_stateTransitionTrace; //!< Trace source for state transitions of the QKD sender
                                //!< application.

    // Events
    /**
     * Event to request a secure session from the KMA.
     * This event is scheduled when the application starts and is used to initiate the
     * secure session establishment process with the KMA.
     */
    EventId m_requestOpenConnectEvent;

    /**
     * Event to request a quantum key from the KMA.
     * This event is scheduled after the secure session is established and is used to
     * request a quantum key from the KMA.
     */
    EventId m_requestGetKeyEvent;

    /**
     * Event to close the secure session with the KMA.
     * This event is scheduled when the application is stopped or when the secure session
     * needs to be closed.
     */
    EventId m_requestCloseEvent;

    /**
     * Event to send encrypted data to the receiver.
     * This event is scheduled when the quantum key is received and is used to send
     * encrypted data to the receiver.
     */
    EventId m_sendEncryptedDataEvent;
};
} // namespace ns3

#endif /* QKD_RECEIVER_H */
