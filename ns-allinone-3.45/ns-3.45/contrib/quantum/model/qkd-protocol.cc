/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Anirudh V Gubbi <anirudhvgubbi@gmail.com>
 *          Akash Ravi <akashravi28055@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-protocol.h"

#include "qkd-header.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/uinteger.h"

#include <string>

const double DEFAULT_QB_ERROR_THRESHOLD = 0.81;
const double DEFAULT_ERROR_THRESHOLD = 0.1;
const uint32_t DEFAULT_FINAL_KEY_SIZE = 64;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QkdProtocol");

NS_OBJECT_ENSURE_REGISTERED(QkdProtocol);

TypeId
QkdProtocol::GetTypeId(void)
{
    NS_LOG_FUNCTION_NOARGS();
    static TypeId tid =
        TypeId("ns3::QkdProtocol")
            .SetParent<Object>()
            .SetGroupName("QKD")
            .AddAttribute("ErrorEstimationBlockLength",
                          "Block of length to estimate QBERR",
                          UintegerValue(16),
                          MakeUintegerAccessor(&QkdProtocol::m_errorEstimationBlockLength),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("QBErrorThreshold",
                          "Maximum acceptable loss of bits while sifting",
                          DoubleValue(DEFAULT_QB_ERROR_THRESHOLD), // Use constant
                          MakeDoubleAccessor(&QkdProtocol::m_qbErrorThreshold),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("ErrorThreshold",
                          "Maximum acceptable error in a random sample of sifted bits",
                          DoubleValue(DEFAULT_ERROR_THRESHOLD), // Use constant
                          MakeDoubleAccessor(&QkdProtocol::m_errorThreshold),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("FinalKeySize",
                          "The size of the final key after privacy amplification",
                          UintegerValue(DEFAULT_FINAL_KEY_SIZE), // Use constant
                          MakeUintegerAccessor(&QkdProtocol::m_finalKeySize),
                          MakeUintegerChecker<uint32_t>(1, 1024));
    return tid;
}

QkdProtocol::QkdProtocol(Callback<void, Ptr<QBit>> sendQBitCallback,
                         Callback<void, Ptr<Packet>> sendClassicalCallback)
    : m_sendQBitCallback(sendQBitCallback),
      m_sendClassicalCallback(sendClassicalCallback)
{
    NS_LOG_FUNCTION(this); // Add logging macro here
    NS_ASSERT_MSG(!m_sendQBitCallback.IsNull(),
                  "The provided m_sendQBitCallback is a null callback");
    NS_ASSERT_MSG(!m_sendClassicalCallback.IsNull(),
                  "The provided sendBitCallback is a null callback");

    m_randomVariableStream = CreateObject<UniformRandomVariable>();
}

void
QkdProtocol::SetKeyGenerationCallback(Callback<void, KeyGenerationData> keyGenerationCallback)
{
    NS_LOG_FUNCTION(this);

    this->m_notifyKeyGenerationCallback = keyGenerationCallback;
}

NS_OBJECT_ENSURE_REGISTERED(B92QkdProtocol);

B92QkdProtocol::B92QkdProtocol(Callback<void, Ptr<QBit>> sendQBitCallback,
                               Callback<void, Ptr<Packet>> sendClassicalCallback)
    : QkdProtocol(sendQBitCallback, sendClassicalCallback)
{
}

void
B92QkdProtocol::InitiateKeyGeneration(std::size_t size)
{
    NS_LOG_FUNCTION(this << size);

    if (m_notifyKeyGenerationCallback.IsNull())
    {
        NS_LOG_WARN("The provided key generation callback is a null callback or "
                    "has not been set. "
                    "Key generation events will not be notified.");
    }

    m_bitString.clear();
    m_bitString.reserve(size);
    m_key.clear();
    m_nextBitIndex = 0;

    for (size_t i = 0; i < size; i++)
    {
        // Generate a random bit (0 or 1)
        uint8_t bit = static_cast<uint8_t>(m_randomVariableStream->GetInteger(0, 1));

        // Append the bit as a character ('0' or '1') to the bit string
        m_bitString.push_back(static_cast<char>('0' + bit));
    }
    NS_LOG_DEBUG("m_bitString : " << m_bitString);

    SendQBitHelper();
}

void
B92QkdProtocol::RecvQBit(Ptr<QBit> qbit)
{
    NS_LOG_FUNCTION(this);

    int randomBasis = m_randomVariableStream->GetInteger(0, 1);
    Bit measurement = m_helper.MeasureQBit(*qbit, randomBasis);
    NS_LOG_DEBUG("Measured as : " << measurement);
    uint8_t sendBit;

    if (measurement == 0)
    {
        m_bitString.push_back('?');
        sendBit = 0;
    }
    else
    {
        sendBit = 1;
        if (randomBasis == 1)
        {
            m_bitString.push_back('0');
            m_key.push_back('0');
        }
        else
        {
            m_bitString.push_back('1');
            m_key.push_back('1');
        }
    }
    NS_LOG_DEBUG("Key now is : " << m_key);

    Ptr<Packet> sendPacket = Create<Packet>(&sendBit, sizeof(sendBit));
    QkdHeader sendHeader;
    sendHeader.SetPhase(KEY_SIFTING);
    sendPacket->AddHeader(sendHeader);
    m_sendClassicalCallback(sendPacket);
}

void
B92QkdProtocol::RecvClassical(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    QkdHeader header;
    packet->RemoveHeader(header);
    QkdPhase phase = header.GetPhase();

    NS_LOG_INFO("Packet received with header : " << QkdPhaseToString(phase));

    switch (phase)
    {
    case KEY_SIFTING: {
        uint8_t receivedBit;
        packet->CopyData(&receivedBit, sizeof(receivedBit));

        if (receivedBit == 1)
        {
            m_key.push_back(m_bitString[m_nextBitIndex]);
        }

        if (m_nextBitIndex >= m_bitString.length() - 1)
        {
            NS_LOG_DEBUG("Sender's key after sifting: " << m_key);
            NS_LOG_DEBUG("Key size after sifting: " << m_key.size());

            m_qbError = static_cast<double>(m_bitString.size() - m_key.size()) / m_bitString.size();
            NS_LOG_INFO("QBERR at Sender is " << m_qbError << " vs " << m_qbErrorThreshold);

            Ptr<Packet> sendPacket = Create<Packet>();
            QkdHeader sendHeader;
            sendHeader.SetPhase(KEY_SIFTING_STOP);
            sendPacket->AddHeader(sendHeader);
            m_sendClassicalCallback(sendPacket);
        }
        else
        {
            m_nextBitIndex++;
            SendQBitHelper();
        }

        break;
    }
    case KEY_SIFTING_STOP: {
        NS_LOG_DEBUG("Receiver's key after sifting: " << m_key);
        NS_LOG_DEBUG("Key size after sifting: " << m_key.size());

        m_qbError = static_cast<double>(m_bitString.size() - m_key.size()) / m_bitString.size();
        NS_LOG_INFO("QBERR at Receiver is " << m_qbError << " vs " << m_qbErrorThreshold);

        Ptr<Packet> sendPacket;
        QkdHeader sendHeader;
        if (m_qbError > m_qbErrorThreshold)
        {
            QkdProtocolStatus status = QkdProtocolStatus::HIGH_KEY_LOSS;
            KeyGenerationData data{status};
            if (!m_notifyKeyGenerationCallback.IsNull())
            {
                m_notifyKeyGenerationCallback(data);
            }

            sendPacket = Create<Packet>(reinterpret_cast<uint8_t*>(&status), sizeof(status));
            sendHeader.SetPhase(KEY_GEN_FAILURE);
        }
        else
        {
            currentRequestData.startIndex =
                m_randomVariableStream->GetInteger(0, m_key.size() - m_errorEstimationBlockLength);
            currentRequestData.blockLength = m_errorEstimationBlockLength;

            NS_LOG_INFO("Requesting block " << currentRequestData.startIndex << ","
                                            << currentRequestData.blockLength);

            sendPacket = Create<Packet>(reinterpret_cast<uint8_t*>(&currentRequestData),
                                        sizeof(currentRequestData));
            sendHeader.SetPhase(ERROR_ESTIMATION_REQ);
        }

        sendPacket->AddHeader(sendHeader);
        m_sendClassicalCallback(sendPacket);

        break;
    }
    case ERROR_ESTIMATION_REQ: {
        packet->CopyData(reinterpret_cast<uint8_t*>(&currentRequestData),
                         sizeof(currentRequestData));
        std::string requestedBlock =
            m_key.substr(currentRequestData.startIndex, currentRequestData.blockLength);
        m_key.erase(currentRequestData.startIndex, currentRequestData.blockLength);
        NS_LOG_DEBUG("Sender's key after err estimation: " << m_key);
        NS_LOG_DEBUG("Sending block: " << requestedBlock);

        Ptr<Packet> sendPacket =
            Create<Packet>(reinterpret_cast<const uint8_t*>(requestedBlock.c_str()),
                           requestedBlock.size());
        QkdHeader sendHeader;
        sendHeader.SetPhase(ERROR_ESTIMATION_RES);
        sendPacket->AddHeader(sendHeader);
        m_sendClassicalCallback(sendPacket);

        break;
    }
    case ERROR_ESTIMATION_RES: {
        uint32_t packetSize = packet->GetSize();
        std::vector<uint8_t> buffer(packetSize);
        packet->CopyData(buffer.data(), packetSize);

        std::string receivedBlock(buffer.begin(), buffer.end());
        NS_LOG_DEBUG("Received block: " << receivedBlock);

        NS_LOG_INFO("Estimating error in block " << currentRequestData.startIndex << ","
                                                 << currentRequestData.blockLength);
        uint32_t errorCount = 0;
        std::string block =
            m_key.substr(currentRequestData.startIndex, currentRequestData.blockLength);
        for (uint32_t i = 0; i < block.size(); i++)
        {
            if (block[i] != receivedBlock[i])
            {
                errorCount++;
            }
        }

        m_key.erase(currentRequestData.startIndex, currentRequestData.blockLength);
        NS_LOG_DEBUG("Receiver's key after err estimation: " << m_key);

        double error = static_cast<double>(errorCount) / currentRequestData.blockLength;

        NS_LOG_INFO("Estimated error : " << error);

        QkdPhase sendPhase = error > m_errorThreshold ? KEY_GEN_FAILURE : ERROR_ESTIMATION_SUCC;
        Ptr<Packet> sendPacket;

        if (sendPhase == KEY_GEN_FAILURE)
        {
            QkdProtocolStatus status = QkdProtocolStatus::HIGH_ERROR;
            KeyGenerationData data{status};
            if (!m_notifyKeyGenerationCallback.IsNull())
            {
                m_notifyKeyGenerationCallback(data);
            }

            sendPacket = Create<Packet>(reinterpret_cast<uint8_t*>(&status), sizeof(status));
        }
        else
        {
            sendPacket =
                Create<Packet>(reinterpret_cast<uint8_t*>(&errorCount), sizeof(errorCount));
        }

        QkdHeader sendHeader;
        sendHeader.SetPhase(sendPhase);
        sendPacket->AddHeader(sendHeader);
        m_sendClassicalCallback(sendPacket);

        break;
    }
    case ERROR_ESTIMATION_SUCC: {
        /// TODO: Move to ERROR_CORRECTION phase if available else move to
        /// PRIVACY_AMPLIFICATION

        Ptr<Packet> sendPacket = Create<Packet>();
        QkdHeader sendHeader;
        sendHeader.SetPhase(PRIVACY_AMPLIFICATION_REQ);
        sendPacket->AddHeader(sendHeader);
        m_sendClassicalCallback(sendPacket);
        break;
    }
    case ERROR_CORRECTION: {
        /// TODO: Implement error correction after figuring out noise models
        break;
    }
    case PRIVACY_AMPLIFICATION_REQ: {
        // Ensure error correction has been completed
        if (m_key.empty())
        {
            NS_LOG_ERROR("Key is empty. Privacy amplification cannot proceed.");
            return;
        }

        // Use the configured final key size
        size_t finalKeySize = std::min(static_cast<size_t>(m_finalKeySize), m_key.size());

        // Generate a random Toeplitz vector of size (m_key.size() + finalKeySize
        // - 1)
        size_t toeplitzSize = m_key.size() + finalKeySize - 1;
        std::vector<uint8_t> toeplitzVector(toeplitzSize);
        for (size_t i = 0; i < toeplitzSize; i++)
        {
            toeplitzVector[i] = static_cast<uint8_t>(m_randomVariableStream->GetInteger(0, 1));
        }

        // Log the generated Toeplitz vector
        std::ostringstream toeplitzStream;
        for (size_t i = 0; i < toeplitzVector.size(); i++)
        {
            toeplitzStream << static_cast<int>(toeplitzVector[i]);
        }
        NS_LOG_DEBUG("Generated Toeplitz vector: " << toeplitzStream.str());

        // Log the key before privacy amplification
        NS_LOG_DEBUG("Key before privacy amplification: " << m_key);

        // Apply the Toeplitz hash function
        std::string finalKey(finalKeySize, '0');
        for (size_t i = 0; i < finalKeySize; i++)
        {
            uint8_t hashValue = 0;
            for (size_t j = 0; j < m_key.size(); j++)
            {
                // Perform XOR between the key bit and the corresponding Toeplitz bit
                hashValue ^= (m_key[j] - '0') & toeplitzVector[i + j];
            }
            // Convert the hash value back to a valid character ('0' or '1')
            finalKey[i] = (hashValue % 2) + '0';
        }

        // Update the key with the final compressed key
        m_key = finalKey;

        // Log the final key after privacy amplification
        NS_LOG_DEBUG("Final key after privacy amplification: " << m_key);
        NS_LOG_DEBUG("Final key size at receiver: " << m_key.size());

        // Send the Toeplitz vector to the receiver
        Ptr<Packet> sendPacket = Create<Packet>(toeplitzVector.data(), toeplitzVector.size());
        QkdHeader sendHeader;
        sendHeader.SetPhase(PRIVACY_AMPLIFICATION_RES);
        sendPacket->AddHeader(sendHeader);
        m_sendClassicalCallback(sendPacket);

        break;
    }
    case PRIVACY_AMPLIFICATION_RES: {
        // Receive the Toeplitz vector (seed) from the sender
        std::vector<uint8_t> toeplitzVector(packet->GetSize());
        packet->CopyData(toeplitzVector.data(), toeplitzVector.size());

        // Log the received Toeplitz vector
        NS_LOG_DEBUG("Received Toeplitz vector for privacy amplification.");

        // Log the generated Toeplitz vector
        std::ostringstream toeplitzStream;
        for (size_t i = 0; i < toeplitzVector.size(); i++)
        {
            toeplitzStream << static_cast<int>(toeplitzVector[i]);
        }
        NS_LOG_DEBUG("Received Toeplitz vector: " << toeplitzStream.str());

        // Log the key before privacy amplification
        NS_LOG_DEBUG("Key before privacy amplification: " << m_key);

        // Define the final key size based on the received Toeplitz vector
        size_t finalKeySize = m_finalKeySize;

        // Apply the Toeplitz hash function
        std::string finalKey(finalKeySize, '0');
        for (size_t i = 0; i < finalKeySize; i++)
        {
            uint8_t hashValue = 0;
            for (size_t j = 0; j < m_key.size(); j++)
            {
                // Perform XOR between the key bit and the corresponding Toeplitz bit
                hashValue ^= (m_key[j] - '0') & toeplitzVector[i + j];
            }
            // Convert the hash value back to a valid character ('0' or '1')
            finalKey[i] = (hashValue % 2) + '0';
        }

        // Update the key with the final compressed key
        m_key = finalKey;

        // Log the final key after privacy amplification
        NS_LOG_DEBUG("Final key after privacy amplification: " << m_key);
        NS_LOG_DEBUG("Final key size at sender: " << m_key.size());

        Ptr<Packet> sendPacket = Create<Packet>();
        QkdHeader sendHeader;
        sendHeader.SetPhase(KEY_GEN_SUCCESS);
        sendPacket->AddHeader(sendHeader);
        m_sendClassicalCallback(sendPacket);

        // Notify the completion of key generation (as a sender)
        KeyGenerationData data{QkdProtocolStatus::SUCCESS, m_key};
        if (!m_notifyKeyGenerationCallback.IsNull())
        {
            m_notifyKeyGenerationCallback(data);
        }

        break;
    }
    case KEY_GEN_SUCCESS: {
        // Notify the completion of key generation (as a receiver)
        KeyGenerationData data{QkdProtocolStatus::SUCCESS, m_key};
        if (!m_notifyKeyGenerationCallback.IsNull())
        {
            m_notifyKeyGenerationCallback(data);
        }

        break;
    }
    case KEY_GEN_FAILURE: {
        QkdProtocolStatus status;
        packet->CopyData(reinterpret_cast<uint8_t*>(&status), sizeof(status));

        KeyGenerationData data{status};
        if (!m_notifyKeyGenerationCallback.IsNull())
        {
            m_notifyKeyGenerationCallback(data);
        }
        break;
    }
    }
}

void
B92QkdProtocol::SendQBitHelper()
{
    NS_LOG_FUNCTION(this);

    Bit bit = static_cast<Bit>(m_bitString[m_nextBitIndex] - '0');
    Ptr<QBit> qbit = CreateObject<QBit>(
        m_helper.GenerateQBit(bit == 0 ? QuantumHelper::zero : QuantumHelper::plus));
    m_sendQBitCallback(qbit);
}

} // namespace ns3
