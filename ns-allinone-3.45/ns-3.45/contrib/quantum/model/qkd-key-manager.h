/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Navaneet Y V N <navaneetyvn.work@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_KEY_MANAGER_H
#define QKD_KEY_MANAGER_H

#include "qkd-device.h"

#include "ns3/address.h"
#include "ns3/data-output-interface.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/qkd-data-collector.h"
#include "ns3/sqlite-output.h"

namespace ns3
{

class QkdKeyManagementLayer;
struct KeyMetadata;

class QkdKeyManager : public DataOutputInterface
{
    /**
     * @brief QkdKeyManager class in order to manage the keys and interface between Application and
     * QKDDevice
     */

  public:
    /**
     * @brief QkdKeyManager constructor
     * List of devices the Key Manager is associated with
     * @param deviceList List of devices
     */
    QkdKeyManager(std::vector<Ptr<QkdDevice>> deviceList);
    /**
     * @brief QkdKeyManager destructor
     */
    ~QkdKeyManager() override;

    /**
     * @brief Gets TypeId of the QkdKeyManager
     * @return TypeId of the QkdKeyManager
     */
    static TypeId GetTypeId();

    /**
     * @brief Gets the key
     * @param ksid Key Session ID
     * @param source Source of Application
     * @param destination Destination of Application
     * @return std::string Key Buffer - Key buffer is an array of bits packed into octets (char*)
     * ordered such that bit[0] of octet[0] is the 1 st bit and bit[7] of octet[n] is the 8*n+8 th
     * bit.
     */
    std::string GetKey(uint32_t ksid, const Address& source, const Address& destination);

    /**
     * @brief Opens a connection for the key session
     * @param ksid Key Session ID
     * @param src Source of Application
     * @param dst Destination of Application
     * @return uint32_t Key Session ID
     */
    uint32_t OpenConnect(uint32_t ksid, const Address& src, const Address& dst);

    /**
     * @brief Opens a connection for the key session
     * @param src Source of Application
     * @param dst Destination of Application
     * @return uint32_t Key Session ID
     */
    uint32_t OpenConnect(const Address& src, const Address& dst);

    /**
     * @brief Outputs the data to the SQLite database
     * @param dc Data Collector
     */
    void Output(DataCollector& dc) override;

    /**
     * @brief Gets the key session ID
     * @param src Source of Application
     * @param dst Destination of Application
     * @return uint32_t Key Session ID
     */
    uint32_t CreateKeySession(QKDDataCollector& dc);

    /**
     * @brief Deletes the key session ID
     * @param ksid Key Session ID
     * @param source Source of Application
     * @param destination Destination of Application
     */
    void DeleteKeySession(uint32_t ksid, const Address& source, const Address& destination);

    /**
     * @brief Verify if Key is valid
     * @param ksid Key Session ID
     * @return true if key is valid, false otherwise
     */
    bool IsKeyValid(uint32_t ksid);

    /**
     * @brief Adds a key management layer to map
     * @param qkdDevice QkdDevice pointer to the QkdDevice
     */
    void AddKeyManagementLayer(Ptr<QkdDevice> qkdDevice);

    /**
     * @brief Adds a mapping of IP address to device ID
     * @param ip Address of the IP
     * @param deviceId Address of the device ID
     */
    void AddIpToDeviceMapping(Address ip, Address deviceId);

    /**
     * @brief Deletes the mapping of IP address to device ID
     * @param ip Address of the IP
     */
    void DeleteIpToDeviceMapping(const Address& ip);

    /**
     * @brief Gets the key management layer
     * @param ip Address of the IP
     * @return QkdKeyManagementLayer pointer to the key management layer
     */
    Ptr<QkdKeyManagementLayer> GetKeyManagementLayer(const Address& ip);

    /**
     * @brief removes the key management layer from the map
     * @param deviceId Device ID
     * @return true if key management layer is removed, false otherwise
     */
    bool RemoveKeyManagementLayer(Address deviceId);

    /**
     * @brief Maps application to a free device
     * @param appId Application ID
     */
    void MapApplicationToDevice(uint32_t appId);

    /**
     * @brief Gets the device ID for the application
     * @param appId Application ID
     * @return Device Address
     */
    Address GetDeviceIdForApplication(uint32_t appId);

    /**
     * @brief Start Key Generation in all Key Management Layers
     */
    void Start();

    /**
     * @brief Stop Key Generation in all Key Management Layers
     */
    void Stop();

  private:
    /**
     * Gets Max Key Session ID and increments it
     * @return uint32_t Max Key Session ID
     */
    uint32_t IncrementMaxKsid();

    /**
     * Converts an Address to a string
     * @param address Address to convert
     * @return adrStr String representation of the address
     */
    std::string AddressToString(const Address& address);

    /**
     * @brief Callback function to handle key generation completion
     * @param keyGenerationData Key generation data containing the status and generated key
     * @note This function is called when the key generation process is completed.
     * It handles the key generation callback and updates the key management layer accordingly.
     */
    void OnKeyReady(std::pair<std::string, std::pair<Address, KeyMetadata>> keyEntry,
                    uint32_t ksid,
                    const Address& src,
                    const Address& dst);

    /**
     * @ingroup quantum
     *
     * @brief Class to generate SQLite output for QKD Table
     */
    class QKDOutputCallback : public DataOutputCallback
    {
      public:
        /**
         * @brief QKDOutputCallback constructor
         * @param db pointer to the instance this object belongs to
         * @param run experiment descriptor
         */
        QKDOutputCallback(const Ptr<SQLiteOutput>& db, uint32_t ksid);

        /**
         * @brief QKDOutputCallback destructor
         */
        ~QKDOutputCallback() override;

        /**
         * @brief Generates data statistics
         * @param key the SQL key to use
         * @param variable the variable name
         * @param statSum the stats to print
         */
        void OutputStatistic(std::string key,
                             std::string variable,
                             const StatisticalSummary* statSum) override;

        /**
         * @brief Generates a single data output
         * @param key the SQL key to use
         * @param variable the variable name
         * @param val the value
         */
        void OutputSingleton(std::string key, std::string variable, int val) override;

        /**
         * @brief Generates a single data output
         * @param key the SQL key to use
         * @param variable the variable name
         * @param val the value
         */
        void OutputSingleton(std::string key, std::string variable, uint32_t val) override;

        /**
         * @brief Generates a single data output
         * @param key the SQL key to use
         * @param variable the variable name
         * @param val the value
         */
        void OutputSingleton(std::string key, std::string variable, double val) override;

        /**
         * @brief Generates a single data output
         * @param key the SQL key to use
         * @param variable the variable name
         * @param val the value
         */
        void OutputSingleton(std::string key, std::string variable, std::string val) override;

        /**
         * @brief Generates a single data output
         * @param key the SQL key to use
         * @param variable the variable name
         * @param val the value
         */
        void OutputSingleton(std::string key, std::string variable, Time val) override;

      private:
        Ptr<SQLiteOutput> m_db;                   //!< Pointer to the SQLite database
        sqlite3_stmt* m_insertSingletonStatement; //!< SQLite statement for inserting a single value
        uint32_t m_ksid;                          //!< Key Session ID
    };

    Ptr<SQLiteOutput> m_sqlite; //!< Pointer to the SQLite database
    std::map<Address, Ptr<QkdKeyManagementLayer>>
        m_deviceMapping; //!< Mapping of devices to key management layers to source
    std::map<Address, Address> m_ipToDeviceMapping; //!< Mapping of applications to devices
};
} // namespace ns3

#endif /* QKD_KEY_MANAGER_H */
