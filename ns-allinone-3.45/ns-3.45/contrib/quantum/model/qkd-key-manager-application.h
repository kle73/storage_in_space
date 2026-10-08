/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_KEY_MANAGER_APPLICATION_H
#define QKD_KEY_MANAGER_APPLICATION_H
#include "qkd-app-header.h"
#include "qkd-app-trailer.h"
#include "qkd-key-manager.h"

#include "ns3/address.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"
#include "ns3/sink-application.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"

#include <map>
#include <vector>

namespace ns3
{

class QkdAppTxBuffer;
class QkdPeerKmaTxBuffer;

/**
 * @ingroup quantum
 * @brief QKD Key Manager Application class
 *
 * This is the model application which simulates the Key Manager Application (KMA) for Quantum Key
 * Distribution (QKD). It works in conjunction with QkdSecureSender and QkdSecureReceiver
 * applications.
 *
 * The KMA is responsible for managing the secure sessions and quantum keys.
 * It handles requests from QKD senders and receivers to open secure sessions, get quantum keys, and
 * close sessions.
 *
 * The KMA accepts connection requests from QKD senders and receivers and keeps them open
 * until the sender or receiver disconnects.
 *
 * The KMA also connects to Peer KMAs to manage secure sessions.
 */
class QkdKeyManagerApplication : public SinkApplication
{
  public:
    /**
     * Creates a new instance of QKD Key Manager Application.
     *
     * There is always only one Qkd Key Manager Application per node.
     */
    QkdKeyManagerApplication();

    /**
     * Destructor for the QKD Key Manager Application.
     */
    ~QkdKeyManagerApplication() override;

    /**
     * Returns the object TypeId.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Returns a pointer to the listener socket.
     * @return A pointer to the listener socket.
     */
    Ptr<Socket> GetSocket() const;

    /**
     * Sets the list of Peer KMAs to connect to.
     * @param peerKMAs A vector of addresses of Peer KMAs to connect to.
     * @param devices A vector of pointers to QkdDevice instances that are associated with the Peer
     * KMAs.
     */
    void SetDevicesAndPeerKMAs(const std::vector<Address>& peerKMAs,
                               std::vector<Ptr<QkdDevice>> devices);

    enum QkdKeyManagerAppState_t
    {
        NOT_STARTED = 0, ///< Before StartApplication() is invoked.
        STARTED,         ///< Passively listening and responding to requests.
        STOPPED          ///< After StopApplication() is invoked.
    };

    /**
     * Returns the current state of the QKD Key Manager Application.
     * @return The current state of the QKD Key Manager Application.
     */
    QkdKeyManagerAppState_t GetState() const;

    /**
     * Returns the current state of the QKD Key Manager Application as a string.
     * @return The current state of the QKD Key Manager Application as a string.
     */
    std::string GetStateString() const;

    /**
     * Returns the given state in string format.
     * @param state An arbitrary state of the QKD Key Manager Application.
     * @return The given state equivalently expressed in string format.
     */
    static std::string GetStateString(QkdKeyManagerAppState_t state);

    /**
     * Callback signature for the `ConnectionEstablished` trace source.
     * @param kma Pointer to this instance of QkdKeyManagerApplication, which is where
     *            the trace originated.
     * @param socket Pointer to the socket where the connection is established.
     */
    typedef void (*ConnectionEstablishedCallback)(Ptr<const QkdKeyManagerApplication> kma,
                                                  Ptr<Socket> socket);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * Sets the local address for the KMA.
     * @param addr The local address to bind to.
     */
    void SetLocal(const Address& addr) override;

    /**
     * Gets the list of QkdDevices in the node.
     * @return A vector of pointers to QkdDevices in the node.
     */
    std::vector<Ptr<QkdDevice>> GetQkdDevices() const;

    /**
     * Connects to the vector of Peer KMAs.
     * Should be called at start application time.
     */
    void ConnectToPeerKMAs();

    /**
     * Callback invoked when a connection request is received from a QKD sender or receiver.
     * @param socket Pointer to the socket where the event originates from.
     * @param from The address of the sender that initiated the connection request.
     * @returns True if the connection request is accepted, false otherwise.
     */
    bool ConnectionRequestCallback(Ptr<Socket> socket, const Address& from);

    /**
     * Callback invoked when a connection is established on the KMA socket.
     * This triggers the OpenConnection() to connect to the Peer KMA.
     * @param socket Pointer to the socket where the event originates from.
     * @param from The address of the sender that initiated the connection.
     */
    void ConnectionSucceededCallback(Ptr<Socket> socket, const Address& from);

    /**
     * Invoked when a connection is normally closed on the KMA socket.
     * @param socket Pointer to the socket where the event originates from.
     */
    void NormalCloseCallback(Ptr<Socket> socket);

    /**
     * Invoked when a connection is terminated abnormally on the KMA socket.
     * Error will be logged, but simulation continues.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ErrorCloseCallback(Ptr<Socket> socket);

    /**
     * Callback invoked when the KMA socket receives some packet data.
     * Fires the 'RxKeyManagerApp' trace source and triggers ReceiveKeySession() or ReceiveKey().
     * @param socket Pointer to the socket where the event originates from.
     */
    void ReceivedDataCallback(Ptr<Socket> socket);

    // With respect to peer KMA connections

    /**
     * Callback invoked when a connection is established on the KMA socket to a Peer KMA.
     * This triggers the RequestOpenConnect() to the Peer KMA.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ConnectionSucceededKMACallback(Ptr<Socket> socket);

    /**
     * Callback invoked when a connection fails on the KMA socket to a Peer KMA.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ConnectionFailedKMACallback(Ptr<Socket> socket);

    /**
     * Callback invoked when connection between the KMA socket and a Peer KMA is closed.
     * @param socket Pointer to the socket where the event originates from.
     */
    void NormalCloseKMACallback(Ptr<Socket> socket);

    /**
     * Callback invoked when connection between the KMA socket and a Peer KMA is terminated
     * abnormally. Error will be logged, but simulation continues.
     * @param socket Pointer to the socket where the event originates from.
     */
    void ErrorCloseKMACallback(Ptr<Socket> socket);

    /**
     * Callback invoked when the KMA socket receives some packet data from a Peer KMA.
     * Fires the 'RxKeyManagerApp' trace source and triggers ReceiveKeySession() or ReceiveKey().
     * @param socket Pointer to the socket where the event originates from.
     */
    void ReceivedDataKMACallback(Ptr<Socket> socket);

    // Transmission-related methods

    /**
     * This Generates/ Gets KSID for a new secure session and pushes
     * it into the AppTxBuffer.
     *
     * Fires the 'TxOpenConnectRequest' trace source.
     *
     * @param socket Pointer ti the socket associated with the request.
     * @param sourceAddress The address of the QKD sender or receiver that initiated the request.
     * @param destinationAddress The address of the Peer KMA to connect to.
     */
    void ServeOpenConnectRequest(Ptr<Socket> socket,
                                 const Address& sourceAddress,
                                 const Address& destinationAddress);

    /**
     * This Generates/ Gets KSID for a new secure session and pushes
     * it into the AppTxBuffer when KSID is already known.
     *
     * Fires the 'TxOpenConnectRequest' trace source.
     *
     * @param socket Pointer ti the socket associated with the request.
     * @param sourceAddress The address of the QKD sender or receiver that initiated the request.
     * @param destinationAddress The address of the Peer KMA to connect to.
     * @param ksid The known KSID for the secure session.
     */
    void ServeOpenConnectRequest(Ptr<Socket> socket,
                                 const Address& sourceAddress,
                                 const Address& destinationAddress,
                                 uint32_t ksid);

    /**
     * This Generates/ Gets a quantum key and pushes it into the AppTxBuffer.
     *
     * Fires the 'TxGetKeyRequest' trace source.
     *
     * @param socket Pointer to the socket associated with the request.
     * @param ksid The KSID for which the quantum key is requested.
     * @param sourceAddress The address of the QKD sender or receiver that initiated the request.
     * @param destinationAddress The address of the Peer KMA to connect to.
     */
    void ServeGetKeyRequest(Ptr<Socket> socket,
                            uint32_t ksid,
                            const Address& sourceAddress,
                            const Address& destinationAddress);

    /**
     * This Closes the secure session and pushes it into the AppTxBuffer.
     *
     * Fires the 'TxCloseConnectRequest' trace source.
     *
     * @param socket Pointer to the socket associated with the request.
     * @param ksid The KSID for which the secure session is to be closed.
     * @param sourceAddress The address of the QKD sender or receiver that initiated the request.
     * @param destinationAddress The address of the Peer KMA to connect to.
     */
    void ServeCloseConnectRequest(Ptr<Socket> socket,
                                  uint32_t ksid,
                                  const Address& sourceAddress,
                                  const Address& destinationAddress);

    /**
     * This function sends a packet to peer KMA with the ksid, source address,
     * destination address to add to the peer KMA's Key Manager.
     *
     * @param socket Pointer to the socket associated with the request.
     */
    void SendNewApp(Ptr<Socket> socket,
                    const Address& sourceAddress,
                    const Address& destinationAddress,
                    uint32_t ksid);

    /**
     * This function sends a packet to peer KMA with the ksid to close the secure session.
     * It includes the KSID in the packet to identify which session to close.
     *
     * @param socket Pointer to the socket associated with the request.
     * @param ksid The KSID for which the secure session is to be closed.
     * @param sourceAddress The address of the QKD sender or receiver that initiated the request.
     * @param destinationAddress The address of the Peer KMA to connect to.
     */
    void SendCloseSession(Ptr<Socket> socket,
                          uint32_t ksid,
                          const Address& sourceAddress,
                          const Address& destinationAddress);

    /**
     * Creates a packet with given header to send to the mentioned socket.
     * @param socket The socket to send the packet to.
     * @param header The header to include in the packet.
     */
    void SendPacket(Ptr<Socket> socket, const QkdAppHeader& header);

    /**
     * Creates a packet with given header and trailer to send to the mentioned socket.
     * @param socket The socket to send the packet to.
     * @param header The header to include in the packet.
     * @param trailer The trailer to include in the packet.
     */
    void SendPacket(Ptr<Socket> socket, const QkdAppHeader& header, const QkdAppTrailer& trailer);

    // Connection-related methods
    /**
     * This function initializes the KMA socket to connect to a Peer KMA
     * and sets up callbacks to listen to its events.
     * Invoked upon the start of the application.
     * This method is responsible for establishing the connection with the Peer KMA.
     * @param peerKMAAddress The address of the Peer KMA to connect to.
     */
    void OpenConnectionPeerKMA(const Address& peerKMAAddress);

    // Reception-related methods

    /**
     * On receiving an New App packet, this function processes the request
     * to open a secure session with the Peer KMA.
     * It extracts the KSID, source address, and destination address from the packet.
     *
     * @param packet The received packet containing the New App request.
     */
    void ReceiveNewApp(Ptr<Packet> packet);

    /**
     * On receiving a Close Session packet, this function processes the request
     * to close a secure session with the Peer KMA.
     * It extracts the KSID from the packet and closes the session.
     *
     * @param packet The received packet containing the  Close Session request.
     */
    void ReceiveCloseSession(Ptr<Packet> packet);

    /**
     * Change the state of the KMA. Fires the `StateTransition` trace source.
     * @param newState The new state to transition to.
     */
    void SwitchToState(QkdKeyManagerAppState_t newState);

    // Attributes

    Ptr<Socket> m_socket; //!< The socket used by the KMA to listen for connection requests.
    QkdKeyManagerAppState_t m_state;    //!< The current state of the KMA.
    Ptr<QkdKeyManager> m_qkdKeyManager; //!< Pointer to the QKD Key Manager instance.
    Ptr<QkdAppTxBuffer> m_appTxBuffer;  //!< The transmission buffer for the KMA application.
    Ptr<QkdPeerKmaTxBuffer>
        m_peerKmaTxBuffer; //!< The transmission buffer for the Peer KMA application.
    std::vector<Address> m_peerKmaAddresses; //!< Addresses of the Peer KMAs to connect to.

    // Add trace sources
}; // class QkdKeyManagerApplication

/**
 * @internal
 * @ingroup quantum
 * The Tranmsission Buffer used by the QKD Key Manager Application for managing socket of QKD
 * senders and receivers
 *
 * The class handles all the sockets which face the connected QKD senders and receivers.
 */
class QkdAppTxBuffer : public SimpleRefCount<QkdAppTxBuffer>
{
  public:
    /**
     * Creates a new instance of QkdAppTxBuffer.
     */
    QkdAppTxBuffer();

    /**
     * This method checks if the Socket is available in the transmission buffer.
     * @param socket The socket to check.
     * @return True if the socket is in the transmission buffer, false otherwise.
     */
    bool IsSocketInBuffer(Ptr<Socket> socket) const;

    /**
     * Adds a socket to the transmission buffer.
     * @param socket The socket to add.
     */
    void AddSocket(Ptr<Socket> socket);

    /**
     * Closes a given socket in the transmission buffer.
     * @param socket The socket to close.
     */
    void CloseSocket(Ptr<Socket> socket);

    /**
     * Closes all sockets in the transmission buffer.
     * This method closes all sockets and clears the transmission buffer.
     */
    void CloseAllSockets();

    /**
     * Removes a socket from the transmission buffer.
     * @param socket The socket to remove.
     */
    void RemoveSocket(Ptr<Socket> socket);

    // Buffer related methods

    /**
     * A method to check if the transmission buffer is empty.
     * @param socket The socket to get the transmission buffer for.
     * @return True if the transmission buffer is empty, false otherwise.
     */
    bool IsBufferEmpty(Ptr<Socket> socket) const;

    enum QkdSocketState_t
    {
        QKD_SOCKET_STARTED = 0,            //!< Socket is free and can accept new requests.
        QKD_SOCKET_OPEN_CONNECT_COMPLETE,  //!< Socket is busy and cannot accept new requests.
        QKD_SOCKET_GET_KEY_COMPLETE,       //!< Socket is busy and cannot accept new requests.
        QKD_SOCKET_CLOSE_CONNECT_COMPLETE, //!< Socket is busy and cannot accept new requests.
        QKD_SOCKET_STATE_CLOSED            //!< Socket is closed and no longer usable.
    };

    /**
     * Returns the state of the socket associated with the transmission buffer.
     * @param socket The socket to get the state for.
     * @return The state of the socket associated with the transmission buffer.
     */
    QkdSocketState_t GetSocketState(Ptr<Socket> socket) const;

    /**
     * Sets the state of the socket associated with the transmission buffer.
     * @param socket The socket to set the state for.
     * @param state The new state of the socket.
     */
    void SetSocketState(Ptr<Socket> socket, QkdSocketState_t state);

    /**
     * Maps to the next event in the transmission buffer.
     * @param socket The socket to get the next event for.
     * @param nextEvent The next event to map.
     */
    void MapNextEvent(Ptr<Socket> socket, EventId nextEvent);

    /**
     * Tell the buffer to close the associated socket once the buffer becomes empty.
     * @param socket Pointer to the socket which is associated with the
     * transmission buffer of interest.
     */
    void PrepareClose(Ptr<Socket> socket);

  private:
    /**
     * Set of fields represnting a single Transmission Buffer associated with a socket.
     */
    struct QkdTxBuffer_t
    {
        EventId nextServe;
        QkdSocketType_t socketType; //!< Type of the socket associated with the transmission buffer.
        QkdSocketState_t
            socketState; //!< State of the socket associated with the transmission buffer.
        Address address; //!< Address of the socket associated with the transmission buffer.
        bool isClosing;  //!< True if the remote end has issued a request to close.
    };

    /**
     * Collection of accepted sockets and its individual transmission buffer.
     * The key is the socket pointer, and the value is the transmission buffer.
     */
    std::map<Ptr<Socket>, QkdTxBuffer_t>
        m_txBuffer; //!< The transmission buffer for the KMA application.
}; // class QkdAppTxBuffer

/**
 * @internal
 * @ingroup quantum
 * The Tranmsission Buffer used by the QKD Key Manager Application for managing socket of Peer
 * KMAs.
 */
class QkdPeerKmaTxBuffer : public SimpleRefCount<QkdPeerKmaTxBuffer>
{
  public:
    /**
     * Creates a new instance of QkdAppTxBuffer.
     */
    QkdPeerKmaTxBuffer();

    /**
     * This method checks if the Socket is available in the transmission buffer.
     * @param socket The socket to check.
     * @return True if the socket is in the transmission buffer, false otherwise.
     */
    bool IsSocketInBuffer(Ptr<Socket> socket) const;

    /**
     * Adds a socket to the transmission buffer.
     * @param socket The socket to add.
     */
    void AddSocket(Ptr<Socket> socket);

    /**
     * Closes a given socket in the transmission buffer.
     * @param socket The socket to close.
     */
    void CloseSocket(Ptr<Socket> socket);

    /**
     * Closes all sockets in the transmission buffer.
     * This method closes all sockets and clears the transmission buffer.
     */
    void CloseAllSockets();

    /**
     * Removes a socket from the transmission buffer.
     * @param socket The socket to remove.
     */
    void RemoveSocket(Ptr<Socket> socket);

    // Buffer related methods

    /**
     * A method to check if the transmission buffer is empty.
     * @param socket The socket to get the transmission buffer for.
     * @return True if the transmission buffer is empty, false otherwise.
     */
    bool IsBufferEmpty(Ptr<Socket> socket) const;

    enum QkdSocketState_t
    {
        QKD_SOCKET_STARTED = 0,            //!< Socket is free and can accept new requests.
        QKD_SOCKET_OPEN_CONNECT_COMPLETE,  //!< Socket is busy and cannot accept new requests.
        QKD_SOCKET_GET_KEY_COMPLETE,       //!< Socket is busy and cannot accept new requests.
        QKD_SOCKET_CLOSE_CONNECT_COMPLETE, //!< Socket is busy and cannot accept new requests.
        QKD_SOCKET_STATE_CLOSED            //!< Socket is closed and no longer usable.
    };

    /**
     * Returns the state of the socket associated with the transmission buffer.
     * @param socket The socket to get the state for.
     * @return The state of the socket associated with the transmission buffer.
     */
    QkdSocketState_t GetSocketState(Ptr<Socket> socket) const;

    /**
     * Sets the state of the socket associated with the transmission buffer.
     * @param socket The socket to set the state for.
     * @param state The new state of the socket.
     */
    void SetSocketState(Ptr<Socket> socket, QkdSocketState_t state);

    /**
     * Maps to the next event in the transmission buffer.
     * @param socket The socket to get the next event for.
     * @param nextEvent The next event to map.
     */
    void MapNextEvent(Ptr<Socket> socket, EventId nextEvent);

    /**
     * Tell the buffer to close the associated socket once the buffer becomes empty.
     * @param socket Pointer to the socket which is associated with the
     * transmission buffer of interest.
     */
    void PrepareClose(Ptr<Socket> socket);

    /**
     * Sets the address of the socket associated with the transmission buffer.
     * @param socket The socket to set the address for.
     * @param address The address to set for the socket.
     */
    void SetSocketAddress(Ptr<Socket> socket, const Address& address);

    /**
     * Gets the socket for the given address.
     * @param address The address to get the socket for.
     * @return The socket associated with the given address, or nullptr if not found.
     */
    Ptr<Socket> GetSocketForAddress(const Address& address) const;

    /**
     * Gets the socket with same subnet as the given address.
     * @param address The address to check for the same subnet.
     * @return The socket associated with the same subnet as the given address, or nullptr if
     */
    Ptr<Socket> GetSocketInSameSubnet(const Address& address) const;

  private:
    /**
     * Set of fields represnting a single Transmission Buffer associated with a socket.
     */
    struct QkdTxBuffer_t
    {
        EventId nextServe;
        QkdSocketType_t socketType; //!< Type of the socket associated with the transmission buffer.
        QkdSocketState_t
            socketState; //!< State of the socket associated with the transmission buffer.
        Address address; //!< Address of the socket associated with the transmission buffer.
        bool isClosing;  //!< True if the remote end has issued a request to close.
    };

    /**
     * Collection of accepted sockets and its individual transmission buffer.
     * The key is the socket pointer, and the value is the transmission buffer.
     */
    std::map<Ptr<Socket>, QkdTxBuffer_t>
        m_txBuffer; //!< The transmission buffer for the KMA application.

    std::map<Address, Ptr<Socket>>
        m_peerKmaSockets; //!< The map of Peer KMA addresses to their respective sockets.
};

} // namespace ns3
#endif // QKD_KEY_MANAGER_APPLICATION_H
