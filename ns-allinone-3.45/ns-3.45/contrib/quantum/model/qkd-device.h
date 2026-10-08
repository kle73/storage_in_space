/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Aniket Singh <aniketsingh84646@gmail.com>
 *          Satyam Shukla <shuklasatyam774@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_DEVICE_H
#define QKD_DEVICE_H

#include "qkd-protocol.h"
#include "quantum-device.h"

#include "ns3/callback.h"
#include "ns3/error-model.h"
#include "ns3/net-device.h"
#include "ns3/quantum-helper.h"
#include "ns3/queue.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simple-channel.h"

#include <cstdint>
#include <map>

namespace ns3
{

/**
 * @brief Structure to store information relevant to a key exchange session.
 */
struct KeyExchangeInfo : public Object
{
    Mac48Address destinationDeviceAddress; //!< Device address of destination
    Ptr<QkdProtocol> protocol;             //!< Protocol instance
};

typedef std::map<Mac48Address, Ptr<struct KeyExchangeInfo>> KeyExchangeInfoMap;

/**
 * @ingroup quantum
 * @class QkdDevice
 * @brief This class represents a QKD (Quantum Key Distribution) device in ns-3.
 *
 * A QkdDevice handles sending and receiving data through quantum and classical
 * channels, and manages key exchange sessions using various QKD protocols.
 */
class QkdDevice : public QuantumDevice
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId(void);

    /**
     * @brief QkdDevice constructor.
     */
    QkdDevice();

    /**
     * @brief QkdDevice destructor.
     */
    ~QkdDevice();

    /**
     * @brief Get if device is eavesdropper.
     * @return the eavesdropper status.
     */
    bool GetEave() const;

    /**
     * @brief Set device as eavesdropper.
     * @param eave true if device is eavesdropper, false otherwise
     */
    void SetEave(bool eave);

    /**
     * @brief Set the post key generation callback.
     * @param keyGenerationCallback callback that will be invoked upon completion
     * of the key generation process (for both successful and failure cases).
     */
    void SetKeyGenerationCallback(Callback<void, KeyGenerationData> keyGenerationCallback);

    /**
     * @brief Initiate the key generation process.
     * @param size size of the key to be generated
     * @param recvDevice pointer to the receiving device.
     */
    void InitiateKeyGeneration(std::size_t size, Ptr<QkdDevice> recvDevice);

    /**
     * @brief Initialize the QkdDevice.
     * This function sets up the device to handle incoming classical packets
     * and prepares it for key generation.
     */
    void DoInitialize() override;

    /**
     * @brief Send a qubit over the quantum channel.
     * @param senderDevice pointer to sender device
     * @param qbit pointer to QBit to be sent
     */
    static void SendThroughQuantumChannel(Ptr<QkdDevice> senderDevice, Ptr<QBit> qbit);

    /**
     * @brief Send a packet over the classical channel.
     * @param keyExchangeInfo pointer to key exchange metadata
     * @param packet pointer to packet to be sent
     */
    static void SendThroughClassicalChannel(Ptr<QkdDevice> senderDevice,
                                            Ptr<KeyExchangeInfo> keyExchangeInfo,
                                            Ptr<Packet> packet);

    /**
     * @brief Handle receiving a qubit from the quantum channel.
     * @param qbit the received QBit
     * @param sourceDeviceAddress the address of the sender
     */
    void ReceiveQubit(Ptr<QBit> qbit, Mac48Address sourceDeviceAddress);

    /**
     * @brief Handle receiving a classical packet.
     * @param keyExchangeInfo pointer to key exchange metadata
     * @param packet the received packet
     */
    bool ReceiveFromClassicalChannel(Ptr<NetDevice> device,
                                     Ptr<const Packet> packet,
                                     uint16_t protocol,
                                     const Address& from);

    /**
     * @brief Send a qubit after a delay.
     * @param qbit the qubit to send
     * @param sourceDeviceAddress the senders address
     */
    void SendQubit(Ptr<QBit> qbit, Mac48Address sourceDeviceAddress);

    /**
     * @brief Set up key exchange information for a specific device address.
     * @param destinationDeviceAddr the device address for which to set up key
     * exchange info
     */
    void SetupKeyExchangeInfo(Mac48Address destinationDeviceAddress);

  private:
    KeyExchangeInfoMap m_keyExchangeInfoMap; //!< Mapping of device address to key exchange info
    bool m_eave;                             //!< Eavesdropper status
    Ptr<UniformRandomVariable> m_randomVariableStream; //!< Random variable stream for generating
                                                       //!< random bits
    Callback<void, KeyGenerationData>
        m_notifyKeyGenerationCallback; //!< Callback function to notify key
                                       //!< generation events.
};

} // namespace ns3

#endif /* QKD_DEVICE_H */
