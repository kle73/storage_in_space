/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Shashank G <shashankgirish07@gmail.com>
 *         Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-app-trailer.h"

#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("QkdAppTrailer");

namespace ns3
{
QkdAppTrailer::QkdAppTrailer()
    : m_keyOrData{}
{
    NS_LOG_FUNCTION(this);
}

QkdAppTrailer::~QkdAppTrailer()
{
    NS_LOG_FUNCTION(this);
}

void
QkdAppTrailer::SetKeyOrData(const std::string& key)
{
    NS_LOG_FUNCTION(this << key);
    m_keyOrData = key;
}

std::string
QkdAppTrailer::GetKeyOrData() const
{
    NS_LOG_FUNCTION(this);
    return m_keyOrData;
}

TypeId
QkdAppTrailer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::QKDTrailer").SetParent<Trailer>().AddConstructor<QkdAppTrailer>();
    return tid;
}

TypeId
QkdAppTrailer::GetInstanceTypeId() const
{
    NS_LOG_FUNCTION(this);
    return GetTypeId();
}

void
QkdAppTrailer::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << "QKD Trailer: Key = " << m_keyOrData;
}

uint32_t
QkdAppTrailer::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 1 + m_keyOrData.size(); // 1 byte for key size + key size
}

void
QkdAppTrailer::Serialize(Buffer::Iterator end) const
{
    end.Prev(GetSerializedSize()); // Move iterator to the end

    // Sanity check to avoid overflow in 1-byte length field
    NS_ASSERT_MSG(m_keyOrData.size() <= 255, "Key size too large to fit in one byte!");
    uint8_t keySize = static_cast<uint8_t>(m_keyOrData.size());

    for (char c : m_keyOrData)
    {
        end.WriteU8(static_cast<uint8_t>(c)); // Write each char as a byte
    }
    end.WriteU8(keySize); // Write length first

    NS_LOG_INFO("Serialized QKD Trailer with total size: " << GetSerializedSize());
}

uint32_t
QkdAppTrailer::Deserialize(Buffer::Iterator end)
{
    NS_LOG_FUNCTION(this << &end);

    end.Prev(1);

    uint8_t keySize = end.ReadU8();

    end.Prev(keySize + 1); // Move iterator back to the start of the key
    NS_LOG_INFO("Deserializing QKD Trailer with key size: " << static_cast<uint32_t>(keySize));
    m_keyOrData.clear();
    m_keyOrData.reserve(keySize);

    for (uint8_t i = 0; i < keySize; ++i)
    {
        m_keyOrData.push_back(static_cast<char>(end.ReadU8()));
    }

    return 1 + keySize; // total bytes read
}

} // namespace ns3
