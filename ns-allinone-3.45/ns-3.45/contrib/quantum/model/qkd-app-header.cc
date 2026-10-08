/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Navaneet Y V N <navaneetyvn.work@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-app-header.h"

#include "ns3/address-utils.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("QkdAppHeader");

namespace ns3
{
QkdAppHeader::QkdAppHeader()
{
}

QkdAppHeader::~QkdAppHeader()
{
}

TypeId
QkdAppHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::QkdAppHeader").SetParent<Header>().AddConstructor<QkdAppHeader>();
    return tid;
}

TypeId
QkdAppHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
QkdAppHeader::Print(std::ostream& os) const
{
    // This method is invoked by the packet printing
    // routines to print the content of my header.
    // os << "data=" << m_data << std::endl;

    if (m_type != QKD_APP_IV)
    {
        os << "ksid=" << m_ksid << " "
           << "source=" << m_source << " "
           << "destination=" << m_destination << " "
           << "type=" << m_type;
    }
    else
    {
        os << "ksid=" << m_ksid << " "
           << "iv=" << m_iv << " "
           << "encryptionAlgo=" << m_encryptionAlgo << "type=" << m_type;
    }
}

uint32_t
QkdAppHeader::GetSerializedSize() const
{
    uint32_t size = 0;
    if (m_type != QKD_APP_IV)
    {
        size = 7; // 1 byte for type, 4 bytes for ksid, 1 byte for source length, 1
                  // byte for destination length
        size += m_source.GetLength();
        size += m_destination.GetLength();
        NS_LOG_FUNCTION(this << size);
    }
    else
    {
        size = 7; // 1 byte for type, 4 bytes for ksid, 1 byte for encryptionAlgo,
                  // 1 byte for IV size
        size += m_iv.size();
        NS_LOG_FUNCTION(this << size);
    }
    return size;
}

void
QkdAppHeader::Serialize(Buffer::Iterator it) const
{
    NS_LOG_FUNCTION(this);
    // we can serialize two bytes at the start of the buffer.
    // we write them in network byte order.
    Buffer::Iterator start = it;
    start.WriteU8(static_cast<uint8_t>(m_type));
    start.WriteHtonU32(m_ksid);
    if (m_type != QKD_APP_IV)
    {
        NS_LOG_INFO("Size of Source Address: " << static_cast<uint32_t>(m_source.GetLength()));
        NS_LOG_INFO(
            "Size of Destination Address: " << static_cast<uint32_t>(m_destination.GetLength()));

        start.WriteU8(m_source.GetLength());
        start.WriteU8(m_destination.GetLength());
        WriteTo(start, m_source);
        WriteTo(start, m_destination);
    }
    else
    {
        NS_LOG_INFO("Size of IV: " << m_iv.size());
        start.WriteU8(m_encryptionAlgo);
        start.WriteU8(m_iv.size());
        for (const auto& c : m_iv)
        {
            start.WriteU8(static_cast<uint8_t>(c));
        }
    }
    NS_LOG_INFO("Serialized Size: " << start.GetDistanceFrom(it));
}

uint32_t
QkdAppHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);
    // we can deserialize two bytes from the start of the buffer.
    // we read them in network byte order and store them
    // in host byte order.
    Buffer::Iterator cur = start;
    m_type = static_cast<QkdAppHeaderType>(cur.ReadU8());
    m_ksid = cur.ReadNtohU32();
    if (m_type != QKD_APP_IV)
    {
        uint32_t sourceLength = cur.ReadU8();
        uint32_t destinationLength = cur.ReadU8();

        NS_LOG_INFO("Size of Source Address: " << sourceLength);
        NS_LOG_INFO("Size of Destination Address: " << destinationLength);
        ReadFrom(cur, m_source, sourceLength);
        ReadFrom(cur, m_destination, destinationLength);

        NS_LOG_INFO("Deserialized Size: " << cur.GetDistanceFrom(start));
    }
    else
    {
        m_encryptionAlgo = static_cast<SymmetricEncryptionAlgo>(cur.ReadU8());
        uint32_t ivSize = cur.ReadU8();
        NS_LOG_INFO("Size of IV: " << ivSize);
        m_iv.clear();
        m_iv.reserve(ivSize);
        for (uint32_t i = 0; i < ivSize; ++i)
        {
            m_iv.push_back(static_cast<char>(cur.ReadU8()));
        }
        NS_LOG_INFO("Deserialized Size: " << cur.GetDistanceFrom(start) << ", IV: " << m_iv);
    }

    return cur.GetDistanceFrom(start);
}

void
QkdAppHeader::SetKSID(uint32_t data)
{
    m_ksid = data;
}

uint32_t
QkdAppHeader::GetKSID() const
{
    return m_ksid;
}

void
QkdAppHeader::SetSource(Address source)
{
    m_source = source;
}

Address
QkdAppHeader::GetSource() const
{
    return m_source;
}

void
QkdAppHeader::SetDestination(Address destination)
{
    m_destination = destination;
}

Address
QkdAppHeader::GetDestination() const
{
    return m_destination;
}

void
QkdAppHeader::SetHeaderType(QkdAppHeaderType type)
{
    m_type = type;
}

QkdAppHeaderType
QkdAppHeader::GetHeaderType() const
{
    return m_type;
}

void
QkdAppHeader::SetIV(std::string iv)
{
    m_iv = iv;
}

std::string
QkdAppHeader::GetIV() const
{
    return m_iv;
}

void
QkdAppHeader::SetEncryptionAlgo(SymmetricEncryptionAlgo algo)
{
    m_encryptionAlgo = algo;
}

SymmetricEncryptionAlgo
QkdAppHeader::GetEncryptionAlgo() const
{
    return m_encryptionAlgo;
}

} // namespace ns3
