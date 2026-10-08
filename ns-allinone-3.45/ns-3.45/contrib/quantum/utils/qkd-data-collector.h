/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Shashank G <shashankgirish07@gmail.com>
 *         Navaneet Y V N <navaneetyvn.work@gmail.com>
 *         Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_DATA_COLLECTOR_H
#define QKD_DATA_COLLECTOR_H

#include "ns3/address.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4.h"
#include "ns3/nstime.h" // Include for Time type
#include "ns3/object.h"
#include "ns3/sqlite-data-output.h"
#include "ns3/tcp-socket-factory.h"

#include <list>
#include <string>

namespace ns3
{

enum KeyStatus
{
    KEY_IN_USE,
    KEY_NOT_IN_USE,
    KEY_EXPIRED,
    KEY_DELETED,
    KEY_GENERATED,
    KEY_NOT_GENERATED,
};

class DataCalculator;

//------------------------------------------------------------
//--------------------------------------------
/**
 * List of Ptrs to DataCalculator objects
 */
typedef std::list<Ptr<DataCalculator>> DataCalculatorList;
/**
 * List of pairs of strings representing metadata
 */
typedef std::list<std::pair<std::string, std::string>> MetadataList;

/**
 * @ingroup dataoutput
 * @class DataCollector
 * @brief Collects data
 */
class QKDDataCollector : public Object
{
  public:
    /**
     * Constructor
     */
    QKDDataCollector();

    /**
     * Destructor
     */
    ~QKDDataCollector() override;

    /**
     * Register this type.
     * @return The TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Provide specific parameters to the DataCollector
     * @param ksid Key Session ID
     * @param key key buffer string
     * @param src Source ID
     * @param dst Destination ID
     * @param status Status of the response
     * @param createdOn Time when the key was created
     * @param expiresBy Time when the key expires
     * @param keyChunkSize Size of the key chunk
     */
    void DescribeRun(uint32_t ksid,
                     std::string key,
                     Address src,
                     Address dst,
                     KeyStatus status,
                     Time createdOn,
                     Time expiresBy,
                     uint32_t keyChunkSize);

    /**
     * @brief Get the source ID
     * @return m_src Source ID
     */
    Address GetSourceID() const;

    /**
     * @brief Get the destination ID
     * @return m_dst Destination ID
     */
    Address GetDestinationID() const;

    /**
     * @brief Get the key session ID
     * @return Key Session ID
     */
    uint32_t GetKeySessionID() const;

    /**
     * @brief Get the key buffer
     * @return Key Buffer
     */
    std::string GetKeyBuffer() const;

    /**
     * @brief Get the status of the response
     * @return Status of the response
     */
    KeyStatus GetStatus() const;

    /**
     * @brief Get the time when the key was created
     * @return Time when the key was created
     */
    Time GetCreatedOn() const;

    /**
     * @brief Get the time when the key expires
     * @return Time when the key expires
     */
    Time GetExpiresBy() const;

    /**
     * @brief Get the size of the key chunk
     * @return Size of the key chunk
     */
    uint32_t GetKeyChunkSize() const;

    /**
     * Add the key and the value as a pair of strings to the metadata list
     * @param key Key value to include
     * @param value Value to include of type string
     */
    void AddMetadata(std::string key, std::string value);
    /**
     * Add the key and the value as a pair of strings to the metadata list
     * @param key Key value to include
     * @param value Value to include of type double
     */
    void AddMetadata(std::string key, double value);
    /**
     * Add the key and the value as a pair of strings to the metadata list
     * @param key Key value to include
     * @param value Value to include of type uint32_t
     */
    void AddMetadata(std::string key, uint32_t value);
    /**
     * Returns an iterator to the beginning of the metadata list
     * @return Iterator pointing to the first value of the metadata list
     */
    MetadataList::iterator MetadataBegin();
    /**
     * Returns an iterator to the past-the-end of the metadata list
     * @return Iterator pointing to the past-the-end element of the metadata list
     */
    MetadataList::iterator MetadataEnd();

    /**
     * Add a DataCalculator object to the DataCollector
     * @param datac DataCalculator object to be added
     */
    void AddDataCalculator(Ptr<DataCalculator> datac);
    /**
     * Returns an iterator to the beginning of the DataCalculator list
     * @return Iterator pointing to the first value of the DataCalculator list
     */
    DataCalculatorList::iterator DataCalculatorBegin();
    /**
     * Returns an iterator to the past-the-end of the DataCalculator list
     * @return Iterator pointing to the past-the-end element of the DataCalculator list
     */
    DataCalculatorList::iterator DataCalculatorEnd();

  protected:
    void DoDispose() override;

  private:
    uint32_t m_ksid;    //!< Key Session ID
    std::string m_key;  //!< Key Buffer - Key buffer is an array of bits packed into octets (char*)
                        //!< ordered such that bit[0] of octet[0] is the 1 st bit and bit[7] of
                        //!< octet[n] is the 8*n+8 th bit.
    Address m_src;      //!< Source of Application
    Address m_dst;      //!< Destination of Application
    KeyStatus m_status; //!< Status of the response
    Time m_createdOn;   //!< Time when the key was created
    Time m_expiresBy;   //!< Time when the key expires
    uint32_t m_keyChunkSize; //!< Size of the key chunk

    MetadataList m_metadata;       //!< List of experiment metadata
    DataCalculatorList m_calcList; //!< List of data calculators

    // end class DataCollector
};

// end namespace ns3
}; // namespace ns3

#endif /* DATA_COLLECTOR_H */
