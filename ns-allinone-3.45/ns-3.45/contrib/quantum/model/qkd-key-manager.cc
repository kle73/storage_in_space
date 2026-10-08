/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Navaneet Y V N <navaneetyvn.work@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "qkd-key-manager.h"

#include "qkd-key-management-layer.h"

#include "ns3/abort.h"
#include "ns3/address.h"
#include "ns3/data-output-interface.h"
#include "ns3/hashing.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/qkd-data-collector.h"
#include "ns3/sqlite-output.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("QkdKeyManager");
NS_OBJECT_ENSURE_REGISTERED(QkdKeyManager);

QkdKeyManager::QkdKeyManager(std::vector<Ptr<QkdDevice>> deviceList)
    : DataOutputInterface()
{
    NS_LOG_FUNCTION(this);
    m_filePrefix = "data";
    m_deviceMapping = std::map<Address, Ptr<QkdKeyManagementLayer>>();
    for (auto device : deviceList)
    {
        // Create Key Management Layer for each device
        Ptr<QkdKeyManagementLayer> keyManagementLayer =
            CreateObject<QkdKeyManagementLayer>(device, 32);
        // Add Key Management Layer to the device mapping
        m_deviceMapping[device->GetAddress()] = keyManagementLayer;
    }
}

QkdKeyManager::~QkdKeyManager()
{
    NS_LOG_FUNCTION(this);
    m_deviceMapping.clear();
    m_sqlite->Unref();
}

TypeId
QkdKeyManager::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::QkdKeyManager").SetParent<DataOutputInterface>().SetGroupName("Quantum");

    return tid;
}

void
QkdKeyManager::AddKeyManagementLayer(Ptr<QkdDevice> device)
{
    NS_LOG_FUNCTION(this);

    // Create Key Management Layer for each device
    Ptr<QkdKeyManagementLayer> keyManagementLayer = CreateObject<QkdKeyManagementLayer>(device, 32);
    // Add Key Management Layer to the device mapping
    m_deviceMapping[device->GetAddress()] = keyManagementLayer;
}

bool
QkdKeyManager::RemoveKeyManagementLayer(Address deviceId)
{
    NS_LOG_FUNCTION(this);
    for (auto it = m_deviceMapping.begin(); it != m_deviceMapping.end(); ++it)
    {
        if (it->first == deviceId)
        {
            m_deviceMapping.erase(it);
            return true;
        }
    }
    return false;
}

Ptr<QkdKeyManagementLayer>
QkdKeyManager::GetKeyManagementLayer(const Address& ip)
{
    NS_LOG_FUNCTION(this);
    // Get corresponding device ID from the IP address
    auto it = m_ipToDeviceMapping.find(ip);
    if (it != m_ipToDeviceMapping.end())
    {
        Address deviceId = it->second;
        // Check if the device ID exists in the mapping
        auto deviceIt = m_deviceMapping.find(deviceId);
        if (deviceIt != m_deviceMapping.end())
        {
            NS_LOG_DEBUG("Found Key Management Layer for device ID: " << deviceId);
            return deviceIt->second;
        }
        else
        {
            NS_LOG_ERROR("Device ID not found in the mapping");
            return nullptr;
        }
    }
    else
    {
        NS_LOG_ERROR("IP address not found in the mapping");
        return nullptr;
    }
}

void
QkdKeyManager::AddIpToDeviceMapping(Address ip, Address deviceId)
{
    NS_LOG_FUNCTION(this);
    // Check if the deviceId is already present in the mapping
    if (m_ipToDeviceMapping.find(ip) != m_ipToDeviceMapping.end())
    {
        NS_LOG_ERROR("IP address already exists in the mapping");
        return;
    }
    if (m_deviceMapping.find(deviceId) == m_deviceMapping.end())
    {
        NS_LOG_ERROR("Device ID not found in the mapping");
        return;
    }
    m_ipToDeviceMapping[ip] = deviceId;
}

void
QkdKeyManager::DeleteIpToDeviceMapping(const Address& ip)
{
    NS_LOG_FUNCTION(this);
    // Check if the IP address exists in the mapping
    auto it = m_ipToDeviceMapping.find(ip);
    if (it != m_ipToDeviceMapping.end())
    {
        m_ipToDeviceMapping.erase(it);
    }
    else
    {
        NS_LOG_ERROR("IP address not found in the mapping");
    }
}

// Just to solve override issue
void
QkdKeyManager::Output(DataCollector& dc)
{
    NS_LOG_FUNCTION(this);
}

uint32_t
QkdKeyManager::IncrementMaxKsid()
{
    NS_LOG_FUNCTION(this);

    if (m_sqlite == nullptr)
    {
        NS_LOG_ERROR("SQLite database not initialized");
        return 1;
    }

    sqlite3_stmt* stmt;
    bool res = m_sqlite->WaitPrepare(&stmt, "SELECT MAX(ksid) FROM KeyManager");
    NS_ASSERT(res);

    res = SQLiteOutput::SpinStep(stmt);
    NS_ASSERT(res);
    int maxKsid = m_sqlite->RetrieveColumn<int>(stmt, 0);
    res = SQLiteOutput::SpinFinalize(stmt);
    NS_ASSERT(res == 0);

    if (maxKsid < 0)
    {
        maxKsid = 0;
    }
    return ++maxKsid;
}

std::string
QkdKeyManager::AddressToString(const Address& address)
{
    NS_LOG_FUNCTION(this << address);

    // IPv4 address with port
    if (InetSocketAddress::IsMatchingType(address))
    {
        InetSocketAddress inetAddr = InetSocketAddress::ConvertFrom(address);
        Ipv4Address ip = inetAddr.GetIpv4();
        uint16_t port = inetAddr.GetPort();

        uint32_t ipVal = ip.Get(); // host order
        uint8_t a = (ipVal >> 24) & 0xFF;
        uint8_t b = (ipVal >> 16) & 0xFF;
        uint8_t c = (ipVal >> 8) & 0xFF;
        uint8_t d = ipVal & 0xFF;

        char buffer[40];
        snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u:%u", a, b, c, d, port);
        return std::string(buffer);
    }

    // Raw IPv4 address (no port)
    else if (Ipv4Address::IsMatchingType(address))
    {
        Ipv4Address ip = Ipv4Address::ConvertFrom(address);
        uint32_t ipVal = ip.Get();
        uint8_t a = (ipVal >> 24) & 0xFF;
        uint8_t b = (ipVal >> 16) & 0xFF;
        uint8_t c = (ipVal >> 8) & 0xFF;
        uint8_t d = ipVal & 0xFF;

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", a, b, c, d);
        return std::string(buffer);
    }

    // IPv6 address with port
    else if (Inet6SocketAddress::IsMatchingType(address))
    {
        Inet6SocketAddress inet6Addr = Inet6SocketAddress::ConvertFrom(address);
        Ipv6Address ip6 = inet6Addr.GetIpv6();
        uint16_t port = inet6Addr.GetPort();
        uint8_t bytes[16];
        ip6.GetBytes(bytes);

        char ipStr[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, bytes, ipStr, INET6_ADDRSTRLEN))
        {
            char buffer[96];
            snprintf(buffer, sizeof(buffer), "[%s]:%u", ipStr,
                     port); // IPv6 with port
            return std::string(buffer);
        }
        return "invalid-ipv6";
    }

    // Raw IPv6 address (no port)
    else if (Ipv6Address::IsMatchingType(address))
    {
        Ipv6Address ip6 = Ipv6Address::ConvertFrom(address);
        uint8_t bytes[16];
        ip6.GetBytes(bytes);

        char ipStr[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, bytes, ipStr, INET6_ADDRSTRLEN))
        {
            return std::string(ipStr); // IPv6 without port
        }
        return "invalid-ipv6";
    }

    return "unknown-address";
}

uint32_t
QkdKeyManager::CreateKeySession(QKDDataCollector& dc)
{
    std::string src = AddressToString(dc.GetSourceID());
    std::string dst = AddressToString(dc.GetDestinationID());
    NS_LOG_FUNCTION(this << dc.GetKeySessionID() << " " << src << " " << dst);

    std::string m_dbFile = m_filePrefix + ".db";
    bool res;

    m_sqlite = new SQLiteOutput(m_dbFile);

    res = m_sqlite->SpinExec("CREATE TABLE IF NOT EXISTS KeyManager ("
                             "ksid INTEGER, "
                             "keyVal TEXT, src TEXT, dst TEXT, status INTEGER, "
                             "createdOn INTEGER, expiresBy INTEGER, keyChunkSize INTEGER,"
                             "PRIMARY KEY(ksid,src,dst))");
    NS_ASSERT(res);

    sqlite3_stmt* stmt;
    res = m_sqlite->WaitPrepare(
        &stmt,
        "INSERT INTO KeyManager "
        "(ksid, keyVal, src, dst, status, createdOn, expiresBy, keyChunkSize) "
        "values (?, ?, ?, ?, ?, ?, ?, ?)");

    NS_ASSERT(res);
    uint32_t ksid;
    if (!dc.GetKeySessionID())
    {
        ksid = IncrementMaxKsid();
    }
    else
    {
        ksid = dc.GetKeySessionID();
    }

    res = m_sqlite->Bind(stmt, 1, ksid);
    NS_ASSERT(res);

    if (dc.GetKeyBuffer().empty())
    {
        res = m_sqlite->BindNull(stmt, 2);
        NS_ASSERT(res);
    }
    else
    {
        res = m_sqlite->Bind(stmt, 2, dc.GetKeyBuffer());
        NS_ASSERT(res);
    }

    res = m_sqlite->Bind(stmt, 3, src);
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 4, dst);
    NS_ASSERT(res);
    NS_ASSERT(src != dst);
    res = m_sqlite->Bind(stmt, 5, static_cast<int>(dc.GetStatus()));
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 6, dc.GetCreatedOn());
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 7, dc.GetExpiresBy());
    NS_ASSERT(res);
    if (dc.GetKeyChunkSize() == 0)
    {
        res = m_sqlite->BindNull(stmt, 8);
        NS_ASSERT(res);
    }
    else
    {
        res = m_sqlite->Bind(stmt, 8, dc.GetKeyChunkSize());
        NS_ASSERT(res);
    }

    res = SQLiteOutput::SpinStep(stmt);
    NS_ASSERT(res);
    res = SQLiteOutput::SpinFinalize(stmt);
    NS_ASSERT(res == 0);

    res = m_sqlite->WaitExec("CREATE TABLE IF NOT EXISTS "
                             "Metadata (ksid integer, keyVal text, value)");
    NS_ASSERT(res);

    res = m_sqlite->WaitPrepare(&stmt,
                                "INSERT INTO Metadata "
                                "(ksid, keyVal, value)"
                                "values (?, ?, ?)");

    for (auto i = dc.MetadataBegin(); i != dc.MetadataEnd(); i++)
    {
        const auto& blob = (*i);
        SQLiteOutput::SpinReset(stmt);
        m_sqlite->Bind(stmt, 1, dc.GetKeySessionID());
        m_sqlite->Bind(stmt, 2, blob.first);
        m_sqlite->Bind(stmt, 3, blob.second);
        SQLiteOutput::SpinStep(stmt);
    }

    SQLiteOutput::SpinFinalize(stmt);

    m_sqlite->SpinExec("BEGIN");
    QKDOutputCallback callback(m_sqlite, dc.GetKeySessionID());
    for (auto i = dc.DataCalculatorBegin(); i != dc.DataCalculatorEnd(); i++)
    {
        (*i)->Output(callback);
    }
    m_sqlite->SpinExec("COMMIT");

    return ksid;
}

void
QkdKeyManager::DeleteKeySession(uint32_t ksid, const Address& src, const Address& dst)
{
    NS_LOG_FUNCTION(this << ksid << src << dst);
    // Update the status of the key session to KEY_DELETED

    if (m_sqlite == nullptr)
    {
        NS_LOG_ERROR("SQLite database not initialized");
        return;
    }
    std::string srcStr = AddressToString(src);
    std::string dstStr = AddressToString(dst);
    NS_LOG_DEBUG("Deleting key session with ksid: " << ksid << ", src: " << srcStr
                                                    << ", dst: " << dstStr);
    m_sqlite->SpinExec("BEGIN");
    sqlite3_stmt* stmt;
    bool res = m_sqlite->WaitPrepare(&stmt,
                                     "UPDATE KeyManager SET status = ? WHERE "
                                     "ksid = ? AND src = ? AND dst = ?");
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 1, static_cast<int>(KeyStatus::KEY_DELETED));
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 2, ksid);
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 3, srcStr);
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 4, dstStr);
    NS_ASSERT(res);

    res = SQLiteOutput::SpinStep(stmt);
    NS_ASSERT(res);
    res = SQLiteOutput::SpinFinalize(stmt);
    NS_ASSERT(res == 0);

    m_sqlite->SpinExec("COMMIT");

    DeleteIpToDeviceMapping(src);

    // m_sqlite->Unref();
}

std::string
QkdKeyManager::GetKey(uint32_t ksid, const Address& src, const Address& dst)
{
    NS_LOG_FUNCTION(this);

    std::string srcStr = AddressToString(src);
    std::string dstStr = AddressToString(dst);

    sqlite3_stmt* stmt;
    bool res = m_sqlite->WaitPrepare(
        &stmt,
        "SELECT keyVal FROM KeyManager WHERE ksid = ? AND src = ? AND dst = ?");
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 1, ksid);
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 2, srcStr);
    NS_ASSERT(res);
    res = m_sqlite->Bind(stmt, 3, dstStr);
    NS_ASSERT(res);

    res = SQLiteOutput::SpinStep(stmt);
    NS_ASSERT(res);
    std::string key = m_sqlite->RetrieveColumn<std::string>(stmt, 0);
    res = SQLiteOutput::SpinFinalize(stmt);
    NS_ASSERT(res == 0);

    if (key.empty())
    {
        NS_LOG_INFO("No key found for given ksid, picking key from key buffer");
        Ptr<QkdKeyManagementLayer> layer = GetKeyManagementLayer(src);
        if (layer == nullptr)
        {
            NS_LOG_ERROR("No Key Management Layer found for source address");
            return "";
        }
        std::pair<std::string, std::pair<Address, KeyMetadata>> keyPair = layer->GetKeyFromBuffer();

        if (keyPair.first.empty())
        {
            NS_LOG_ERROR("No key found in the key buffer for destination address");
            HashingAlgorithm hasher;
            return hasher.HashData("why_god_why", SHA256);
        }
        key.assign(keyPair.first);
        NS_LOG_INFO("Key found in key buffer: " << key);

        // Get Key Metadata
        KeyMetadata keyParams = keyPair.second.second;

        // Update the key session in the database with key metadata
        m_sqlite->SpinExec("BEGIN");
        sqlite3_stmt* updateStmt;
        res = m_sqlite->WaitPrepare(&updateStmt,
                                    "UPDATE KeyManager SET keyVal = ?, status = ?, "
                                    "createdOn = ?, expiresBy = ?, "
                                    "keyChunkSize = ? "
                                    "WHERE ksid = ? AND src = ? AND dst = ?");
        NS_ASSERT(res);
        res = m_sqlite->Bind(updateStmt, 1, key);
        NS_ASSERT(res);
        res = m_sqlite->Bind(updateStmt, 2, static_cast<int>(KeyStatus::KEY_IN_USE));
        NS_ASSERT(res);
        res = m_sqlite->Bind(updateStmt, 3, keyParams.creationTime);
        NS_ASSERT(res);
        res = m_sqlite->Bind(updateStmt, 4, keyParams.expirationTime);
        NS_ASSERT(res);
        res = m_sqlite->Bind(updateStmt, 5, keyParams.size);
        NS_ASSERT(res);
        res = m_sqlite->Bind(updateStmt, 6, ksid);
        NS_ASSERT(res);
        res = m_sqlite->Bind(updateStmt, 7, srcStr);
        NS_ASSERT(res);
        res = m_sqlite->Bind(updateStmt, 8, dstStr);
        NS_ASSERT(res);

        // Execute the update statement transaction
        res = SQLiteOutput::SpinStep(updateStmt);
        NS_ASSERT(res);
        res = SQLiteOutput::SpinFinalize(updateStmt);
        NS_ASSERT(res == 0);
        m_sqlite->SpinExec("COMMIT");
    }
    NS_LOG_DEBUG("Key retrieved: " << key);
    HashingAlgorithm hasher;
    return hasher.HashData(key.substr(0, 16), SHA256); // Return the hashed key
}

uint32_t
QkdKeyManager::OpenConnect(uint32_t ksid, const Address& src, const Address& dst)
{
    NS_LOG_FUNCTION(this << ksid << src << dst);
    QKDDataCollector dc;
    dc.DescribeRun(ksid,
                   "",
                   src,
                   dst,
                   KeyStatus::KEY_NOT_IN_USE,
                   Simulator::Now(),
                   Seconds(120),
                   16);
    uint32_t ksidReturned = CreateKeySession(dc);

    AddIpToDeviceMapping(
        src,
        m_deviceMapping.begin()->first); // Assuming the first device is used for mapping

    // The below code is for reactive key generation... Should be removed later
    // when proactive key gen is done
    Ptr<QkdKeyManagementLayer> layer = GetKeyManagementLayer(src);
    if (layer == nullptr)
    {
        NS_LOG_ERROR("No Key Management Layer found for source address");
        return 0;
    }
    Simulator::ScheduleNow(&QkdKeyManagementLayer::ScheduleKeyGeneration, layer);
    return ksidReturned;
}

uint32_t
QkdKeyManager::OpenConnect(const Address& src, const Address& dst)
{
    NS_LOG_FUNCTION(this << src << dst);
    QKDDataCollector dc;
    dc.DescribeRun(0, "", src, dst, KeyStatus::KEY_NOT_IN_USE, Simulator::Now(), Seconds(120), 16);
    uint32_t ksid = CreateKeySession(dc);
    AddIpToDeviceMapping(
        src,
        m_deviceMapping.begin()->first); // Assuming the first device is used for mapping
    // The below code is for reactive key generation... Should be removed later
    // when proactive key gen is done
    Ptr<QkdKeyManagementLayer> layer = GetKeyManagementLayer(src);
    if (layer == nullptr)
    {
        NS_LOG_ERROR("No Key Management Layer found for source address");
        return 0;
    }
    Simulator::ScheduleNow(&QkdKeyManagementLayer::ScheduleKeyGeneration, layer);
    return ksid;
}

// TODO: Proactively fill the buffer with keys
// void
// QkdKeyManager::Start()
// {
//     for (auto it : m_deviceMapping)
//     {
//         it.second->Start();
//     }
// }

// void
// QkdKeyManager::Stop()
// {
//     for (auto it : m_deviceMapping)
//     {
//         it.second->Stop();
//     }
// }

QkdKeyManager::QKDOutputCallback::QKDOutputCallback(const Ptr<SQLiteOutput>& db, uint32_t ksid)
    : m_db(db),
      m_ksid(ksid)
{
    m_db->WaitExec("CREATE TABLE IF NOT EXISTS Singletons "
                   "( ksid integer, name text, variable text, value )");

    m_db->WaitPrepare(&m_insertSingletonStatement,
                      "INSERT INTO Singletons "
                      "(ksid, name, variable, value)"
                      "values (?, ?, ?, ?)");
    m_db->Bind(m_insertSingletonStatement, 1, m_ksid);
}

QkdKeyManager::QKDOutputCallback::~QKDOutputCallback()
{
    SQLiteOutput::SpinFinalize(m_insertSingletonStatement);
}

void
QkdKeyManager::QKDOutputCallback::OutputStatistic(std::string key,
                                                  std::string variable,
                                                  const StatisticalSummary* statSum)
{
    OutputSingleton(key, variable + "-count", static_cast<double>(statSum->getCount()));
    if (!std::isnan(statSum->getSum()))
    {
        OutputSingleton(key, variable + "-total", statSum->getSum());
    }
    if (!std::isnan(statSum->getMax()))
    {
        OutputSingleton(key, variable + "-max", statSum->getMax());
    }
    if (!std::isnan(statSum->getMin()))
    {
        OutputSingleton(key, variable + "-min", statSum->getMin());
    }
    if (!std::isnan(statSum->getSqrSum()))
    {
        OutputSingleton(key, variable + "-sqrsum", statSum->getSqrSum());
    }
    if (!std::isnan(statSum->getStddev()))
    {
        OutputSingleton(key, variable + "-stddev", statSum->getStddev());
    }
}

void
QkdKeyManager::QKDOutputCallback::OutputSingleton(std::string key, std::string variable, int val)
{
    SQLiteOutput::SpinReset(m_insertSingletonStatement);
    m_db->Bind(m_insertSingletonStatement, 2, key);
    m_db->Bind(m_insertSingletonStatement, 3, variable);
    m_db->Bind(m_insertSingletonStatement, 4, val);
    SQLiteOutput::SpinStep(m_insertSingletonStatement);
}

void
QkdKeyManager::QKDOutputCallback::OutputSingleton(std::string key,
                                                  std::string variable,
                                                  uint32_t val)
{
    SQLiteOutput::SpinReset(m_insertSingletonStatement);
    m_db->Bind(m_insertSingletonStatement, 2, key);
    m_db->Bind(m_insertSingletonStatement, 3, variable);
    m_db->Bind(m_insertSingletonStatement, 4, val);
    SQLiteOutput::SpinStep(m_insertSingletonStatement);
}

void
QkdKeyManager::QKDOutputCallback::OutputSingleton(std::string key, std::string variable, double val)
{
    SQLiteOutput::SpinReset(m_insertSingletonStatement);
    m_db->Bind(m_insertSingletonStatement, 2, key);
    m_db->Bind(m_insertSingletonStatement, 3, variable);
    m_db->Bind(m_insertSingletonStatement, 4, val);
    SQLiteOutput::SpinStep(m_insertSingletonStatement);
}

void
QkdKeyManager::QKDOutputCallback::OutputSingleton(std::string key,
                                                  std::string variable,
                                                  std::string val)
{
    SQLiteOutput::SpinReset(m_insertSingletonStatement);
    m_db->Bind(m_insertSingletonStatement, 2, key);
    m_db->Bind(m_insertSingletonStatement, 3, variable);
    m_db->Bind(m_insertSingletonStatement, 4, val);
    SQLiteOutput::SpinStep(m_insertSingletonStatement);
}

void
QkdKeyManager::QKDOutputCallback::OutputSingleton(std::string key, std::string variable, Time val)
{
    SQLiteOutput::SpinReset(m_insertSingletonStatement);
    m_db->Bind(m_insertSingletonStatement, 2, key);
    m_db->Bind(m_insertSingletonStatement, 3, variable);
    m_db->Bind(m_insertSingletonStatement, 4, val.GetTimeStep());
    SQLiteOutput::SpinStep(m_insertSingletonStatement);
}
} // namespace ns3
