/*
 * Copyright (c) 2024 Tokushima University, Japan
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

#ifndef UART_LRWPAN_MAC_H
#define UART_LRWPAN_MAC_H

#include "ns3/lr-wpan-mac-base.h"
#include "ns3/simulator.h"
#include "ns3/traced-callback.h"
#include "ns3/traced-value.h"

#include <iostream>
#include <thread>
#include <vector>

namespace ns3
{

class Packet;

namespace uart
{

enum ReadState
{
    RX_START = 0,
    RX_PRIMITIVE_TYPE = 1,
    RX_PARAMS_SIZE = 2,
    RX_WAIT_DATA = 3
};

enum PrimitiveType
{
    NONE_CFM = 0,
    SCAN_CFM = 1,
    START_CFM = 2,
    ASSOCIATE_CFM = 3,
    ASSOCIATE_IND = 4,
    COMM_STATUS_IND = 5,
    DATA_CFM = 6,
    DATA_IND = 7,
    SET_CFM = 8,
    GET_CFM = 9,
    ORPHAN_IND = 10,
    BEACON_NOTIFY_IND = 11,
    SENSOR_IND = 20
};

/**
 * @ingroup lr-wpan
 *
 * Indicates the sensor type of the sensor request.
 */
enum class SensorType : std::uint8_t
{
    TEMPERATURE_HUMIDITY = 0x00
};

/**
 *@ingroup lr-wpan
 *
 * SENSOR.request params (Non-standard primitive parameters)
 */
struct SensorRequestParams
{
    SensorType m_type{SensorType::TEMPERATURE_HUMIDITY}; //!< Sensor type to request
};

/**
 * @ingroup lr-wpan
 *
 * This structure contains the parameters of the SensorIndicationCallback.
 * It is used to return the sensor data after a SensorRequest has been called.
 */
struct SensorIndicationParams
{
    lrwpan::MacStatus m_status{
        lrwpan::MacStatus::UNSUPPORTED_ATTRIBUTE};       //!< Status of the sensor data request
    SensorType m_type{SensorType::TEMPERATURE_HUMIDITY}; //!< Sensor type
    int32_t m_temperature{-99}; //!< Temperature in Celsius degrees, -99 if not available
    int32_t m_humidity{-99};    //!< Humidity in percentage (0-100), -99 if not available
};

/**
 * @ingroup lr-wpan
 *
 * This callback is called after a SensorRequest has been called from
 * the higher layer to set a PIB. It returns a status of the outcome and the
 * sensor data. Note: Sensor request or indication are not standard
 * primitives of IEEE 802.15.4.
 */
using SensorIndicationCallback = Callback<void, SensorIndicationParams>;

/**
 *  Implements the ns-3 lr-wpan class that communicates with the shim layer of a
 *  NXP JN516x device.
 */
class UartLrWpanMac : public lrwpan::LrWpanMacBase
{
  public:
    /**
     * Get the type ID.
     *
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * Default constructor.
     */
    UartLrWpanMac();
    /**
     * The constructor with port device parameter used to establish a serial connection with
     * a NXP JN516x device.
     *
     * @param port The physical port identifier of the device to which this class instance will
     * connect.
     */
    UartLrWpanMac(const std::string& port);
    ~UartLrWpanMac() override;

    // Inherited from LrWpanMacBase
    void McpsDataRequest(lrwpan::McpsDataRequestParams params, Ptr<Packet> p) override;

    void MlmeStartRequest(lrwpan::MlmeStartRequestParams params) override;

    void MlmeScanRequest(lrwpan::MlmeScanRequestParams params) override;

    void MlmeAssociateRequest(lrwpan::MlmeAssociateRequestParams params) override;

    void MlmeAssociateResponse(lrwpan::MlmeAssociateResponseParams params) override;

    void MlmeOrphanResponse(lrwpan::MlmeOrphanResponseParams params) override;

    void MlmeSyncRequest(lrwpan::MlmeSyncRequestParams params) override;

    void MlmePollRequest(lrwpan::MlmePollRequestParams params) override;

    void MlmeSetRequest(lrwpan::MacPibAttributeIdentifier id,
                        Ptr<lrwpan::MacPibAttributes> attribute) override;

    void MlmeGetRequest(lrwpan::MacPibAttributeIdentifier id) override;

    /**
     * Set the nodeId associated to the Netdevice of this MAC.
     * This is used to provide context to the MAC layer
     *
     * @param nodeId The node identifier
     */
    void SetNodeId(uint32_t nodeId);

    //////////////////////////
    //  Optional primitives //
    //////////////////////////

    /**
     * Obtain the data of a sensor contained in the device.
     * This primitive is not compliant with IEEE 802.15.4 and
     * is meant to be use only with Twelite CUE and Twelite ARIA
     * (NXP JN5169) devices.
     *
     * @param params The Mlme sensor request parameters
     */
    void SensorRequest(SensorRequestParams params);

    /**
     * Set the callback for the indication of sensor data request.
     * Sensor
     *
     * @param c the callback
     */
    void SetSensorIndicationCallback(SensorIndicationCallback c);

  protected:
    /**
     * This callback is used to report sensor data request to the
     * upper layers.
     * This callback belongs to a non-standard primitive intended for
     * JN5169 devices that has sensors (e.g. Twelite CUE and Twelite ARIA).
     */
    SensorIndicationCallback m_sensorIndicationCallback;

    // Inherited from Object.
    void DoInitialize() override;
    void DoDispose() override;

  private:
    /**
     *  Helper function to add an uint8_t value into a uint8_t list.
     *
     * @param dataArray The list of uint8_t elements
     * @param intValue  The uint8_t element to be added to the list.
     */
    void Uint8ToBytes(std::vector<uint8_t>& dataArray, uint8_t intValue);

    /**
     *  Helper function to break and add an uint16_t value into a uint8_t list.
     *
     * @param dataArray The list of uint8_t elements
     * @param intValue  The uint16_t element to be broken and added to the list.
     */
    void Uint16ToBytes(std::vector<uint8_t>& dataArray, uint16_t intValue);

    /**
     *  Helper function to break and add a uint32_t value into a uint8_t list.
     *
     * @param dataArray The list of uint8_t elements
     * @param intValue  The uint32_t element to be broken and added to the list.
     */
    void Uint32ToBytes(std::vector<uint8_t>& dataArray, uint32_t intValue);

    /**
     *  Helper function to break and add a uint64_t value into a uint8_t list.
     *
     * @param dataArray The list of uint8_t elements
     * @param intValue  The uint64_t element to be broken and added to the list.
     */
    void Uint64ToBytes(std::vector<uint8_t>& dataArray, uint64_t intValue);

    /**
     * Helper function to extract a uint8_t value (1 byte) from a list of uint8_t elements.
     *
     * @param dataArray The list from which the value is extracted.
     * @param pos The position in the list after the extraction of the element.
     * @return The extracted uint8_t value (1 byte).
     */
    uint8_t BytesToUint8(const std::vector<uint8_t>& dataArray, uint8_t& pos);

    /**
     * Helper function to extract a uint16_t value (2 bytes) from a list of uint8_t elements.
     *
     * @param dataArray The list from which the value is extracted.
     * @param pos The position in the list after the extraction of the element.
     * @return The extracted uint16_t value (2 bytes).
     */
    uint16_t BytesToUint16(const std::vector<uint8_t>& dataArray, uint8_t& pos);

    /**
     * Helper function to extract a uint32_t value (4 bytes) from a list of uint8_t elements.
     *
     * @param dataArray The list from which the value is extracted.
     * @param pos The position in the list after the extraction of the element.
     * @return The extracted uint32_t value (4 bytes).
     */
    uint32_t BytesToUint32(const std::vector<uint8_t>& dataArray, uint8_t& pos);

    /**
     * Helper function to extract a uint64_t value (8 bytes) from a list of uint8_t elements.
     *
     * @param dataArray The list from which the value is extracted.
     * @param pos The position in the list after the extraction of the element.
     * @return The extracted uint16_t value (8 bytes).
     */
    uint64_t BytesToUint64(const std::vector<uint8_t>& dataArray, uint8_t& pos);

    /**
     * Used to open a serial connection to a NXP JN516x device.
     */
    void OpenPort();

    /**
     * Asynchronously read a single byte received from the configured NXP JN516x device
     * assigned to this class instance. Received bytes are analyzed and accumulated to
     * further processing.
     */
    void ReadByte();

    /**
     * Used to initiate the IO context necessary for the asynchronous operations in the configured
     * NXP JN516x device.
     */
    void RunIoContext();

    /**
     * Function used to interpreted the accumulated received bytes and process the received data
     * into IEEE 802.15.4 MAC common part sublayer (MCPS) and MAC layer management entity (MLME)
     * services.
     */
    void ProcessData();

    /**
     *  Process the received primitive parameters and triggers a MLME-SCAN.confirm
     *  callback to be processed by next higher layer.
     */
    void ScanConfirm();

    /**
     *  Process the received primitive parameters and triggers a MLME-START.confirm
     *  callback to be processed by next higher layer.
     */
    void StartConfirm();

    /**
     *  Process the received primitive parameters and triggers a MLME-ASSOCIATE.indication
     *  callback to be processed by next higher layer.
     */
    void AssociateIndication();

    /**
     *  Process the received primitive parameters and triggers a MLME-COMM-STATUS.indication
     *  callback to be processed by next higher layer.
     */
    void CommStatusIndication();

    /**
     *  Process the received primitive parameters and triggers a MLME-ASSOCIATE.confirm
     *  callback to be processed by next higher layer.
     */
    void AssociateConfirm();

    /**
     *  Process the received primitive parameters and triggers a MCPS-DATA.confirm
     *  callback to be processed by next higher layer.
     */
    void DataConfirm();

    /**
     *  Process the received primitive parameters and triggers a MCPS-DATA.indication
     *  callback to be processed by next higher layer.
     */
    void DataIndication();

    /**
     *  Process the received primitive parameters and triggers a MLME-SET.confirm
     *  callback to be processed by next higher layer.
     */
    void SetConfirm();

    /**
     *  Process the received primitive parameters and triggers a MLME-GET.confirm
     *  callback to be processed by next higher layer.
     */
    void GetConfirm();

    /**
     *  Process the received primitive parameters and triggers a MLME-ORPHAN.indication
     *  callback to be processed by next higher layer.
     */
    void OrphanIndication();

    /**
     * Process the received primitive parameters and triggers a MLME-BEACON-NOTIFY.indication
     * callback to be processed by the next higher layer.
     */
    void BeaconNotifyIndication();

    /**
     * Process the received primitive parameters and triggers a MLME-SENSOR.indication
     * callback to be processed by the next higher layer.
     * This primitive indication is not compliant with the standard and is meant to serve
     * JN5169 devices that contain sensors (such as Tweilite CUE and Twelite ARIA)
     */
    void SensorIndication();

    /**
     * The port used by the NXP JN516x device associated to this instance.
     * e.g. /dev/ttyUSB0
     */
    std::string m_port;

    /**
     * The thread used to run the asynchronous serial communication IO context.
     */
    std::thread m_ioContextThread;

    /**
     * The object used to handle the mutually exclusive zones.
     */
    std::mutex m_mutex;

    /**
     * The identifier of the currently used instance of a boost::serial_port object.
     * Instances and their identifiers are stored in the static map serialPortInstances.
     */
    uint32_t m_currentInstanceId;

    /**
     *  Indicates the state of the current received data during its process to be interpreted
     *  into primitives.
     */
    ReadState m_rxState;

    /**
     * Used to store the type of IEEE 802.15.4 MAC primitive type
     * last received.
     */
    PrimitiveType m_rxPrimitiveType;

    /**
     * Stores the size in bytes of the parameters in last received IEEE 802.15.4 MAC primitive.
     */
    uint32_t m_paramsMaxSize;

    /**
     *  The buffer of bytes for all the received unprocessed data.
     */
    std::vector<uint8_t> m_rxData;

    /**
     * Counts the number of bytes since the last identified uprocessed primitive.
     * Primitives are processed once the number of bytes counted equals to the m_paramsMaxSize
     * identified.
     */
    uint32_t m_rxByteCount;

    /**
     * The node id of Node associated to this MAC layer Netdevice.
     */
    uint32_t m_nodeId;
};

} // namespace uart
} // namespace ns3

#endif /* UART_LRWPAN_MAC_H */
