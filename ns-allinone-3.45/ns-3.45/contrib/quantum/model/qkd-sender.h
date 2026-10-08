/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_SENDER_H
#define QKD_SENDER_H
#include "qkd-app-header.h"
#include "qkd-app-trailer.h"

#include "ns3/address.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/source-application.h"
#include "ns3/traced-callback.h"

#include <optional>

namespace ns3
{

/**
 * @ingroup quantum
 *
 * This application simulates a Quantum Key Distribution (QKD) sender.
 *
 * The QKD sender application is responsible for establishing a connection with a QKD receiver
 * and to the Qkd Key Manager Application (KMA) to request a secure session and quantum keys.
 *
 * It handles communication with the QKD receiver and the KMA, manages the state of the QKD session,
 * and provides trace sources for various events such as connection establishment, key reception,
 * etc.
 */
class QkdSecureSender : public SourceApplication
{
  public:
    /**
     * Creates a new instance of QKD sender application.
     */
    QkdSecureSender();

    /**
     * Destructor for the QKD sender application.
     */
    ~QkdSecureSender() override;

    /**
     * Returns the object TypeId.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Sets the remote address of the QKD sender and the receiver.
     * @param addr The remote address of the receiver to which the QKD sender will send packets.
     */
    void SetRemote(const Address& addr) override;

    /**
     * Sets the remote address of the Key Manager Application (KMA).
     * @param addr The remote address of the KMA to which the QKD sender will send packets.
     */
    void SetRemoteKMA(const Address& addr);

    /**
     * Returns the socket associated with the QKD sender and the receiver.
     * @return Pointer to the associated receiver socket.
     */
    Ptr<Socket> GetSocket() const;

    /**
     * Returns the socket associated with the QKD sender and the Key Manager Application (KMA).
     * @return Pointer to the associated KMA socket.
     */
    Ptr<Socket> GetSocketKMA() const;

    /// Possible states of the QKD sender application.
    enum QkdAppState_t
    {
        /// Before StartApplication() is invoked.
        NOT_STARTED = 0,
        /// Sent the receiver a connection request and waiting for the receiver to accept it.
        CONNECTING_RECEIVER,
        /// Sent the Key Manager Application a connection request and waiting for the KMA to accept
        /// it.
        CONNECTING_KMA,
        /// To send the KMA a request to start a secure session.
        REQUEST_OPEN_CONNECT,
        // Sent the KMA a request to start a secure session and waiting for the KMA to respond.
        AWAIT_KSID,
        /// Send the received Key Session ID (KSID) to the receiver.
        SEND_KSID_TO_RECEIVER,
        /// Sent the receiver a request to open a secure session and waiting for the receiver to
        /// respond.
        AWAIT_KSID_ACK,
        /// To send the KMA a request to send a quantum key.
        REQUEST_GET_KEY,
        /// Sent the KMA a request to send a quantum key and waiting for the KMA to respond.
        AWAIT_KEY,
        /// Sent the KMA a request to close the secure session.
        CLOSE_CONNECT,
        /// Sent the Receiver some data and waiting for the receiver to acknowledge it and respond.
        EXPECTING_ACK,
        /// Encrypting data with the received quantum key and sending it to the receiver.
        ENCRYPTING_DATA,
        /// After StopApplication() is invoked.
        STOPPED
    };

    /**
     * Returns the current state of the QKD sender application.
     * @return The current state of the QKD sender application.
     */
    QkdAppState_t GetState() const;

    /**
     * Returns the current state of the QKD sender application in string format.
     * @return The current state of the QKD sender application in string format.
     */
    std::string GetStateString() const;

    /**
     * Returns the given state in string format.
     * @param state An arbitrary state of a QKD sender application.
     * @return The given state equivalently expressed in string format.
     */
    static std::string GetStateString(QkdAppState_t state);

    /**
     * Common callback signature for 'ConnectionEstablished'and 'RxKeyManagerApp', and
     * 'RxEmbeddedObject' trace sources.
     * @param qkdSender Pointer to this instance of QkdSender,
     *                               which is where the trace originated.
     */
    typedef void (*TracedCallback)(Ptr<const QkdSecureSender> qkdSender);

    /**
     * Callback signature for 'RxKey' trace sources.
     * @param qkdSender Pointer to this instance of QkdSender,
     *                               which is where the trace originated.
     * @param time Elapse time from the start to the end of the request.
     * @param key The quantum key received from the receiver.
     * @param keySize The size of the quantum key in bits.
     */
    typedef void (*RxKMATracedCallback)(Ptr<const QkdSecureSender> qkdSender,
                                        const Time& time,
                                        const std::string& key,
                                        uint32_t keySize);

    /**
     * Callback signature for 'RxData' trace sources.
     * @param qkdSender Pointer to this instance of QkdSender,
     *                              which is where the trace originated.
     * @param time Elapse time from the start to the end of the request.
     * @param data The data received from the receiver.
     * @param dataSize The size of the data in bytes.
     */
    typedef void (*RxDataTracedCallback)(Ptr<const QkdSecureSender> qkdSender,
                                         const Time& time,
                                         const std::string& data,
                                         uint32_t dataSize);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * Sets the Key Session ID for the QKD sender application.
     * @param keySessionId The Key Session ID to be set.
     */
    void SetKsid(uint32_t keySessionId);

    /**
     * Returns the Key Session ID for the QKD sender application.
     * @return The Key Session ID.
     */
    uint32_t GetKsid() const;

    /**
     * Sets the quantum key for the QKD sender application.
     * @param key The quantum key to be set.
     */
    void SetKey(const std::string& key);

    /**
     * Returns the quantum key for the QKD sender application.
     * @return The quantum key.
     */
    std::string GetKey() const;

    // Socket Callbacks for Receiver and KMA

    /**
     * Invoked when a connection is established on m_socket.
     * This triggers the Open Connect request to the receiver.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ConnectionSucceededReceiverCallback(Ptr<Socket> socket);

    /**
     * Invoked when a connection fails on m_socket to the receiver.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ConnectionFailedReceiverCallback(Ptr<Socket> socket);
    /**
     * Invoked when connection between m_socket and the receiver is closed.
     * @param socket Pointer to the socket where the event originates from.
     */
    void NormalCloseReceiverCallback(Ptr<Socket> socket);

    /**
     * Invoked when connection between m_socket and the receiver is terminated abnormally.
     * Error will be logged, but simulation continues.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ErrorCloseReceiverCallback(Ptr<Socket> socket);

    /**
     * Involved when m_socket receives some packet data from the receiver.
     * Fires the 'RxData' trace source and triggers ReceiveEncryptedData().
     * @param socket Pointer to the socket where the event originates from.
     */
    void ReceivedDataReceiverCallback(Ptr<Socket> socket);

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
     * Initialize m_socket to connect to the destination receiver at m_peer and
     * set up callbacks to listen to its event. Invoked upon the start of the application.
     * This method is responsible for establishing the connection and is called
     * when the application starts.
     */
    void OpenConnectionReceiver();

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
     * Sends encrypted data to the receiver.
     * Fires the 'TxEncryptedData' trace source.
     * @param data The data to be sent, encrypted with the quantum key.
     */
    void SendEncryptedData(const std::string& data);

    /**
     * Sends the Key Session ID to the receiver.
     * Fires the 'TxKeySessionId' trace source.
     */
    void SendKeySessionId();

    // Reception-related methods
    /**
     * Receives encrypted data from the receiver.
     * Fires the 'RxEncryptedData' trace source.
     * @param packet The packet containing the encrypted data.
     * @param header The header containing the Key Session ID.
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
     * @param header The header containing the key session ID.
     */
    void ReceiveKey(Ptr<Packet> packet, QkdAppHeader& header);

    // Other methods
    /**
     * Cancel all pending events related to the QKD sender application.
     * This method is invoked when the application is stopped or when a connection
     * is terminated.
     */
    void CancelAllPendingEvents();

    /**
     * Change the state of the QKD sender application.
     * Fires the 'StateTransition' trace source.
     * @param state The new state to transition to.
     */
    void SwitchToState(QkdAppState_t state);

    /// Variables
    QkdAppState_t m_state;   //!< The current state of the QKD sender application.
    Ptr<Socket> m_socket;    //!< The socket for sending and receiving packets to/from the receiver.
    Ptr<Socket> m_socketKMA; //!< The socket for sending and receiving packets to/from the KMA.
    Address m_peer;          //!< The remote address of the receiver.
    Address m_kmaPeer;       //!< The remote address of the Key Manager Application (KMA).
    uint32_t m_ksid;         //!< The Key Session ID received from the KMA.
    std::string m_key;       //!< The quantum key received from the KMA.

    // Optional variables
    std::optional<uint16_t> m_peerPort;    //!< The remote port of the receiver.
    std::optional<uint16_t> m_kmaPeerPort; //!< The remote port of the KMA.

    // Trace sources
    ns3::TracedCallback<Ptr<const QkdSecureSender>>
        m_connectionEstablishedReceiverTrace; //!< Trace source for connection establishment with
                                              //!< the receiver.
    ns3::TracedCallback<Ptr<const QkdSecureSender>>
        m_connectionEstablishedKMATrace; //!< Trace source for connection establishment with the
                                         //!< KMA.
    ns3::TracedCallback<Ptr<const QkdSecureSender>>
        m_connectionClosedReceiverTrace; //!< Trace source for connection closure with the receiver.
    ns3::TracedCallback<Ptr<const QkdSecureSender>>
        m_connectionClosedKMATrace; //!< Trace source for connection closure with the KMA.
    ns3::TracedCallback<Ptr<Packet>, const Address&>
        m_rxKeyManagerAppTrace; //!< Trace source for receiving data from the KMA.
    ns3::TracedCallback<Ptr<const QkdSecureSender>, const Time&, const std::string&, uint32_t>
        m_rxKeyTrace; //!< Trace source for receiving a quantum key from the KMA.
    ns3::TracedCallback<Ptr<const QkdSecureSender>, const Time&, const std::string&, uint32_t>
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
#endif /* QKD_SENDER_H */
