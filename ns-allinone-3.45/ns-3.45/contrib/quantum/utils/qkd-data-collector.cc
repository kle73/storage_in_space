/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Navaneet Y V N <navaneetyvn.work@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-data-collector.h"

#include "ns3/data-calculator.h"
#include "ns3/log.h"
#include "ns3/object.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QKDDataCollector");

QKDDataCollector::QKDDataCollector()
{
    NS_LOG_FUNCTION(this);
}

QKDDataCollector::~QKDDataCollector()
{
    NS_LOG_FUNCTION(this);
}

TypeId
QKDDataCollector::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QKDDataCollector")
                            .SetParent<Object>()
                            .SetGroupName("Stats")
                            .AddConstructor<QKDDataCollector>();
    return tid;
}

void
QKDDataCollector::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_calcList.clear();
    m_metadata.clear();

    Object::DoDispose();
    // end QKDDataCollector::DoDispose
}

void
QKDDataCollector::DescribeRun(uint32_t ksid,
                              std::string key,
                              Address src,
                              Address dst,
                              KeyStatus status,
                              Time createdOn,
                              Time expiresBy,
                              uint32_t keyChunkSize)
{
    m_ksid = ksid;
    m_key = key;
    m_src = src;
    m_dst = dst;
    m_status = status;
    m_createdOn = createdOn;
    m_expiresBy = expiresBy;
    m_keyChunkSize = keyChunkSize;
}

uint32_t
QKDDataCollector::GetKeySessionID() const
{
    return m_ksid;
}

std::string
QKDDataCollector::GetKeyBuffer() const
{
    return m_key;
}

Address
QKDDataCollector::GetSourceID() const
{
    return m_src;
}

Address
QKDDataCollector::GetDestinationID() const
{
    return m_dst;
}

KeyStatus
QKDDataCollector::GetStatus() const
{
    return m_status;
}

Time
QKDDataCollector::GetCreatedOn() const
{
    return m_createdOn;
}

Time
QKDDataCollector::GetExpiresBy() const
{
    return m_expiresBy;
}

uint32_t
QKDDataCollector::GetKeyChunkSize() const
{
    return m_keyChunkSize;
}

void
QKDDataCollector::AddDataCalculator(Ptr<DataCalculator> datacalc)
{
    m_calcList.push_back(datacalc);
}

DataCalculatorList::iterator
QKDDataCollector::DataCalculatorBegin()
{
    return m_calcList.begin();
}

DataCalculatorList::iterator
QKDDataCollector::DataCalculatorEnd()
{
    return m_calcList.end();
}

void
QKDDataCollector::AddMetadata(std::string key, std::string value)
{
    std::pair<std::string, std::string> blob(key, value);
    m_metadata.push_back(blob);
}

void
QKDDataCollector::AddMetadata(std::string key, uint32_t value)
{
    std::stringstream st;
    st << value;

    std::pair<std::string, std::string> blob(key, st.str());
    m_metadata.push_back(blob);
}

void
QKDDataCollector::AddMetadata(std::string key, double value)
{
    std::stringstream st;
    st << value;

    std::pair<std::string, std::string> blob(key, st.str());
    m_metadata.push_back(blob);
}

MetadataList::iterator
QKDDataCollector::MetadataBegin()
{
    return m_metadata.begin();
}

MetadataList::iterator
QKDDataCollector::MetadataEnd()
{
    return m_metadata.end();
}
} // namespace ns3
