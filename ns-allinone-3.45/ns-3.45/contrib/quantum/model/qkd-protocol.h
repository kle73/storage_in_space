/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Anirudh V Gubbi <anirudhvgubbi@gmail.com>
 *          Akash Ravi <akashravi28055@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_PROTOCOL_H
#define QKD_PROTOCOL_H

#include "ns3/callback.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/quantum-helper.h"
#include "ns3/random-variable-stream.h"

#include <string>
#include <vector>

namespace ns3
{

enum QkdProtocolStatus
{
    SUCCESS,
    HIGH_KEY_LOSS, // QBit error exceeds m_qbErrorThreshold
    HIGH_ERROR,    // Randomly sampled error exceeds m_errorThreshold, either noisy
                   // channel or presence of eavesdropper
    UNEXPECTED,
};

struct KeyGenerationData
{
    QkdProtocolStatus status;
    std::string key;
};

/**
 * @ingroup qkd
 *
 * @brief Represents a Quantum Key Distribution (QKD) protocol implementation.
 *
 * The `QkdProtocol` class provides an abstraction for implementing a QKD
 * protocol, which facilitates the secure generation and distribution of
 * cryptographic keys using quantum communication principles.
 */
class QkdProtocol : public Object
{
  public:
    static TypeId GetTypeId(void);
    /**
     * @brief Constructs a `QkdProtocol` object with specified callback functions.
     *
     * @param sendQBitCallback A callback function to handle sending of a quantum
     * bit (QBit).
     * @param sendBitCallback Callback function to handle the sending of data over
     * classical channel.
     */
    QkdProtocol(Callback<void, Ptr<QBit>> sendQBitCallback,
                Callback<void, Ptr<Packet>> sendClassicalCallback);

    /**
     * @brief Set the post key generation callback.
     * @param keyGenerationCallback callback that will be invoked upon completion
     * of the key generation process (for both successful and failure cases).
     */
    void SetKeyGenerationCallback(Callback<void, KeyGenerationData> keyGenerationCallback);

    /**
     * @brief Initiates the key generation process.
     *
     * @param size The size of the key to be generated, in bits.
     */
    virtual void InitiateKeyGeneration(std::size_t size) = 0;

    /**
     * @brief Handles the reception of a single quantum bit (QBit).
     *
     * @param qbit The received QBit.
     */
    virtual void RecvQBit(Ptr<QBit> qbit) = 0;

    /**
     * @brief Handles the reception of classical data.
     *
     * @param packet The received classical packet.
     */
    virtual void RecvClassical(Ptr<Packet> packet) = 0;

  protected:
    double m_qbErrorThreshold;
    double m_errorThreshold;
    uint32_t m_errorEstimationBlockLength;
    uint32_t m_finalKeySize;

    /// Helper for quantum operations
    struct QkdErrorRequestData
    {
        uint32_t startIndex;
        uint32_t blockLength;
    };

    QkdErrorRequestData currentRequestData;

    QuantumHelper m_helper;

    /// Stores the current bit string used in the protocol for the sender, and the
    /// measured bit string for the receiver.
    std::string m_bitString;

    /// Stores the index of the next bit of bitString to be sent.
    size_t m_nextBitIndex;

    /// Stores the final key after sifting.
    std::string m_key;

    /// Stores the QBit error rate.
    double m_qbError;

    /// Callback function to notify key generation events.
    Callback<void, KeyGenerationData> m_notifyKeyGenerationCallback;

    /// Callback function to handle the sending of a single quantum bit (QBit).
    Callback<void, Ptr<QBit>> m_sendQBitCallback;

    /// Callback function to handle the sending of data over classical channel.
    Callback<void, Ptr<Packet>> m_sendClassicalCallback;

    /// To generate random bits.
    Ptr<UniformRandomVariable> m_randomVariableStream;
};

/**
 * @ingroup qkd
 *
 * @brief Implements the B92 Quantum Key Distribution (QKD) protocol.
 *
 * The `B92QkdProtocol` class provides implementation of B92 QKD protocol,
 * which facilitates the secure generation and distribution of cryptographic
 * keys using quantum communication principles and 2 non-orthogonal bases.
 */
class B92QkdProtocol : public QkdProtocol
{
  public:
    /**
     * @brief Constructs a `B92QkdProtocol` object with specified callback
     * functions.
     *
     * @param sendQBitCallback A callback function to handle sending of a quantum
     * bit (QBit).
     * @param sendBitCallback Callback function to handle the sending of data over
     * classical channel.
     */
    B92QkdProtocol(Callback<void, Ptr<QBit>> sendQBitCallback,
                   Callback<void, Ptr<Packet>> sendClassicalCallback);

    void InitiateKeyGeneration(std::size_t size) override;
    void RecvQBit(Ptr<QBit> qbit) override;
    void RecvClassical(Ptr<Packet> bits) override;

  private:
    /**
     * @brief Generates and sends the next QBit from bitString according to rules
     * of B92.
     */
    void SendQBitHelper();
};
} // namespace ns3

#endif /* QKD_PROTOCOL_H */
