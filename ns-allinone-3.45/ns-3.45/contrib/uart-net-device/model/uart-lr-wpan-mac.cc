/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

#include "uart-lr-wpan-mac.h"

#include "ns3/log.h"

#include <boost/asio.hpp>
#include <map>

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT

using namespace ns3::lrwpan;

namespace ns3
{
namespace uart
{

/**
 *  The IO context used to provide asynchronous serial communication. This context is used by
 *  the serial port instances.
 *  This is declared as a static variable in here to avoid "cppyyy undefined symbols" errors in
 *  the python bindings when using external libraries such as the present boost library.
 */
static boost::asio::io_context g_ioContext;

/**
 *  Map containing the serial port instances necessary to realize serial communication
 *  with hardware devices.
 *  This is declared as a static variable in here to avoid "cppyyy undefined symbols" errors in
 *  the python bindings when using external libraries such as the present boost library.
 */
static std::map<uint32_t, boost::asio::serial_port> g_serialPortInstances;

NS_LOG_COMPONENT_DEFINE("UartLrWpanMac");
NS_OBJECT_ENSURE_REGISTERED(UartLrWpanMac);

TypeId
UartLrWpanMac::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UartLrWpanMac")
                            .SetParent<LrWpanMacBase>()
                            .SetGroupName("UartLrWpanMac")
                            .AddConstructor<UartLrWpanMac>();
    return tid;
}

UartLrWpanMac::UartLrWpanMac()
{
    // Use the size of the map to assign instances IDs
    m_currentInstanceId = g_serialPortInstances.size();
    g_serialPortInstances.insert(
        {g_serialPortInstances.size(), boost::asio::serial_port(g_ioContext)});
}

UartLrWpanMac::UartLrWpanMac(const std::string& port)
    : m_port(port)
{
    NS_LOG_FUNCTION(this);

    m_currentInstanceId = g_serialPortInstances.size();
    g_serialPortInstances.insert(
        {g_serialPortInstances.size(), boost::asio::serial_port(g_ioContext)});

    m_rxState = RX_START;
    m_rxPrimitiveType = NONE_CFM;
    m_paramsMaxSize = 0;
    m_rxByteCount = 0;

    OpenPort();
    ReadByte();
    m_ioContextThread = std::thread([this] { RunIoContext(); });
}

void
UartLrWpanMac::DoInitialize()
{
    NS_LOG_FUNCTION(this);
}

UartLrWpanMac::~UartLrWpanMac()
{
    NS_LOG_FUNCTION(this);
}

void
UartLrWpanMac::DoDispose()
{
    NS_LOG_FUNCTION(this);

    g_serialPortInstances.at(m_currentInstanceId).close();
    if (!g_ioContext.stopped())
    {
        g_ioContext.stop();
    }

    m_ioContextThread.join();

    m_mcpsDataConfirmCallback = MakeNullCallback<void, McpsDataConfirmParams>();
    m_mcpsDataIndicationCallback = MakeNullCallback<void, McpsDataIndicationParams, Ptr<Packet>>();
    m_mlmeStartConfirmCallback = MakeNullCallback<void, MlmeStartConfirmParams>();
    m_mlmeBeaconNotifyIndicationCallback =
        MakeNullCallback<void, MlmeBeaconNotifyIndicationParams>();
    m_mlmeSyncLossIndicationCallback = MakeNullCallback<void, MlmeSyncLossIndicationParams>();
    m_mlmePollConfirmCallback = MakeNullCallback<void, MlmePollConfirmParams>();
    m_mlmeScanConfirmCallback = MakeNullCallback<void, MlmeScanConfirmParams>();
    m_mlmeAssociateConfirmCallback = MakeNullCallback<void, MlmeAssociateConfirmParams>();
    m_mlmeAssociateIndicationCallback = MakeNullCallback<void, MlmeAssociateIndicationParams>();
    m_mlmeCommStatusIndicationCallback = MakeNullCallback<void, MlmeCommStatusIndicationParams>();
    m_mlmeOrphanIndicationCallback = MakeNullCallback<void, MlmeOrphanIndicationParams>();
    m_mlmeSetConfirmCallback = MakeNullCallback<void, MlmeSetConfirmParams>();
    m_sensorIndicationCallback = MakeNullCallback<void, SensorIndicationParams>();
    Object::DoDispose();
}

void
UartLrWpanMac::McpsDataRequest(McpsDataRequestParams params, Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this);

    std::vector<uint8_t> dataBytes;

    // TODO: verify that packet Size is not more than 118 bytes(maximum possible size in JN5169)

    // Note: Alghouth JN5169 data frame can handle extra information, as described in the standard,
    // srcPANID and srcAddress is not included in the frame.

    Uint8ToBytes(dataBytes, 0xBB); // Begin of data 0xBB (MCPS primitive)
    Uint8ToBytes(dataBytes, 0);    // primitive type
    // Parameters size =  7 + variable bytes
    // Handle (1) + SrcAddrMode (1) + DstAddrMode (1) + DstPanId (2)
    // TxOption (1) + DataLength (1) + DstAddr (variable)
    // Payload (variable)
    uint8_t parametersSize = 7;

    if (params.m_dstAddrMode == lrwpan::AddressMode::EXT_ADDR)
    {
        parametersSize += 8;
    }
    else
    {
        parametersSize += 2;
    }
    parametersSize += p->GetSize();
    // Total size of all parameters (including data)
    Uint8ToBytes(dataBytes, parametersSize);

    Uint8ToBytes(dataBytes, params.m_msduHandle);
    Uint8ToBytes(dataBytes, params.m_srcAddrMode);
    Uint8ToBytes(dataBytes, params.m_dstAddrMode);
    Uint16ToBytes(dataBytes, params.m_dstPanId);
    if (params.m_dstAddrMode == EXT_ADDR)
    {
        Uint64ToBytes(dataBytes, params.m_dstExtAddr.ConvertToInt());
    }
    else
    {
        Uint16ToBytes(dataBytes, params.m_dstAddr.ConvertToInt());
    }
    Uint8ToBytes(dataBytes, params.m_txOptions);

    Uint8ToBytes(dataBytes, p->GetSize());
    auto bufferPtr = new uint8_t[p->GetSize()];
    p->CopyData(bufferPtr, p->GetSize());

    for (uint32_t i = 0; i < p->GetSize(); i++)
    {
        Uint8ToBytes(dataBytes, bufferPtr[i]);
    }
    delete[] bufferPtr;

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));
        // std::cout << "write without issues (DataRequest)\n";
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "problems while writing (DataRequest)\n";
    }
}

void
UartLrWpanMac::MlmeStartRequest(MlmeStartRequestParams params)
{
    NS_LOG_FUNCTION(this);

    std::vector<uint8_t> dataBytes;

    Uint8ToBytes(dataBytes, 0xAA); // Begin of data 0xAA
    Uint8ToBytes(dataBytes, 8);    // primitive type
    // Parameters size = 13 bytes
    // panId (2) + Channel (1) + Page (1) + startTime (4) +
    // beaconOrder (1) + superframeOrder (1) + panCoord (1) +
    // battLifeExt (1) + coordRealigment (1)
    Uint8ToBytes(dataBytes, 13);
    Uint16ToBytes(dataBytes, params.m_PanId);
    Uint8ToBytes(dataBytes, params.m_logCh);
    Uint8ToBytes(dataBytes, params.m_logChPage);
    Uint32ToBytes(dataBytes, params.m_startTime);
    Uint8ToBytes(dataBytes, params.m_bcnOrd);
    Uint8ToBytes(dataBytes, params.m_sfrmOrd);
    Uint8ToBytes(dataBytes, params.m_panCoor);
    Uint8ToBytes(dataBytes, params.m_battLifeExt);
    Uint8ToBytes(dataBytes, params.m_coorRealgn);

    /* for (uint8_t data : dataBytes)
     {
         std::cout << " 0x" << std::hex << (uint32_t)data << std::dec;
     }
     std::cout << "\n";*/

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));
        // std::cout << "write without issues (StartRequest)\n";
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "problems while writing (StartRequest)\n";
    }
}

void
UartLrWpanMac::MlmeScanRequest(MlmeScanRequestParams params)
{
    NS_LOG_FUNCTION(this);

    std::vector<uint8_t> dataBytes;

    Uint8ToBytes(dataBytes, 0xAA); // Begin of data 0xAA
    Uint8ToBytes(dataBytes, 6);    // primitive type
    // Parameters size = 6 bytes
    // ScanChannels (4) + ScanType (1) + ScanDuration (1)
    // Note: Security information is not included
    Uint8ToBytes(dataBytes, 6);
    Uint32ToBytes(dataBytes, params.m_scanChannels);
    Uint8ToBytes(dataBytes, params.m_scanType);
    Uint8ToBytes(dataBytes, params.m_scanDuration);

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));

        // std::cout << "write without issues (ScanRequest)\n";
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "problems while writing (ScanRequest)\n";
    }
}

void
UartLrWpanMac::MlmeAssociateRequest(MlmeAssociateRequestParams params)
{
    NS_LOG_FUNCTION(this);
    std::vector<uint8_t> dataBytes;

    Uint8ToBytes(dataBytes, 0xAA); // Begin of data 0xAA
    Uint8ToBytes(dataBytes, 0);    // primitive type

    // Parameters size = 14 or 8 bytes
    // Channel number (1) + Channel Page (1) + Address Mode (1)
    // PanId (2) + CapabilityInfo (1) + Coordinator address (2 or 8)
    // Note: Security information is not included
    if (params.m_coordAddrMode == EXT_ADDR)
    {
        Uint8ToBytes(dataBytes, 14);
    }
    else
    {
        Uint8ToBytes(dataBytes, 8);
    }

    Uint8ToBytes(dataBytes, params.m_chNum);
    Uint8ToBytes(dataBytes, params.m_chPage);
    Uint8ToBytes(dataBytes, params.m_coordAddrMode);
    Uint16ToBytes(dataBytes, params.m_coordPanId);
    Uint8ToBytes(dataBytes, params.m_capabilityInfo);

    if (params.m_coordAddrMode == EXT_ADDR)
    {
        Uint64ToBytes(dataBytes, params.m_coordExtAddr.ConvertToInt());
    }
    else
    {
        Uint16ToBytes(dataBytes, params.m_coordShortAddr.ConvertToInt());
    }

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));
        // std::cout << "write without issues (AssociationRequest)\n";
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "problems while writing (AssociationRequest)\n";
    }
}

void
UartLrWpanMac::MlmeAssociateResponse(MlmeAssociateResponseParams params)
{
    NS_LOG_FUNCTION(this);

    std::vector<uint8_t> dataBytes;

    Uint8ToBytes(dataBytes, 0xAA); // Begin of data 0xAA
    Uint8ToBytes(dataBytes, 11);   // primitive type
    // Parameters size = 11 bytes
    // ExtDevAddr (8) + AssocShortAddr (2) + Status (1)
    Uint8ToBytes(dataBytes, 11);

    Uint64ToBytes(dataBytes, params.m_extDevAddr.ConvertToInt());
    Uint16ToBytes(dataBytes, params.m_assocShortAddr.ConvertToInt());
    Uint8ToBytes(dataBytes, static_cast<uint8_t>(params.m_status));

    /* for (uint8_t data : dataBytes)
     {
         std::cout << " 0x" << std::hex << (uint32_t)data << std::dec;
     }
     std::cout << "\n";*/

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));
        // std::cout << "write without issues (AssociationResponse)\n";
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "problems while writing (AssociationResponse)\n";
    }
}

void
UartLrWpanMac::MlmeOrphanResponse(MlmeOrphanResponseParams params)
{
    NS_LOG_FUNCTION(this);
    std::vector<uint8_t> dataBytes;

    Uint8ToBytes(dataBytes, 0xAA); // Begin of data 0xAA
    Uint8ToBytes(dataBytes, 12);   // primitive type
    // Size :Orphan Address (8) + ShortAddress (2) + AssociatedMember (1) = 11 bytes
    Uint8ToBytes(dataBytes, 11);
    Uint64ToBytes(dataBytes, params.m_orphanAddr.ConvertToInt());
    Uint16ToBytes(dataBytes, params.m_shortAddr.ConvertToInt());
    Uint8ToBytes(dataBytes, params.m_assocMember);

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));
        // std::cout << "write without issues (AssociationResponse)\n";
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "problems while writing (OrphanResponse)\n";
    }
}

void
UartLrWpanMac::MlmeSyncRequest(MlmeSyncRequestParams params)
{
    NS_LOG_FUNCTION(this);
    // TODO
    NS_ABORT_MSG("Primitive not supported");
}

void
UartLrWpanMac::MlmePollRequest(MlmePollRequestParams params)
{
    NS_LOG_FUNCTION(this);
    // TODO
    NS_ABORT_MSG("Primitive not supported");
}

void
UartLrWpanMac::MlmeSetRequest(MacPibAttributeIdentifier id, Ptr<MacPibAttributes> attribute)
{
    NS_LOG_FUNCTION(this);
    std::vector<uint8_t> dataBytes;

    Uint8ToBytes(dataBytes, 0xAA); // Begin of data 0xAA
    Uint8ToBytes(dataBytes, 7);    // primitive type
    // Parameters size = 1 + variable
    // attribute id(1) + parameter size (variable)
    switch (id)
    {
    case MacPibAttributeIdentifier::pCurrentChannel:
        Uint8ToBytes(dataBytes, 2); // Size: id(1) + phyCurrentChannel (1)
        Uint8ToBytes(dataBytes, MacPibAttributeIdentifier::pCurrentChannel);
        Uint8ToBytes(dataBytes, attribute->pCurrentChannel);
        break;
    case MacPibAttributeIdentifier::pCurrentPage:
        Uint8ToBytes(dataBytes, 2); // Size: id(1) + phyCurrentPage (1)
        Uint8ToBytes(dataBytes, MacPibAttributeIdentifier::pCurrentPage);
        Uint8ToBytes(dataBytes, attribute->pCurrentPage);
        break;
    case MacPibAttributeIdentifier::macPanId:
        Uint8ToBytes(dataBytes, 3); // Size: id(1) + macPanId (2)
        Uint8ToBytes(dataBytes, MacPibAttributeIdentifier::macPanId);
        Uint16ToBytes(dataBytes, attribute->macPanId);
        break;
    case MacPibAttributeIdentifier::macShortAddress:
        Uint8ToBytes(dataBytes, 3); // Size: id(1) + macShortAddr (2)
        Uint8ToBytes(dataBytes, MacPibAttributeIdentifier::macShortAddress);
        Uint16ToBytes(dataBytes, attribute->macShortAddress.ConvertToInt());
        break;
    case MacPibAttributeIdentifier::macBeaconPayloadLength:
        Uint8ToBytes(dataBytes, 2); // Size: id(1) + beaconPayloadLength (1)
        Uint8ToBytes(dataBytes, MacPibAttributeIdentifier::macBeaconPayloadLength);
        Uint8ToBytes(dataBytes, attribute->macBeaconPayloadLength);
        break;
    case MacPibAttributeIdentifier::macBeaconPayload: {
        // Size: id(1) + beaconPayload (variable)
        Uint8ToBytes(dataBytes, attribute->macBeaconPayload.size() + 1);
        Uint8ToBytes(dataBytes, MacPibAttributeIdentifier::macBeaconPayload);
        for (auto element : attribute->macBeaconPayload)
        {
            dataBytes.emplace_back(element);
        }
        break;
    }
    default:
        NS_ABORT_MSG("Attribute not supported");
        break;
    }

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));
        // std::cout << "write without issues (SetRequest)\n";
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "problems while writing (SetRequest)\n";
    }
}

void
UartLrWpanMac::MlmeGetRequest(MacPibAttributeIdentifier id)
{
    NS_LOG_FUNCTION(this);
    std::vector<uint8_t> dataBytes;

    Uint8ToBytes(dataBytes, 0xAA); // Begin of data 0xAA
    Uint8ToBytes(dataBytes, 2);    // primitive type
    Uint8ToBytes(dataBytes, 1);    // parameter bytes
    Uint8ToBytes(dataBytes, static_cast<uint8_t>(id));

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));
        // std::cout << "write without issues (GetRequest)\n";
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "problems while writing (GetRequest)\n";
    }
}

void
UartLrWpanMac::SensorRequest(SensorRequestParams params)
{
    NS_LOG_FUNCTION(this);
    std::vector<uint8_t> dataBytes;

    Uint8ToBytes(dataBytes, 0xAA); // Begin of data 0xAA
    Uint8ToBytes(dataBytes, 99);   // primitive type
    Uint8ToBytes(dataBytes, 1);    // parameter bytes
    Uint8ToBytes(dataBytes, static_cast<uint8_t>(params.m_type));

    try
    {
        boost::asio::write(g_serialPortInstances.at(m_currentInstanceId),
                           boost::asio::buffer(dataBytes));
    }
    catch (const boost::system::system_error& e)
    {
        std::cout << "problems while trying to get sensor data\n";
    }
}

void
UartLrWpanMac::SetSensorIndicationCallback(SensorIndicationCallback c)
{
    m_sensorIndicationCallback = c;
}

void
UartLrWpanMac::Uint8ToBytes(std::vector<uint8_t>& dataArray, uint8_t intValue)
{
    dataArray.emplace_back(intValue);
}

void
UartLrWpanMac::Uint16ToBytes(std::vector<uint8_t>& dataArray, uint16_t intValue)
{
    dataArray.emplace_back(static_cast<uint8_t>(intValue & 0xFF));
    dataArray.emplace_back(static_cast<uint8_t>((intValue >> 8) & 0xFF));
}

void
UartLrWpanMac::Uint32ToBytes(std::vector<uint8_t>& dataArray, uint32_t intValue)
{
    dataArray.emplace_back(static_cast<uint8_t>(intValue & 0xFF));
    dataArray.emplace_back(static_cast<uint8_t>((intValue >> 8) & 0xFF));
    dataArray.emplace_back(static_cast<uint8_t>((intValue >> 16) & 0xFF));
    dataArray.emplace_back(static_cast<uint8_t>((intValue >> 24) & 0xFF));
}

void
UartLrWpanMac::Uint64ToBytes(std::vector<uint8_t>& dataArray, uint64_t intValue)
{
    for (int i = 0; i < 8; ++i)
    {
        dataArray.emplace_back(static_cast<uint8_t>((intValue >> (i * 8)) & 0xFF));
    }
}

uint8_t
UartLrWpanMac::BytesToUint8(const std::vector<uint8_t>& dataArray, uint8_t& pos)
{
    return dataArray[pos++];
}

uint16_t
UartLrWpanMac::BytesToUint16(const std::vector<uint8_t>& dataArray, uint8_t& pos)
{
    uint16_t value;
    value = (dataArray[pos++] & 0xFF);
    value |= (dataArray[pos++] << 8);
    return value;
}

uint32_t
UartLrWpanMac::BytesToUint32(const std::vector<uint8_t>& dataArray, uint8_t& pos)
{
    uint32_t value;
    value = (dataArray[pos++] & 0xFF);
    value |= (dataArray[pos++] << 8);
    value |= (dataArray[pos++] << 16);
    value |= (dataArray[pos++] << 24);
    return value;
}

uint64_t
UartLrWpanMac::BytesToUint64(const std::vector<uint8_t>& dataArray, uint8_t& pos)
{
    uint64_t value;
    value = (static_cast<uint64_t>(dataArray[pos++]) & 0xFF);
    value |= (static_cast<uint64_t>(dataArray[pos++]) << 8);
    value |= (static_cast<uint64_t>(dataArray[pos++]) << 16);
    value |= (static_cast<uint64_t>(dataArray[pos++]) << 24);

    value |= (static_cast<uint64_t>(dataArray[pos++]) << 32);
    value |= (static_cast<uint64_t>(dataArray[pos++]) << 40);
    value |= (static_cast<uint64_t>(dataArray[pos++]) << 48);
    value |= (static_cast<uint64_t>(dataArray[pos++]) << 56);

    return value;
}

void
UartLrWpanMac::OpenPort()
{
    try
    {
        g_serialPortInstances.at(m_currentInstanceId).open(m_port);
        g_serialPortInstances.at(m_currentInstanceId)
            .set_option(boost::asio::serial_port_base::baud_rate(115200));
        g_serialPortInstances.at(m_currentInstanceId)
            .set_option(boost::asio::serial_port_base::character_size(8));
        g_serialPortInstances.at(m_currentInstanceId)
            .set_option(boost::asio::serial_port_base::stop_bits(
                boost::asio::serial_port_base::stop_bits::one));
        g_serialPortInstances.at(m_currentInstanceId)
            .set_option(
                boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));

        NS_LOG_DEBUG("Connected to port: " << m_port);
    }
    catch (const boost::system::system_error& e)
    {
        // NS_LOG_ABORT("Serial communication error: " << e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        std::cout << "port connection failed\n";
    }
}

void
UartLrWpanMac::ReadByte()
{
    try
    {
        auto rxByte = std::make_shared<uint8_t>();

        async_read(g_serialPortInstances.at(m_currentInstanceId),
                   boost::asio::buffer(rxByte.get(), 1),
                   [this, rxByte](const boost::system::error_code& errorCode,
                                  std::size_t bytes_transferred) {
                       if (!errorCode)
                       {
                           {
                               // Process the received byte (read_byte).
                               std::lock_guard<std::mutex> lock(m_mutex);

                               // A complete received primitive instruction requires these bytes:
                               // Initial byte 0xAA (1byte)| primitive type (1 byte) |
                               // primitive parameter size (1 byte) | parameters bytes (variable)
                               switch (m_rxState)
                               {
                               case RX_START:
                                   if ((*rxByte) == 0xAA)
                                   {
                                       m_rxState = RX_PRIMITIVE_TYPE;
                                   }
                                   else
                                   { // No instruction received, print character (Use for debugging)
                                       std::cout << (*rxByte);
                                       // uint8_t byte = (*rxByte.get());
                                       // std::cout << std::hex << "0x" <<
                                       // static_cast<uint32_t>(byte)
                                       //           << std::dec << "\n";
                                   }
                                   break;
                               case RX_PRIMITIVE_TYPE:
                                   m_rxPrimitiveType = static_cast<PrimitiveType>(*rxByte);
                                   m_rxState = RX_PARAMS_SIZE;
                                   break;
                               case RX_PARAMS_SIZE:
                                   m_paramsMaxSize = (*rxByte);
                                   m_rxState = RX_WAIT_DATA;
                                   break;
                               case RX_WAIT_DATA:
                                   m_rxData.emplace_back(*rxByte);
                                   m_rxByteCount++;
                                   if (m_rxByteCount == m_paramsMaxSize)
                                   {
                                       // Schedule an event with the current NodeId context.
                                       // This is necessary to update the Simulator event time after
                                       // receiving an external input (In this case, the information
                                       // received via UART from the MCU)
                                       Simulator::ScheduleWithContext(
                                           m_nodeId,
                                           Time(0),
                                           MakeEvent(&UartLrWpanMac::ProcessData, this));
                                       m_rxState = RX_START;
                                       m_rxData.clear();
                                       m_rxByteCount = 0;
                                   }
                                   break;
                               }
                           }

                           ReadByte();
                       }
                       else
                       {
                           // Only print an error if is not a graceful exit
                           if (errorCode.value() != 125)
                           {
                               std::cerr << "Error in async_read: " << errorCode.message()
                                         << std::endl;
                           }

                           // Handle error appropriately, e.g., stop reading or close the port
                       }
                   });
    }
    catch (const boost::system::system_error& error)
    {
        //   NS_LOG_ABORT("Serial communication error: " << e.what());
    }
}

void
UartLrWpanMac::ProcessData()
{
    switch (m_rxPrimitiveType)
    {
    case SCAN_CFM:
        ScanConfirm();
        break;
    case START_CFM:
        StartConfirm();
        break;
    case ASSOCIATE_CFM:
        AssociateConfirm();
        break;
    case ASSOCIATE_IND:
        AssociateIndication();
        break;
    case COMM_STATUS_IND:
        CommStatusIndication();
        break;
    case DATA_CFM:
        DataConfirm();
        break;
    case DATA_IND:
        DataIndication();
        break;
    case SET_CFM:
        SetConfirm();
        break;
    case GET_CFM:
        GetConfirm();
        break;
    case ORPHAN_IND:
        OrphanIndication();
        break;
    case BEACON_NOTIFY_IND:
        BeaconNotifyIndication();
        break;
    case SENSOR_IND:
        SensorIndication();
        break;
    default:
        std::cout << "Unknown Primitive with code " << static_cast<uint32_t>(m_rxPrimitiveType)
                  << " received \n";
        NS_LOG_ERROR("Cannot process data, primitive with code "
                     << static_cast<uint32_t>(m_rxPrimitiveType) << " does not exist");
        break;
    }
}

void
UartLrWpanMac::ScanConfirm()
{
    uint8_t pos = 0;
    MlmeScanConfirmParams params;

    params.m_status = static_cast<MacStatus>(BytesToUint8(m_rxData, pos));
    params.m_scanType = static_cast<MlmeScanType>(BytesToUint8(m_rxData, pos));
    BytesToUint32(m_rxData, pos); // TODO params.m_unscannedCh = BytesToUint32(m_rxData, pos);
    params.m_resultListSize = BytesToUint8(m_rxData, pos);
    if (params.m_status == MacStatus::SUCCESS)
    {
        if (params.m_scanType == MLMESCAN_ED)
        {
            for (int i = 0; i < params.m_resultListSize; i++)
            {
                params.m_energyDetList.emplace_back(BytesToUint8(m_rxData, pos));
            }
        }
        else if (params.m_scanType == MLMESCAN_ORPHAN)
        {
        }
        else
        {
            // Active or Passive scans

            for (int i = 0; i < params.m_resultListSize; i++)
            {
                PanDescriptor panDescriptor;
                panDescriptor.m_coorAddrMode =
                    static_cast<lrwpan::AddressMode>(BytesToUint8(m_rxData, pos));

                panDescriptor.m_coorPanId = BytesToUint16(m_rxData, pos);
                if (panDescriptor.m_coorAddrMode == SHORT_ADDR)
                {
                    panDescriptor.m_coorShortAddr = Mac16Address(BytesToUint16(m_rxData, pos));
                }
                else
                {
                    panDescriptor.m_coorExtAddr = Mac64Address(BytesToUint64(m_rxData, pos));
                }

                panDescriptor.m_logCh = BytesToUint8(m_rxData, pos);
                panDescriptor.m_logChPage = BytesToUint8(m_rxData, pos);
                panDescriptor.m_superframeSpec = BytesToUint16(m_rxData, pos);
                panDescriptor.m_gtsPermit = BytesToUint8(m_rxData, pos);
                panDescriptor.m_linkQuality = BytesToUint8(m_rxData, pos);
                // panDescriptor.m_timeStamp
                BytesToUint32(m_rxData, pos);

                params.m_panDescList.emplace_back(panDescriptor);
            }
        }
    }

    if (!m_mlmeScanConfirmCallback.IsNull())
    {
        m_mlmeScanConfirmCallback(params);
    }
}

void
UartLrWpanMac::StartConfirm()
{
    if (!m_mlmeStartConfirmCallback.IsNull())
    {
        uint8_t pos = 0;
        MlmeStartConfirmParams params;
        params.m_status = static_cast<MacStatus>(BytesToUint8(m_rxData, pos));
        m_mlmeStartConfirmCallback(params);
    }
}

void
UartLrWpanMac::AssociateIndication()
{
    if (!m_mlmeAssociateIndicationCallback.IsNull())
    {
        uint8_t pos = 0;
        MlmeAssociateIndicationParams params;
        params.m_extDevAddr = Mac64Address(BytesToUint64(m_rxData, pos));
        params.capabilityInfo = BytesToUint8(m_rxData, pos);
        m_mlmeAssociateIndicationCallback(params);
    }
}

void
UartLrWpanMac::CommStatusIndication()
{
    if (!m_mlmeCommStatusIndicationCallback.IsNull())
    {
        uint8_t pos = 0;
        MlmeCommStatusIndicationParams params;

        params.m_panId = BytesToUint16(m_rxData, pos);
        params.m_srcAddrMode = BytesToUint8(m_rxData, pos);
        if (params.m_srcAddrMode == EXT_ADDR)
        {
            params.m_srcExtAddr = BytesToUint64(m_rxData, pos);
        }
        else
        {
            params.m_srcShortAddr = BytesToUint16(m_rxData, pos);
        }

        params.m_dstAddrMode = BytesToUint8(m_rxData, pos);

        if (params.m_dstAddrMode == EXT_ADDR)
        {
            params.m_dstExtAddr = BytesToUint64(m_rxData, pos);
        }
        else
        {
            params.m_dstShortAddr = BytesToUint16(m_rxData, pos);
        }

        params.m_status = static_cast<MacStatus>(BytesToUint8(m_rxData, pos));

        m_mlmeCommStatusIndicationCallback(params);
    }
}

void
UartLrWpanMac::AssociateConfirm()
{
    if (!m_mlmeAssociateConfirmCallback.IsNull())
    {
        uint8_t pos = 0;
        MlmeAssociateConfirmParams params;
        params.m_status = static_cast<MacStatus>(BytesToUint8(m_rxData, pos));
        params.m_assocShortAddr = BytesToUint16(m_rxData, pos);
        m_mlmeAssociateConfirmCallback(params);
    }
}

void
UartLrWpanMac::DataConfirm()
{
    if (!m_mcpsDataConfirmCallback.IsNull())
    {
        uint8_t pos = 0;
        McpsDataConfirmParams params;
        params.m_status = static_cast<MacStatus>(BytesToUint8(m_rxData, pos));
        params.m_msduHandle = BytesToUint8(m_rxData, pos);
        // TODO: Add timestamp when available
        m_mcpsDataConfirmCallback(params);
    }
}

void
UartLrWpanMac::DataIndication()
{
    if (!m_mcpsDataIndicationCallback.IsNull())
    {
        uint8_t pos = 0;
        McpsDataIndicationParams params;

        params.m_srcAddrMode = BytesToUint8(m_rxData, pos);
        params.m_srcPanId = BytesToUint16(m_rxData, pos);
        if (params.m_srcAddrMode == EXT_ADDR)
        {
            params.m_srcExtAddr = BytesToUint64(m_rxData, pos);
        }
        else
        {
            params.m_srcAddr = BytesToUint16(m_rxData, pos);
        }
        params.m_dstAddrMode = BytesToUint8(m_rxData, pos);
        params.m_dstPanId = BytesToUint16(m_rxData, pos);
        if (params.m_dstAddrMode == EXT_ADDR)
        {
            params.m_dstExtAddr = BytesToUint64(m_rxData, pos);
        }
        else
        {
            params.m_dstAddr = BytesToUint16(m_rxData, pos);
        }
        params.m_mpduLinkQuality = BytesToUint8(m_rxData, pos);
        params.m_dsn = BytesToUint8(m_rxData, pos);

        // TODO Add timeStamp and msduLength when supported,
        // for now we just use msdu length here but we do not
        // push it up the stack

        uint32_t msduLength = BytesToUint8(m_rxData, pos);
        auto bufferPtr = new uint8_t[msduLength];

        for (uint32_t i = 0; i < msduLength; i++)
        {
            bufferPtr[i] = BytesToUint8(m_rxData, pos);
        }
        Ptr<Packet> packet = Create<Packet>(bufferPtr, msduLength);

        m_mcpsDataIndicationCallback(params, packet);
    }
}

void
UartLrWpanMac::SetConfirm()
{
    if (!m_mlmeSetConfirmCallback.IsNull())
    {
        uint8_t pos = 0;
        MlmeSetConfirmParams params;

        params.id = static_cast<MacPibAttributeIdentifier>(BytesToUint8(m_rxData, pos));
        params.m_status = static_cast<MacStatus>(BytesToUint8(m_rxData, pos));
        m_mlmeSetConfirmCallback(params);
    }
}

void
UartLrWpanMac::GetConfirm()
{
    if (!m_mlmeGetConfirmCallback.IsNull())
    {
        uint8_t pos = 0;
        auto status = static_cast<MacStatus>(BytesToUint8(m_rxData, pos));
        auto id = static_cast<MacPibAttributeIdentifier>(BytesToUint8(m_rxData, pos));
        Ptr<MacPibAttributes> pibAttr = Create<MacPibAttributes>();

        switch (id)
        {
        case pCurrentChannel:
            pibAttr->pCurrentChannel = BytesToUint8(m_rxData, pos);
            break;
        case pCurrentPage:
            pibAttr->pCurrentPage = BytesToUint8(m_rxData, pos);
            break;
        case macExtendedAddress:
            pibAttr->macExtendedAddress = BytesToUint64(m_rxData, pos);
            break;
        case macBeaconPayload: {
            for (uint32_t i = pos; i < m_rxData.size(); i++)
            {
                pibAttr->macBeaconPayload.emplace_back(m_rxData[i]);
            }
            break;
        }
        case macBeaconPayloadLength:
            pibAttr->macBeaconPayloadLength = BytesToUint8(m_rxData, pos);
            break;
        case macPanId:
            pibAttr->macPanId = BytesToUint16(m_rxData, pos);
            break;
        case macShortAddress:
            pibAttr->macShortAddress = BytesToUint16(m_rxData, pos);
            break;
        default:
            NS_ABORT_MSG("Attribute not supported in MLME-GET.request");
            break;
        }

        m_mlmeGetConfirmCallback(status, id, pibAttr);
    }
}

void
UartLrWpanMac::OrphanIndication()
{
    if (!m_mlmeOrphanIndicationCallback.IsNull())
    {
        MlmeOrphanIndicationParams params;
        uint8_t pos = 0;
        params.m_orphanAddr = BytesToUint64(m_rxData, pos);
        // TODO: Add security parameters when supported
        m_mlmeOrphanIndicationCallback(params);
    }
}

void
UartLrWpanMac::BeaconNotifyIndication()
{
    if (!m_mlmeBeaconNotifyIndicationCallback.IsNull())
    {
        MlmeBeaconNotifyIndicationParams params;
        uint8_t pos = 0;

        params.m_bsn = BytesToUint8(m_rxData, pos);
        params.m_panDescriptor.m_coorAddrMode =
            static_cast<lrwpan::AddressMode>(BytesToUint8(m_rxData, pos));
        params.m_panDescriptor.m_coorPanId = BytesToUint16(m_rxData, pos);
        if (params.m_panDescriptor.m_coorAddrMode == lrwpan::AddressMode::EXT_ADDR)
        {
            params.m_panDescriptor.m_coorExtAddr = Mac64Address(BytesToUint64(m_rxData, pos));
        }
        else
        {
            params.m_panDescriptor.m_coorShortAddr = Mac16Address(BytesToUint16(m_rxData, pos));
        }
        params.m_panDescriptor.m_logCh = BytesToUint8(m_rxData, pos);
        params.m_panDescriptor.m_logChPage = BytesToUint8(m_rxData, pos);
        params.m_panDescriptor.m_superframeSpec = BytesToUint16(m_rxData, pos);
        params.m_panDescriptor.m_gtsPermit = BytesToUint8(m_rxData, pos);
        params.m_panDescriptor.m_linkQuality = BytesToUint8(m_rxData, pos);
        // TODO:params.m_panDescriptor.m_timeStamp // not supported
        BytesToUint32(m_rxData, pos);
        // TODO: Add pan descriptor security parameters here when supported

        // TODO: Pending address specification field and pending address list missing,
        //       add to beacon notify params when available
        uint8_t pendingAddrSpecField = BytesToUint8(m_rxData, pos);
        uint8_t numPendingShortAddr = pendingAddrSpecField & (0x07);    // bits 0-2
        uint8_t numPendingExtAddr = (pendingAddrSpecField & 0x70) >> 4; // bits 4-6;
        for (uint8_t i = 0; i < numPendingShortAddr; i++)
        {
            BytesToUint16(m_rxData, pos);
        }

        for (uint8_t i = 0; i < numPendingExtAddr; i++)
        {
            BytesToUint64(m_rxData, pos);
        }

        params.m_sduLength = BytesToUint8(m_rxData, pos);
        std::vector<uint8_t> sdu;
        sdu.resize(params.m_sduLength);
        for (uint32_t z = 0; z < params.m_sduLength; z++)
        {
            sdu.emplace_back(BytesToUint8(m_rxData, pos));
        }
        params.m_sdu = Create<Packet>(sdu.data(), sdu.size());

        m_mlmeBeaconNotifyIndicationCallback(params);
    }
}

void
UartLrWpanMac::SensorIndication()
{
    SensorIndicationParams params;
    uint8_t pos = 0;

    params.m_type = static_cast<SensorType>(BytesToUint8(m_rxData, pos));
    params.m_status = static_cast<MacStatus>(BytesToUint8(m_rxData, pos));
    switch (params.m_type)
    {
    case SensorType::TEMPERATURE_HUMIDITY: {
        if (params.m_status == MacStatus::SUCCESS)
        {
            params.m_temperature = BytesToUint32(m_rxData, pos);
            params.m_humidity = BytesToUint32(m_rxData, pos);
        }
        else
        {
            params.m_temperature = -99;
            params.m_humidity = -99;
        }
        break;
    }
    default:
        NS_LOG_DEBUG("Unknown sensor indication ");
        break;
    }

    if (!m_sensorIndicationCallback.IsNull())
    {
        m_sensorIndicationCallback(params);
    }
}

void
UartLrWpanMac::RunIoContext()
{
    g_ioContext.run();
}

void
UartLrWpanMac::SetNodeId(uint32_t nodeId)
{
    // Save the node ID for use in ProcessData.
    // This is necessary to update the current context after
    // receiving data from an external source (The data received
    // via UART from the MCU)
    m_nodeId = nodeId;
}

} // namespace uart
} // namespace ns3
