/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#ifndef WBAN_PHY_H
#define WBAN_PHY_H

#include "wban-error-model.h"
#include "wban-interference-helper.h"

#include "ns3/event-id.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/spectrum-phy.h"
#include "ns3/traced-callback.h"
#include "ns3/traced-value.h"

namespace ns3
{
class Packet;
class SpectrumValue;
class MobilityModel;
class SpectrumChannel;
class SpectrumModel;
class AntennaModel;
class NetDevice;
class UniformRandomVariable;
class ErrorModel;

namespace wban
{

class WbanErrorModel;
struct WbanSpectrumSignalParameters;

/**
 * @ingroup wban
 *
 * Helper structure to manage the power measurement during ED.
 */
typedef struct
{
    double averagePower;    //!< Average measured power
    Time lastUpdate;        //!< Last update time
    Time measurementLength; //!< Total measurement period
} WbanEdPower;

/**
 * @ingroup wban
 *
 * This data structure provides the Bit rate and Symbol rate for a given channel
 * See IEEE802.15.6  Table 29
 */
typedef struct
{
    double bitRate;    //!< bit rate
    double symbolRate; //!< symbol rate
} WbanPhyPreambleAndPlcpDataSymbolRates;

/**
 * @ingroup wban
 * @struct WbanPhyModulationParameters
 * @brief Holds the modulation-related parameters used by the WBAN PHY layer.
 * This structure stores the bitrate of the preamble and other modulation settings,
 * used for determining transmission characteristics at the PHY layer.
 */
struct WbanPhyModulationParameters
{
    double preambleBitRate; //!< kbps newly added parameter
    double bitRate;         //!< kbps
    double symbolRate;      //!< ksps
    double m;               //!< modulation
    double k;               //!< BCH code rate "k value"
    double n;               //!< BCH code rate "n value"
    double s;               //!< spreading Factor
    double sensitivity;     //!< minimum sensitivity for each band
    double channel;         //!< max number of channels
};

/**
 * @ingroup wban
 * This Phy option will be used to index frequencies in Tables 29-35
 */
enum class WbanPhyOption : uint32_t
{
    NB_402_MHZ_75_9 = 0,
    NB_402_MHZ_151_8 = 1,
    NB_863_MHZ_101_2 = 2,
    NB_863_MHZ_202_4 = 3,
    NB_902_MHZ_101_2 = 4,
    NB_902_MHZ_202_4 = 5,
    NB_950_MHZ_101_2 = 6,
    NB_950_MHZ_202_4 = 7,
    NB_2360_MHZ_121_4 = 8,
    NB_2360_MHZ_242_9 = 9,
    NB_2400_MHZ_121_4 = 10,
    NB_2400_MHZ_242_9 = 11,
    NB_INVALID_BAND = 12
};

/**
 * @ingroup wban
 * IEEE802.15.6 PHY Enumerations
 */
enum class WbanPhyState
{
    PHY_BUSY = 0x00,
    PHY_BUSY_RX = 0x01,
    PHY_BUSY_TX = 0x02,
    PHY_FORCE_TRX_OFF = 0x03,
    PHY_IDLE = 0x04,
    PHY_INVALID_PARAMETER = 0x05,
    PHY_RX_ON = 0x06,
    PHY_SUCCESS = 0x07,
    PHY_TRX_OFF = 0x08,
    PHY_TX_ON = 0x09,
    PHY_UNSUPPORTED_ATTRIBUTE = 0xa,
    PHY_READ_ONLY = 0xb,
    PHY_UNSPECIFIED = 0xc // all cases not covered by ieee802.15.6
};

/**
 *  Overloaded operator to print the value of a PhyEnumeration.
 *
 *  @param os The output stream
 *  @param state The text value of the PHY state
 *  @return The output stream with text value of the PHY state
 */
std::ostream& operator<<(std::ostream& os, const WbanPhyOption& phyOp);

/**
 *  Overloaded operator to print the value of a TracedValue<PhyEnumeration>.
 *
 *  @param os The output stream
 *  @param state The text value of the PHY state
 *  @return The output stream with text value of the PHY state
 */
std::ostream& operator<<(std::ostream& os, const TracedValue<WbanPhyOption>& phyOp);

/**
 *  Overloaded operator to print the value of a PhyEnumeration.
 *
 *  @param os The output stream
 *  @param state The text value of the PHY state
 *  @return The output stream with text value of the PHY state
 */
std::ostream& operator<<(std::ostream& os, const WbanPhyState& state);

/**
 *  Overloaded operator to print the value of a TracedValue<PhyEnumeration>.
 *
 *  @param os The output stream
 *  @param state The text value of the PHY state
 *  @return The output stream with text value of the PHY state
 */
std::ostream& operator<<(std::ostream& os, const TracedValue<WbanPhyState>& state);

namespace TracedValueCallback
{
/**
 * @ingroup wban
 * TracedValue callback signature for WbanPhyState.
 *
 * @param [in] oldValue original value of the traced variable
 * @param [in] newValue new value of the traced variable
 */
typedef void (*WbanPhyState)(WbanPhyState oldValue, WbanPhyState newValue);
} // namespace TracedValueCallback

/**
 * @ingroup wban
 *
 * PHY PIB Attribute Identifiers
 */
typedef enum
{
    phyCurrentChannel = 0x00,
    phyChannelsSupported = 0x01,
    phyTransmitPower = 0x02,
    phyCCAMode = 0x03,
} WbanPibAttributeIdentifier;

/**
 * @ingroup wban
 * * \struct WbanPhyPibAttributes
 * @brief Stores PHY PIB (PAN Information Base) attributes for WBAN PHY layer.
 */
typedef struct
{
    uint8_t phyCurrentChannel;          //!< The RF channel to use
    uint32_t phyChannelsSupported[256]; //!< BitField representing the available channels supported
    uint8_t phyTransmitPower; //!< 2 MSB: tolerance on the transmit power, 6 LSB: Tx power in dBm
                              //!< relative to 1mW (signed int in 2-complement format)
    uint8_t phyCCAMode;       //!< CCA mode
} WbanPhyPibAttributes;

/**
 * @enum WbanBandOptions
 * @brief Enumerates the available WBAN PHY frequency band options.
 */
typedef enum
{
    narrowBand = 0x00,
    ultraWideBand = 0x01,
    humanBodyBand = 0x02,
} WbanBandOptionsIdentifier;

/**
 * @ingroup wban
 * @brief Enumeration of available WBAN PHY bands.
 */
typedef struct
{
    uint8_t narrowBand;    //!< WBAN Narrowband
    uint8_t ultraWideBand; //!< WBAN Ultra wide band
    uint8_t humanBodyBand; //!< WBAN Human body communication band
} WbanBandOptions;

/**
 * @ingroup wban
 *
 * This method implements the PD SAP: PhyDataIndication
 *
 *  @param psduLength number of bytes in the PSDU
 *  @param p the packet to be transmitted
 *  @param lqi Link quality (LQI) value measured during reception of the PPDU
 */
typedef Callback<void, uint32_t, Ptr<Packet>, uint8_t> PhyDataIndicationCallback;

/**
 * @ingroup wban
 *
 * This method implements the PD SAP: PhyDataConfirm
 *
 * @param status the status to be transmitted
 */
typedef Callback<void, WbanPhyState> PhyDataConfirmCallback;

/**
 * @ingroup wban
 *
 * This method implements the PD SAP: PhyCcaConfirm
 *
 * @param status the status of CCA
 */
typedef Callback<void, WbanPhyState> PhyCcaConfirmCallback;

/**
 * @ingroup wban
 *
 * This method implements the PD SAP: PhyEdConfirm
 *
 * @param status the status of ED
 * @param energyLevel the energy level of ED
 */
typedef Callback<void, WbanPhyState, uint8_t> PhyEdConfirmCallback;

/**
 * @ingroup wban
 *
 * This method implements the PD SAP: PhyGetAttributeConfirm
 *
 * @param status the status of PhyGetAttributeRequest
 * @param id the identifier of attribute
 * @param attribute the pointer to attribute struct
 */
typedef Callback<void, WbanPhyState, WbanPibAttributeIdentifier, WbanPhyPibAttributes*>
    PhyGetAttributeConfirmCallback;

/**
 * @ingroup wban
 *
 * This method implements the PD SAP: PhySetTRXStateConfirm
 *
 * @param status the status of PhySetTRXStateRequest
 */
typedef Callback<void, WbanPhyState> PhySetTRXStateConfirmCallback;

/**
 * @ingroup wban
 *
 * This method implements the PD SAP: PhySetAttributeConfirm
 *
 * @param status the status of PhySetAttributeRequest
 * @param id the identifier of attribute
 */
typedef Callback<void, WbanPhyState, WbanPibAttributeIdentifier> PhySetAttributeConfirmCallback;

/**
 * @class WbanPhy
 * @brief Models the Physical Layer for IEEE 802.15.6 WBAN in NS-3.
 */
class WbanPhy : public SpectrumPhy
{
  public:
    /**
     * Get the type ID.
     *
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    /**
     * The maximum packet size accepted by the PHY.
     */
    static const uint32_t aMaxPhyPacketSize;
    /**
     * The turnaround time for switching the transceiver from RX to TX or vice
     * versa.
     */
    static const uint32_t aTurnaroundTime;
    /**
     * Default constructor.
     */
    WbanPhy();
    ~WbanPhy() override;
    /**
     * The currently configured PHY type.
     */
    WbanPhyOption m_phyOption;
    /**
     * @brief Set the modulation option used by this PHY.
     * See IEEE 802.15.6 Table 29-35
     * @param m_phyOption The phy modulation option used by the model.
     */
    void SetPhyOptions(WbanPhyOption m_phyOption);
    /**
     * @brief Set the receiver power sensitivity used by this device in dBm.
     * @param dbmSensitivity The receiver power sensitivity to set in dBm.
     */
    void SetRxSensitivity(double dbmSensitivity);
    /**
     * Get the receiver power sensitivity used by this device in dBm.
     *
     * @return The receiver power sensitivity used by this device in dBm.
     */
    double GetRxSensitivity();
    // inherited from SpectrumPhy
    void SetMobility(Ptr<MobilityModel> m) override;
    Ptr<MobilityModel> GetMobility() const override;
    void SetChannel(Ptr<SpectrumChannel> c) override;
    /**
     * Get the currently attached channel.
     *
     * @return the channel
     */
    Ptr<SpectrumChannel> GetChannel();
    void SetDevice(Ptr<NetDevice> d) override;
    Ptr<NetDevice> GetDevice() const override;
    /**
     * Set the attached antenna.
     *
     * @param a the antenna
     */
    void SetAntenna(Ptr<AntennaModel> a);
    Ptr<Object> GetAntenna() const override;
    Ptr<const SpectrumModel> GetRxSpectrumModel() const override;

    /**
     * set the error model to use
     *
     * @param e pointer to WbanErrorModel to use
     */

    void SetErrorModel(Ptr<WbanErrorModel> e);
    /**
     * get the error model in use
     *
     * @return pointer to WbanErrorModel in use
     */
    Ptr<WbanErrorModel> GetErrorModel() const;
    /**
     * Set the Power Spectral Density of outgoing signals in W/Hz.
     *
     * @param txPsd the Power Spectral Density value
     */
    void SetTxPowerSpectralDensity(Ptr<SpectrumValue> txPsd);
    /**
     * Set the noise power spectral density.
     *
     * @param noisePsd the Noise Power Spectral Density in power units
     * (Watt, Pascal...) per Hz.
     */
    void SetNoisePowerSpectralDensity(Ptr<const SpectrumValue> noisePsd);
    /**
     * set the callback for the end of an GetAttribute, as part of the
     * interconnections between the PHY and the MAC. The callback
     * implement PHY GetAttribute confirm SAP
     * @param c the callback
     */
    void SetPhyGetAttributeConfirmCallback(PhyGetAttributeConfirmCallback c);
    /**
     * set the callback for the end of an SetAttribute, as part of the
     * interconnections between the PHY and the MAC. The callback
     * implement PHY SetAttribute confirm SAP
     * @param c the callback
     */
    void SetPhySetAttributeConfirmCallback(PhySetAttributeConfirmCallback c);
    /**
     * set the callback for the end of an ED, as part of the
     * interconnections between the PHY and the MAC. The callback
     * implement PHY ED confirm SAP
     * @param c the callback
     */
    void SetPhyEdConfirmCallback(PhyEdConfirmCallback c);
    /**
     * set the callback for the end of an SetTRXState, as part of the
     * interconnections between the PHY and the MAC. The callback
     * implement PHY SetTRXState confirm SAP
     * @param c the callback
     */
    void SetPhySetTRXStateConfirmCallback(PhySetTRXStateConfirmCallback c);
    /**
     * set the callback for the end of a RX, as part of the
     * interconnections between the PHY and the MAC. The callback
     * implements PD Indication SAP.
     * @param c the callback
     */
    void SetPhyDataIndicationCallback(PhyDataIndicationCallback c);
    /**
     * set the callback for the end of a CCA, as part of the
     * interconnections between the PHY and the MAC. The callback
     * implement PHY CCA confirm SAP
     * @param c the callback
     */
    void SetPhyCcaConfirmCallback(PhyCcaConfirmCallback c);
    /**
     * set the callback for the end of a TX, as part of the
     * interconnections between the PHY and the MAC. The callback
     * implements PD SAP.
     * @param c the callback
     */
    void SetPhyDataConfirmCallback(PhyDataConfirmCallback c);
    /**
     *  IEEE 802.15.6
     *  PHY DATA request
     *  Request to transfer MPDU from MAC (transmitting)
     *  @param psduLength number of bytes in the PSDU
     *  @param p the packet to be transmitted
     */
    void PhyDataRequest(uint32_t psduLength, Ptr<Packet> p);
    /**
     * Get the noise power spectral density.
     *
     * @return the Noise Power Spectral Density
     */
    Ptr<const SpectrumValue> GetNoisePowerSpectralDensity();
    /**
     *Notify the SpectrumPhy instance of an incoming waveform.
     *
     * @param params the SpectrumSignalParameters associated with the incoming waveform
     */
    void StartRx(Ptr<SpectrumSignalParameters> params) override;
    /**
     *  PHY-CCA.request
     */
    void PhyCcaRequest();
    /**
     *  PHY-ED.request
     */
    void PhyEdRequest();
    /**
     *  PHY-GET.request
     *  @param id the attributed identifier
     */
    void PhyGetAttributeRequest(WbanPibAttributeIdentifier id);
    /**
     *  PHY-SET-TRX-STATE.request
     *  Set PHY state
     *  @param state in RX_ON,TRX_OFF,FORCE_TRX_OFF,TX_ON
     */
    void PhySetTRXStateRequest(WbanPhyState state);
    /**
     *  PHY-SET.request
     *  @param id the attributed identifier
     *  @param attribute the attribute value
     */
    void PhySetAttributeRequest(WbanPibAttributeIdentifier id, WbanPhyPibAttributes* attribute);
    /**
     * @brief Get the Preamble Data Rate Or Symbol Rate object
     * @param isData
     * @return double
     */
    double GetPreambleDataOrSymbolRate(bool isData);
    /**
     * @brief Get the PSDU Data Rate Or Symbol Rate object
     * @param isData
     * @return double
     */
    double GetPsduDataOrSymbolRate(bool isData);
    /**
     * @brief calculate the PSDU Data Rate object
     * @param isData
     * @return double
     */
    double GetPsduDataRate(bool isData) const;
    /**
     * TracedCallback signature for Trx state change events.
     *
     * @param [in] time The time of the state change.
     * @param [in] oldState The old state.
     * @param [in] newState The new state.
     * @deprecated The WbanPhyState state is now accessible as the
     * TracedValue @c TrxStateValue.  The @c TrxState TracedCallback will
     * be removed in a future release.
     */
    typedef void (*StateTracedCallback)(Time time, WbanPhyState oldState, WbanPhyState newState);

    /**
     * Assign a fixed random variable stream number to the random variables
     * used by this model.  Return the number of streams that have been assigned.
     *
     * @param stream first stream index to use
     * @return the number of stream indices assigned by this model
     */
    int64_t AssignStreams(int64_t stream);

  protected:
    /**
     * The data and symbol rates for the different PHY options.
     * See Table 29 in IEEE 802.15.6.
     * Bit rate is in kBit/s.  Symbol rate is in kSymbol/s.
     */
    static const WbanPhyPreambleAndPlcpDataSymbolRates preambleAndPlcpDataSymbolRates[6];

  private:
    /**
     * The second is true if the first is flagged as error/invalid.
     */
    typedef std::pair<Ptr<Packet>, bool> PacketAndStatus;
    // Inherited from Object.
    void DoDispose() override;
    /**
     * Change the PHY state to the given new state, firing the state change trace.
     *
     * @param newState the new state
     */
    void ChangeTrxState(WbanPhyState newState);
    /**
     * Get the currently configured PHY option.
     * See IEEE 802.15.6 , section 8.6.2 Table 45.
     *
     * @return the PHY option
     */
    WbanPhyOption GetPhyOption() const;
    /**
     * Finish the transmission of a frame. This is called at the end of a frame
     * transmission, applying possibly pending PHY state changes and firing the
     * appropriate trace sources and confirm callbacks to the MAC.
     */
    void EndTx();
    /**
     * Check if the interference destroys a frame currently received. Called
     * whenever a change in interference is detected.
     */
    void CheckInterference();
    /**
     * Finish the reception of a frame. This is called at the end of a frame
     * reception, applying possibly pending PHY state changes and firing the
     * appropriate trace sources and indication callbacks to the MAC. A frame
     * destroyed by noise/interference is dropped here, but not during reception.
     * This method is also called for every packet which only contributes to
     * interference.
     *
     * @param params signal parameters of the packet
     */
    void EndRx(Ptr<SpectrumSignalParameters> params);
    /**
     * Cancel an ongoing ED procedure. This is called when the transceiver is
     * switched off or set to TX mode. This calls the appropriate confirm callback
     * of the MAC.
     *
     * @param state the new state which is the cause for canceling ED
     */
    void CancelEd(WbanPhyState state);
    /**
     * Called at the end of the ED procedure. The average energy detected is
     * reported to the MAC.
     */
    void EndEd();
    /**
     * Called at the end of the CCA. The channel condition (busy or idle) is
     * reported to the MAC or CSMA/CA.
     */
    void EndCca();
    /**
     * Called after applying a deferred transceiver state switch. The result of
     * the state switch is reported to the MAC.
     */
    void EndSetTRXState();
    /**
     * Calculate the time required for sending the given packet, including
     * preamble, SFD and PHR.
     *
     * @param packet the packet for which the transmission time should be calculated
     * @return the time required for transmitting the packet
     */
    Time CalculateTxTime(Ptr<const Packet> packet);
    /**
     * Calculate the time required for sending the PPDU header, that is the
     * preamble,+ plcp header.
     * @returns The time required for sending the PPDU header.
     */
    Time GetPlcpHeaderTxTime();
    /**
     * Transform watts (W) to decibels milliwatts (dBm).
     *
     * @param watt The Watts that will be converted to dBm.
     * @return The value of Watts in dBm.
     */
    double WToDbm(double watt);
    /**
     * Transforms decibels milliWatts (dBm) to watts (W).
     *
     * @param dbm The DBm that will be converted to Watts.
     * @return The value of DBm in Watts.
     */
    double DbmToW(double dbm);
    /**
     * Check if the given channel is supported by the PHY.
     *
     * @param channel the channel to check
     * @return true, if the channel is supported, false otherwise
     */
    bool ChannelSupported(uint8_t channel);
    /**
     * Check if the PHY is busy, which is the case if the PHY is currently sending
     * or receiving a frame.
     * @return true, if the PHY is busy, false otherwise
     */
    bool PhyIsBusy() const;
    // Trace sources
    /**
     * The trace source fired when a packet begins the transmission process on
     * the medium.
     * @see class CallBackTraceSource
     */
    TracedCallback<Ptr<const Packet>> m_phyTxBeginTrace;
    /**
     * The trace source fired when a packet ends the transmission process on
     * the medium.
     * @see class CallBackTraceSource
     */
    TracedCallback<Ptr<const Packet>> m_phyTxEndTrace;
    /**
     * The trace source fired when the phy layer drops a packet as it tries
     * to transmit it.
     * @see class CallBackTraceSource
     */
    TracedCallback<Ptr<const Packet>> m_phyTxDropTrace;
    /**
     * The trace source fired when a packet begins the reception process from
     * the medium.
     *
     * @see class CallBackTraceSource
     */
    TracedCallback<Ptr<const Packet>> m_phyRxBeginTrace;
    /**
     * The trace source fired when a packet ends the reception process from
     * the medium.  Second quantity is received SINR.
     *
     * @see class CallBackTraceSource
     */
    TracedCallback<Ptr<const Packet>, double> m_phyRxEndTrace;
    /**
     * The trace source fired when the phy layer drops a packet it has received.
     *
     * @see class CallBackTraceSource
     */
    TracedCallback<Ptr<const Packet>> m_phyRxDropTrace;
    /**
     * The trace source fired when the phy layer changes the transceiver state.
     *
     * @see class CallBackTraceSource
     * @deprecated The WbanPhyState state is now accessible as the
     * TracedValue @c TrxStateValue.  This TracedCallback will
     * be removed in a future release.
     */
    TracedCallback<Time, WbanPhyState, WbanPhyState> m_trxStateLogger;
    /**
     * This callback is used to report attribute set results back to the MAC.
     */
    PhySetAttributeConfirmCallback m_phySetAttributeConfirmCallback;
    /**
     * This callback is used to report requested attribute values back to the MAC.
     */
    PhyGetAttributeConfirmCallback m_phyGetAttributeConfirmCallback;
    /**
     * This callback is used to report ED status to the MAC.
     */
    PhyEdConfirmCallback m_phyEdConfirmCallback;
    // Callbacks
    /**
     * This callback is used to notify incoming packets to the MAC layer.
     */
    PhyDataIndicationCallback m_phyDataIndicationCallback;
    /**
     * This callback is used to report CCA status to the MAC or CSMA/CA.
     */
    PhyCcaConfirmCallback m_phyCcaConfirmCallback;
    /**
     * This callback is used to report packet transmission status to the MAC layer.
     */
    PhyDataConfirmCallback m_phyDataConfirmCallback;
    /**
     * This callback is used to report transceiver state change status to the MAC.
     */
    PhySetTRXStateConfirmCallback m_phySetTRXStateConfirmCallback;
    /**
     * Calculates the nominal transmit power of the device in decibels relative to 1 mW
     * according to the representation of the PIB attribute phyTransmitPower.
     * This function extracts the 6 LSBs corresponding to the nominal transmit power and
     * converts it to a `int8_t`.
     *
     * @param phyTransmitPower the PIB attribute
     * @return The nominal transmit power in dBm.
     */
    int8_t GetNominalTxPowerFromPib(uint8_t phyTransmitPower);
    /**
     * The mobility model used by the PHY.
     */
    Ptr<MobilityModel> m_mobility;
    /**
     * The configured net device.
     */
    Ptr<NetDevice> m_device;
    /**
     * The channel attached to this transceiver.
     */
    Ptr<SpectrumChannel> m_channel;
    /**
     * The antenna used by the transceiver.
     */
    Ptr<AntennaModel> m_antenna;
    /**
     * The transmit power spectral density.
     */
    Ptr<SpectrumValue> m_txPsd;
    /**
     * The spectral density for for the noise.
     */
    Ptr<const SpectrumValue> m_noise;
    /**
     * The error model describing the bit and packet error rates.
     */
    Ptr<WbanErrorModel> m_errorModel;
    /**
     * The current PHY PIB attributes.
     */
    WbanPhyPibAttributes m_phyPIBAttributes;
    // State variables
    /**
     * The current transceiver state.
     */
    TracedValue<WbanPhyState> m_trxState;
    /**
     * The next pending state to applied after the current action of the PHY is
     * completed.
     */
    WbanPhyState m_trxStatePending;
    /**
     * Helper value for tracking the average power during ED.
     */
    WbanEdPower m_edPower;
    /**
     * Helper value for the peak power value during CCA.
     */
    double m_ccaPeakPower;
    /**
     * The receiver sensitivity.
     */
    double m_rxSensitivity;
    /**
     * The accumulated signals currently received by the transceiver, including
     * the signal of a possibly received packet, as well as all signals
     * considered noise.
     */
    Ptr<WbanInterferenceHelper> m_signal;
    /**
     * Timestamp of the last calculation of the PER of a packet currently received.
     */
    Time m_rxLastUpdate;
    /**
     * Status information of the currently received packet. The first parameter
     * contains the frame, as well the signal power of the frame. The second
     * parameter is set to false, if the frame is either invalid or destroyed
     * due to interference.
     */
    std::pair<Ptr<WbanSpectrumSignalParameters>, bool> m_currentRxPacket;
    /**
     * Status information of the currently transmitted packet. The first parameter
     * contains the frame. The second parameter is set to false, if the frame not
     * completely transmitted, in the event of a force transceiver switch, for
     * example.
     */
    PacketAndStatus m_currentTxPacket;
    /**
     * Scheduler event of a currently running CCA request.
     */
    EventId m_ccaRequest;
    /**
     * Scheduler event of a currently running ED request.
     */
    EventId m_edRequest;
    /**
     * Scheduler event of a currently running deferred transceiver state switch.
     */
    EventId m_setTRXState;
    /**
     * Scheduler event of a currently running data transmission request.
     */
    EventId m_phyDataRequest;
    /**
     * Uniform random variable stream.
     */
    Ptr<UniformRandomVariable> m_random;
};

} // namespace wban
} // namespace ns3
#endif /* WBAN_PHY_H */
