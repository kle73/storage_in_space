/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#include "wban-phy.h"

#include "wban-error-model.h"
#include "wban-lqi-tag.h"
#include "wban-phy-header.h"
#include "wban-spectrum-signal-parameters.h"
#include "wban-spectrum-value-helper.h"

#include "ns3/abort.h"
#include "ns3/antenna-model.h"
#include "ns3/double.h"
#include "ns3/header.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/net-device.h"
#include "ns3/network-module.h"
#include "ns3/nstime.h"
#include "ns3/packet-burst.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/spectrum-channel.h"
#include "ns3/spectrum-value.h"

#include <cmath>

namespace ns3
{

namespace wban
{

NS_LOG_COMPONENT_DEFINE("WbanPhy");

NS_OBJECT_ENSURE_REGISTERED(WbanPhy);

const uint32_t WbanPhy::aMaxPhyPacketSize = 264; // max PSDU in octets section 8.4 IEEE 802.15.6
const uint32_t WbanPhy::aTurnaroundTime = 85;    // RX-to-TX or TX-to-RX in microseconds

/* Modulation parameters in the order of
 * preamble bit Rate ,bitRate,symbolRate,M (Modulation),k (from BCH codes),n (from BCH codes),S
 * (Spreading factor), bandwidth, minimum sensitivity, number of channels .
 * Refer second row IEEE802.15.6 Table 29 to Table 35 PSDU π/2-DBPSK
 * The index follows WbanPhyOption
 */
static const WbanPhyModulationParameters ModulationParameters[] = {
    {57.5, 75.9, 187.5, 2, 51, 63, 2, -95, 9},  // NB_402MHZ, 75.9kbps, (Table 29)
    {57.5, 151.8, 187.5, 2, 51, 63, 1, -92, 9}, // NB_402MHZ, 151.8kbps, (Table 29)
    {76.6, 101.2, 250, 2, 51, 63, 2, -94, 13},  // NB_863MHZ, 101.2kbps, (Table 31)
    {76.6, 202.4, 250, 2, 51, 63, 1, -91, 13},  // NB_863MHZ, 202.4kbps, (Table 31)
    {76.6, 101.2, 250, 2, 51, 63, 2, -94, 59},  // NB_902MHZ, 101.2kbps, (Table 31)
    {76.6, 202.4, 250, 2, 51, 63, 1, -91, 59},  // NB_902MHZ, 202.4kbps, (Table 31)
    {76.6, 101.2, 250, 2, 51, 63, 2, -94, 15},  // NB_950MHZ, 101.2kbps, (Table 31)
    {76.6, 202.4, 250, 2, 51, 63, 1, -91, 15},  // NB_950MHZ, 202.4kbps, (Table 31)
    {91.9, 121.4, 600, 2, 51, 63, 4, -92, 38},  // NB_2360MHZ, 121.4kbps, (Table 35)
    {91.9, 242.9, 600, 2, 51, 63, 2, -90, 38},  // NB_2360MHZ, 242.9kbps, (Table 35)
    {91.9, 121.4, 600, 2, 51, 63, 4, -92, 78},  // NB_2400MHZ, 121.4kbps, (Table 35)
    {91.9, 242.9, 600, 2, 51, 63, 2, -90, 78}   // NB_2400MHZ, 242.9kbps, (Table 35)
};

std::ostream&
operator<<(std::ostream& os, const WbanPhyState& state)
{
    switch (state)
    {
    case WbanPhyState::PHY_BUSY:
        os << "BUSY";
        break;
    case WbanPhyState::PHY_BUSY_RX:
        os << "BUSY_RX";
        break;
    case WbanPhyState::PHY_BUSY_TX:
        os << "BUSY_TX";
        break;
    case WbanPhyState::PHY_FORCE_TRX_OFF:
        os << "FORCE_TRX_OFF";
        break;
    case WbanPhyState::PHY_IDLE:
        os << "IDLE";
        break;
    case WbanPhyState::PHY_INVALID_PARAMETER:
        os << "INVALID_PARAMETER";
        break;
    case WbanPhyState::PHY_RX_ON:
        os << "RX_ON";
        break;
    case WbanPhyState::PHY_SUCCESS:
        os << "SUCCESS";
        break;
    case WbanPhyState::PHY_TRX_OFF:
        os << "TRX_OFF";
        break;
    case WbanPhyState::PHY_TX_ON:
        os << "TX_ON";
        break;
    case WbanPhyState::PHY_UNSUPPORTED_ATTRIBUTE:
        os << "UNSUPPORTED";
        break;
    case WbanPhyState::PHY_READ_ONLY:
        os << "READ_ONLY";
        break;
    case WbanPhyState::PHY_UNSPECIFIED:
        os << "UNSPECIFIED";
        break;
    }
    return os;
}

std::ostream&
operator<<(std::ostream& os, const TracedValue<WbanPhyState>& state)
{
    WbanPhyState s = state;
    return os << s;
}

std::ostream&
operator<<(std::ostream& os, const WbanPhyOption& phyOp)
{
    switch (phyOp)
    {
    case WbanPhyOption::NB_402_MHZ_75_9:
        os << "NB_402_MHZ_75_9";
        break;
    case WbanPhyOption::NB_402_MHZ_151_8:
        os << "NB_402_MHZ_151_8";
        break;
    case WbanPhyOption::NB_863_MHZ_101_2:
        os << "NB_863_MHZ_101_2";
        break;
    case WbanPhyOption::NB_863_MHZ_202_4:
        os << "NB_863_MHZ_202_4";
        break;
    case WbanPhyOption::NB_902_MHZ_101_2:
        os << "NB_902_MHZ_101_2";
        break;
    case WbanPhyOption::NB_902_MHZ_202_4:
        os << "NB_902_MHZ_202_4";
        break;
    case WbanPhyOption::NB_950_MHZ_101_2:
        os << "NB_950_MHZ_101_2";
        break;
    case WbanPhyOption::NB_950_MHZ_202_4:
        os << "NB_950_MHZ_202_4";
        break;
    case WbanPhyOption::NB_2360_MHZ_121_4:
        os << "NB_2360_MHZ_121_4";
        break;
    case WbanPhyOption::NB_2360_MHZ_242_9:
        os << "NB_2360_MHZ_242_9";
        break;
    case WbanPhyOption::NB_2400_MHZ_121_4:
        os << "NB_2400_MHZ_121_4";
        break;
    case WbanPhyOption::NB_2400_MHZ_242_9:
        os << "NB_2400_MHZ_242_9";
        break;
    case WbanPhyOption::NB_INVALID_BAND:
        os << "NB_INVALID_BAND";
        break;
    }
    return os;
}

std::ostream&
operator<<(std::ostream& os, const TracedValue<WbanPhyOption>& phyOp)
{
    WbanPhyOption s = phyOp;
    return os << s;
}

TypeId
WbanPhy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::WbanPhy")
                            .SetParent<SpectrumPhy>()
                            .SetGroupName("Wban")
                            .AddConstructor<WbanPhy>()
                            .AddTraceSource("TrxStateValue",
                                            "The state of the transceiver",
                                            MakeTraceSourceAccessor(&WbanPhy::m_trxState),
                                            "ns3::TracedValueCallback::WbanPhyState")
                            .AddTraceSource("TrxState",
                                            "The state of the transceiver",
                                            MakeTraceSourceAccessor(&WbanPhy::m_trxStateLogger),
                                            "ns3::WbanPhy::StateTracedCallback")
                            .AddTraceSource("PhyTxBegin",
                                            "Trace source indicating a packet has "
                                            "begun transmitting over the channel medium",
                                            MakeTraceSourceAccessor(&WbanPhy::m_phyTxBeginTrace),
                                            "ns3::Packet::TracedCallback")
                            .AddTraceSource("PhyTxEnd",
                                            "Trace source indicating a packet has been "
                                            "completely transmitted over the channel.",
                                            MakeTraceSourceAccessor(&WbanPhy::m_phyTxEndTrace),
                                            "ns3::Packet::TracedCallback")
                            .AddTraceSource("PhyTxDrop",
                                            "Trace source indicating a packet has been "
                                            "dropped by the device during transmission",
                                            MakeTraceSourceAccessor(&WbanPhy::m_phyTxDropTrace),
                                            "ns3::Packet::TracedCallback")
                            .AddTraceSource("PhyRxBegin",
                                            "Trace source indicating a packet has begun "
                                            "being received from the channel medium by the device",
                                            MakeTraceSourceAccessor(&WbanPhy::m_phyRxBeginTrace),
                                            "ns3::Packet::TracedCallback")
                            .AddTraceSource("PhyRxEnd",
                                            "Trace source indicating a packet has been "
                                            "completely received from the channel medium "
                                            "by the device",
                                            MakeTraceSourceAccessor(&WbanPhy::m_phyRxEndTrace),
                                            "ns3::Packet::SinrTracedCallback")
                            .AddTraceSource("PhyRxDrop",
                                            "Trace source indicating a packet has been "
                                            "dropped by the device during reception",
                                            MakeTraceSourceAccessor(&WbanPhy::m_phyRxDropTrace),
                                            "ns3::Packet::TracedCallback");
    return tid;
}

WbanPhy::WbanPhy()
    : m_edRequest(),
      m_setTRXState()
{
    m_trxState = WbanPhyState::PHY_TRX_OFF;
    m_trxStatePending = WbanPhyState::PHY_IDLE;

    // default PHY PIB attributes
    // m_phyPIBAttributes.phyCurrentChannel = 11;
    m_phyPIBAttributes.phyTransmitPower = 0;

    for (uint32_t i = 0; i < 32; i++)
    {
        m_phyPIBAttributes.phyChannelsSupported[i] = 0x07ffffff;
    }
    m_phyPIBAttributes.phyCCAMode = 1;

    m_phyOption = WbanPhyOption::NB_2400_MHZ_242_9;
    m_edPower.averagePower = 0.0;
    m_edPower.lastUpdate = Seconds(0.0);
    m_edPower.measurementLength = Seconds(0.0);

    // default -113 dBm in W for 2.4 GHz
    m_rxSensitivity = pow(10.0, -113 / 10.0) / 1000.0;
    WbanSpectrumValueHelper psdHelper;
    m_txPsd = psdHelper.CreateTxPowerSpectralDensity(
        GetNominalTxPowerFromPib(m_phyPIBAttributes.phyTransmitPower),
        m_phyPIBAttributes.phyCurrentChannel);
    m_noise = psdHelper.CreateNoisePowerSpectralDensity(m_phyPIBAttributes.phyCurrentChannel);
    m_signal = Create<WbanInterferenceHelper>(m_noise->GetSpectrumModel());
    m_rxLastUpdate = Seconds(0);
    Ptr<Packet> none_packet = nullptr;
    Ptr<WbanSpectrumSignalParameters> none_params = nullptr;
    m_currentRxPacket = std::make_pair(none_params, true);
    m_currentTxPacket = std::make_pair(none_packet, true);
    m_random = CreateObject<UniformRandomVariable>();
    m_random->SetAttribute("Min", DoubleValue(0.0));
    m_random->SetAttribute("Max", DoubleValue(1.0));

    ChangeTrxState(WbanPhyState::PHY_TRX_OFF);
}

WbanPhy::~WbanPhy()
{
}

void
WbanPhy::DoDispose()
{
    NS_LOG_FUNCTION(this);

    // Cancel pending transceiver state change, if one is in progress.
    m_setTRXState.Cancel();
    m_trxState = WbanPhyState::PHY_TRX_OFF;
    m_trxStatePending = WbanPhyState::PHY_IDLE;

    m_device = nullptr;
    m_mobility = nullptr;
    m_channel = nullptr;
    m_txPsd = nullptr;
    m_noise = nullptr;
    m_random = nullptr;
    m_signal = nullptr;
    m_errorModel = nullptr;

    m_currentRxPacket.first = nullptr;
    m_currentTxPacket.first = nullptr;

    m_ccaRequest.Cancel();
    m_edRequest.Cancel();
    m_setTRXState.Cancel();
    m_phyDataRequest.Cancel();

    m_phyDataIndicationCallback = MakeNullCallback<void, uint32_t, Ptr<Packet>, uint8_t>();
    m_phyDataConfirmCallback = MakeNullCallback<void, WbanPhyState>();
    m_phySetTRXStateConfirmCallback = MakeNullCallback<void, WbanPhyState>();
    m_phyEdConfirmCallback = MakeNullCallback<void, WbanPhyState, uint8_t>();
    m_phyGetAttributeConfirmCallback =
        MakeNullCallback<void, WbanPhyState, WbanPibAttributeIdentifier, WbanPhyPibAttributes*>();
    m_phySetAttributeConfirmCallback =
        MakeNullCallback<void, WbanPhyState, WbanPibAttributeIdentifier>();
    m_phyCcaConfirmCallback = MakeNullCallback<void, WbanPhyState>();
    SpectrumPhy::DoDispose();
}

Ptr<NetDevice>
WbanPhy::GetDevice() const
{
    NS_LOG_FUNCTION(this);
    return m_device;
}

Ptr<MobilityModel>
WbanPhy::GetMobility() const
{
    NS_LOG_FUNCTION(this);
    return m_mobility;
}

void
WbanPhy::SetDevice(Ptr<NetDevice> d)
{
    NS_LOG_FUNCTION(this << d);
    m_device = d;
}

void
WbanPhy::SetMobility(Ptr<MobilityModel> m)
{
    NS_LOG_FUNCTION(this << m);
    m_mobility = m;
}

void
WbanPhy::SetChannel(Ptr<SpectrumChannel> c)
{
    NS_LOG_FUNCTION(this << c);
    m_channel = c;
}

Ptr<SpectrumChannel>
WbanPhy::GetChannel()
{
    NS_LOG_FUNCTION(this);
    return m_channel;
}

Ptr<const SpectrumModel>
WbanPhy::GetRxSpectrumModel() const
{
    NS_LOG_FUNCTION(this);
    if (m_txPsd)
    {
        return m_txPsd->GetSpectrumModel();
    }
    else
    {
        return nullptr;
    }
}

Ptr<Object>
WbanPhy::GetAntenna() const
{
    NS_LOG_FUNCTION(this);
    return m_antenna;
}

void
WbanPhy::SetAntenna(Ptr<AntennaModel> a)
{
    NS_LOG_FUNCTION(this << a);
    m_antenna = a;
}

void
WbanPhy::SetErrorModel(Ptr<WbanErrorModel> e)
{
    NS_LOG_FUNCTION(this << e);
    NS_ASSERT(e);
    m_errorModel = e;
}

Ptr<WbanErrorModel>
WbanPhy::GetErrorModel() const
{
    NS_LOG_FUNCTION(this);
    return m_errorModel;
}

void
WbanPhy::StartRx(Ptr<SpectrumSignalParameters> spectrumRxParams)
{
    NS_LOG_FUNCTION(this << spectrumRxParams);
    // WbanSpectrumValueHelper psdHelper;

    if (!m_edRequest.IsExpired())
    {
        // Update the average receive power during ED.
        Time now = Simulator::Now();
        m_edPower.averagePower +=
            WbanSpectrumValueHelper::TotalAvgPower(m_signal->GetSignalPsd(),
                                                   m_phyPIBAttributes.phyCurrentChannel) *
            (now - m_edPower.lastUpdate).GetTimeStep() / m_edPower.measurementLength.GetTimeStep();
        m_edPower.lastUpdate = now;
    }

    Ptr<WbanSpectrumSignalParameters> wbanRxParams =
        DynamicCast<WbanSpectrumSignalParameters>(spectrumRxParams);
    if (!wbanRxParams)
    {
        CheckInterference();
        m_signal->AddSignal(spectrumRxParams->psd);

        // Update peak power if CCA is in progress.
        if (!m_ccaRequest.IsExpired())
        {
            double power =
                WbanSpectrumValueHelper::TotalAvgPower(m_signal->GetSignalPsd(),
                                                       m_phyPIBAttributes.phyCurrentChannel);
            if (m_ccaPeakPower < power)
            {
                m_ccaPeakPower = power;
            }
        }

        Simulator::Schedule(spectrumRxParams->duration, &WbanPhy::EndRx, this, spectrumRxParams);
        return;
    }

    Ptr<Packet> p = (wbanRxParams->packetBurst->GetPackets()).front();
    NS_ASSERT(p);

    // Prevent PHY from receiving another packet while switching the transceiver state.
    if (m_trxState == WbanPhyState::PHY_RX_ON && !m_setTRXState.IsPending())
    {
        // The specification doesn't seem to refer to BUSY_RX, but vendor
        // data sheets suggest that this is a sub-state of the RX_ON state
        // that is entered after preamble detection when the digital receiver
        // is enabled.  Here, for now, we use BUSY_RX to mark the period between
        // StartRx() and EndRx() states.

        // We are going to BUSY_RX state when receiving the first bit of an SHR,
        // as opposed to real receivers, which should go to this state only after
        // successfully receiving the SHR.

        // If synchronizing to the packet is possible, change to BUSY_RX state,
        // otherwise drop the packet and stay in RX state. The actual synchronization
        // is not modeled.

        // Add any incoming packet to the current interference before checking the
        // SINR.

        NS_LOG_DEBUG(this << " receiving packet with power: "
                          << 10 * log10(WbanSpectrumValueHelper::TotalAvgPower(
                                      wbanRxParams->psd,
                                      m_phyPIBAttributes.phyCurrentChannel)) +
                                 30
                          << "dBm");
        m_signal->AddSignal(wbanRxParams->psd);
        Ptr<SpectrumValue> interferenceAndNoise = m_signal->GetSignalPsd();
        *interferenceAndNoise -= *wbanRxParams->psd;
        *interferenceAndNoise += *m_noise;
        double sinr = WbanSpectrumValueHelper::TotalAvgPower(wbanRxParams->psd,
                                                             m_phyPIBAttributes.phyCurrentChannel) /
                      WbanSpectrumValueHelper::TotalAvgPower(interferenceAndNoise,
                                                             m_phyPIBAttributes.phyCurrentChannel);

        // NS_LOG_DEBUG (sinr << " = per");
        //  Std. 802.15.4-2006, appendix E, Figure E.2
        //  At SNR < -5 the BER is less than 10e-1.
        //  It's useless to even *try* to decode the packet.
        // double a = 0 * log10 (sinr);
        if (10 * log10(sinr) > -5)
        {
            ChangeTrxState(WbanPhyState::PHY_BUSY_RX);
            m_currentRxPacket = std::make_pair(wbanRxParams, false);
            m_phyRxBeginTrace(p);

            m_rxLastUpdate = Simulator::Now();
        }
        else
        {
            m_phyRxDropTrace(p);
        }
    }
    else if (m_trxState == WbanPhyState::PHY_BUSY_RX)
    {
        // Drop the new packet.
        NS_LOG_DEBUG(this << " packet collision");
        m_phyRxDropTrace(p);

        // Check if we correctly received the old packet up to now.
        CheckInterference();

        // Add the incoming packet to the current interference after we have
        // checked for successful reception of the current packet for the time
        // before the additional interference.
        m_signal->AddSignal(wbanRxParams->psd);
    }
    else
    {
        // Simply drop the packet.
        NS_LOG_DEBUG(this << " transceiver not in RX state ");
        m_phyRxDropTrace(p);

        // Add the signal power to the interference, anyway.
        m_signal->AddSignal(wbanRxParams->psd);
    }

    // Update peak power if CCA is in progress.
    if (!m_ccaRequest.IsExpired())
    {
        double power = WbanSpectrumValueHelper::TotalAvgPower(m_signal->GetSignalPsd(),
                                                              m_phyPIBAttributes.phyCurrentChannel);
        if (m_ccaPeakPower < power)
        {
            m_ccaPeakPower = power;
        }
    }

    Simulator::Schedule(spectrumRxParams->duration, &WbanPhy::EndRx, this, spectrumRxParams);
}

void
WbanPhy::CheckInterference()
{
    // We are currently receiving a packet.
    // Calculate whether packet was lost.
    WbanSpectrumValueHelper psdHelper;
    Ptr<WbanSpectrumSignalParameters> currentRxParams = m_currentRxPacket.first;
    // We are currently receiving a packet.
    if (m_trxState == WbanPhyState::PHY_BUSY_RX)
    {
        //      NS_ASSERT (currentRxParams && !m_currentRxPacket.second);
        Ptr<Packet> currentPacket = currentRxParams->packetBurst->GetPackets().front();

        if (m_errorModel)
        {
            // How many bits did we receive since the last calculation?
            double t = (Simulator::Now() - m_rxLastUpdate).ToDouble(Time::MS);
            uint32_t chunkSize = ceil(t * (GetPsduDataOrSymbolRate(true) / 1000));
            Ptr<SpectrumValue> interferenceAndNoise = m_signal->GetSignalPsd();

            *interferenceAndNoise -= *currentRxParams->psd;

            *interferenceAndNoise += *m_noise;

            double sinr =
                WbanSpectrumValueHelper::TotalAvgPower(currentRxParams->psd,
                                                       m_phyPIBAttributes.phyCurrentChannel) /
                WbanSpectrumValueHelper::TotalAvgPower(interferenceAndNoise,
                                                       m_phyPIBAttributes.phyCurrentChannel);
            double per =
                1.0 - m_errorModel->GetChunkSuccessRate2400Mhz242KbpsUncoded(sinr, chunkSize);

            // The LQI is the total packet success rate scaled to 0-255.
            // If not already set, initialize to 255.
            WbanLqiTag tag(std::numeric_limits<uint8_t>::max());
            currentPacket->PeekPacketTag(tag);
            uint8_t lqi = tag.Get();
            tag.Set(lqi - (per * lqi));
            currentPacket->ReplacePacketTag(tag);

            if (m_random->GetValue() < per)
            {
                //           The packet was destroyed, drop the packet after reception.
                m_currentRxPacket.second = true;
            }
        }
        else
        {
            NS_LOG_WARN("Missing ErrorModel");
        }
    }
    else
    {
        NS_LOG_DEBUG("PHY_BUSY_RX, hence did implement error model");
    }
    m_rxLastUpdate = Simulator::Now();
}

void
WbanPhy::EndRx(Ptr<SpectrumSignalParameters> par)
{
    NS_LOG_FUNCTION(this);

    Ptr<WbanSpectrumSignalParameters> params = DynamicCast<WbanSpectrumSignalParameters>(par);

    if (!m_edRequest.IsExpired())
    {
        // Update the average receive power during ED.
        Time now = Simulator::Now();
        m_edPower.averagePower +=
            WbanSpectrumValueHelper::TotalAvgPower(m_signal->GetSignalPsd(),
                                                   m_phyPIBAttributes.phyCurrentChannel) *
            (now - m_edPower.lastUpdate).GetTimeStep() / m_edPower.measurementLength.GetTimeStep();
        m_edPower.lastUpdate = now;
    }

    Ptr<WbanSpectrumSignalParameters> currentRxParams = m_currentRxPacket.first;
    if (currentRxParams == params)
    {
        CheckInterference();
    }

    if (!params)
    {
        NS_LOG_LOGIC(/* commented for time being*/ "Node: "
                     << m_device->GetAddress() << " Removing interference: " << *(par->psd));
        return;
    }

    // Update the interference.
    m_signal->RemoveSignal(par->psd);

    if (currentRxParams == params)
    {
        Ptr<Packet> currentPacket = currentRxParams->packetBurst->GetPackets().front();
        NS_ASSERT(currentPacket);

        // If there is no error model attached to the PHY, we always report the maximum LQI value.
        WbanLqiTag tag(std::numeric_limits<uint8_t>::max());
        currentPacket->PeekPacketTag(tag);
        m_phyRxEndTrace(currentPacket, tag.Get());

        // remove header
        NarrowBandPhyHeader nb;
        currentPacket->RemoveHeader(nb);

        if (!m_currentRxPacket.second)
        {
            m_currentRxPacket = std::make_pair(nullptr, true);
            ChangeTrxState(WbanPhyState::PHY_RX_ON);
            NS_LOG_DEBUG("Packet successfully received");

            // The packet was successfully received, push it up the stack.
            if (!m_phyDataIndicationCallback.IsNull())
            {
                m_phyDataIndicationCallback(currentPacket->GetSize(), currentPacket, tag.Get());
            }
        }
        else
        {
            // The packet was destroyed, drop it.
            m_phyRxDropTrace(currentPacket);
        }

        Ptr<WbanSpectrumSignalParameters> none = nullptr;
        m_currentRxPacket = std::make_pair(none, true);

        // We may be waiting to apply a pending state change.
        if (m_trxStatePending != WbanPhyState::PHY_IDLE)
        {
            // Only change the state immediately, if the transceiver is not already
            // switching the state.
            if (!m_setTRXState.IsPending())
            {
                NS_LOG_LOGIC("Apply pending state change to " << m_trxStatePending);
                ChangeTrxState(m_trxStatePending);
                m_trxStatePending = WbanPhyState::PHY_IDLE;
            }
        }
        else
        {
            ChangeTrxState(WbanPhyState::PHY_RX_ON);
        }
    }
}

void
WbanPhy::PhyDataRequest(uint32_t psduLength, Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << psduLength << p);

    if (psduLength > aMaxPhyPacketSize)
    {
        if (!m_phyDataConfirmCallback.IsNull())
        {
            m_phyDataConfirmCallback(WbanPhyState::PHY_UNSPECIFIED);
        }
        NS_LOG_DEBUG("Drop packet because psduLength too long: " << psduLength);
        return;
    }

    // Prevent PHY from sending a packet while switching the transceiver state.
    if (!m_setTRXState.IsPending())
    {
        if (m_trxState == WbanPhyState::PHY_TX_ON)
        {
            // send down
            NS_ASSERT(m_channel);

            // Remove a possible LQI tag from a previous transmission of the packet.
            WbanLqiTag lqiTag;
            p->RemovePacketTag(lqiTag);

            m_phyTxBeginTrace(p);
            m_currentTxPacket.first = p;
            m_currentTxPacket.second = false;
            // Add narrowBand header to the packet
            NarrowBandPhyHeader nb;
            nb.SetRate(GetPsduDataOrSymbolRate(true));
            nb.SetLength(10);
            nb.SetBurstEnable();
            nb.SetSeedDisable();
            nb.SetHcs(2);
            nb.SetBchParity(01);
            p->AddHeader(nb);

            Ptr<WbanSpectrumSignalParameters> txParams = Create<WbanSpectrumSignalParameters>();
            txParams->duration = CalculateTxTime(p);
            NS_LOG_DEBUG("duration " << txParams->duration);

            txParams->txPhy = GetObject<SpectrumPhy>();
            txParams->psd = m_txPsd;
            txParams->txAntenna = m_antenna;
            Ptr<PacketBurst> pb = CreateObject<PacketBurst>();
            pb->AddPacket(p);

            txParams->packetBurst = pb;
            m_channel->StartTx(txParams);
            m_phyDataRequest = Simulator::Schedule(txParams->duration, &WbanPhy::EndTx, this);
            ChangeTrxState(WbanPhyState::PHY_BUSY_TX);
            return;
        }
        else if ((m_trxState == WbanPhyState::PHY_RX_ON) ||
                 (m_trxState == WbanPhyState::PHY_TRX_OFF) ||
                 (m_trxState == WbanPhyState::PHY_BUSY_TX))
        {
            if (!m_phyDataConfirmCallback.IsNull())
            {
                m_phyDataConfirmCallback(m_trxState);
            }

            // Drop packet, hit PhyTxDrop trace
            m_phyTxDropTrace(p);
            return;
        }
        else
        {
            NS_FATAL_ERROR("This should be unreachable, or else state "
                           << m_trxState << " should be added as a case");
        }
    }
    else
    {
        if (!m_phyDataConfirmCallback.IsNull())
        {
            m_phyDataConfirmCallback(WbanPhyState::PHY_UNSPECIFIED);
        }
        // Drop packet, hit PhyTxDrop trace
        m_phyTxDropTrace(p);
        return;
    }
}

void
WbanPhy::PhyCcaRequest()
{
    NS_LOG_FUNCTION(this);

    if (m_trxState == WbanPhyState::PHY_RX_ON || m_trxState == WbanPhyState::PHY_BUSY_RX)
    {
        m_ccaPeakPower = 0.0;
        Time ccaTime = Seconds(8.0 / GetPsduDataOrSymbolRate(false));
        m_ccaRequest = Simulator::Schedule(ccaTime, &WbanPhy::EndCca, this);
    }
    else
    {
        if (!m_phyCcaConfirmCallback.IsNull())
        {
            if (m_trxState == WbanPhyState::PHY_TRX_OFF)
            {
                m_phyCcaConfirmCallback(WbanPhyState::PHY_TRX_OFF);
            }
            else
            {
                m_phyCcaConfirmCallback(WbanPhyState::PHY_BUSY);
            }
        }
    }
}

void
WbanPhy::PhyEdRequest()
{
    NS_LOG_FUNCTION(this);
    if (m_trxState == WbanPhyState::PHY_RX_ON || m_trxState == WbanPhyState::PHY_BUSY_RX)
    {
        // Average over the powers of all signals received until EndEd()
        m_edPower.averagePower = 0;
        m_edPower.lastUpdate = Simulator::Now();
        m_edPower.measurementLength = Seconds(8.0 / GetPsduDataOrSymbolRate(false));
        m_edRequest = Simulator::Schedule(m_edPower.measurementLength, &WbanPhy::EndEd, this);
    }
    else
    {
        WbanPhyState result = m_trxState;
        if (m_trxState == WbanPhyState::PHY_BUSY_TX)
        {
            result = WbanPhyState::PHY_TX_ON;
        }
        if (!m_phyEdConfirmCallback.IsNull())
        {
            m_phyEdConfirmCallback(result, 0);
        }
    }
}

void
WbanPhy::PhyGetAttributeRequest(WbanPibAttributeIdentifier id)
{
    NS_LOG_FUNCTION(this << id);
    WbanPhyState status;

    switch (id)
    {
    case phyCurrentChannel:
    case phyChannelsSupported:
    case phyTransmitPower:
    case phyCCAMode: {
        status = WbanPhyState::PHY_SUCCESS;
        break;
    }
    default: {
        status = WbanPhyState::PHY_UNSUPPORTED_ATTRIBUTE;
        break;
    }
    }
    if (!m_phyGetAttributeConfirmCallback.IsNull())
    {
        WbanPhyPibAttributes retValue;
        memcpy(&retValue, &m_phyPIBAttributes, sizeof(WbanPhyPibAttributes));
        m_phyGetAttributeConfirmCallback(status, id, &retValue);
    }
}

void
WbanPhy::PhySetTRXStateRequest(WbanPhyState state)
{
    NS_LOG_FUNCTION(this << state);

    NS_ABORT_IF((state != WbanPhyState::PHY_RX_ON) && (state != WbanPhyState::PHY_TRX_OFF) &&
                (state != WbanPhyState::PHY_FORCE_TRX_OFF) && (state != WbanPhyState::PHY_TX_ON));

    NS_LOG_LOGIC("Trying to set m_trxState from " << m_trxState << " to " << state);
    // this method always overrides previous state setting attempts
    if (!m_setTRXState.IsExpired())
    {
        if (m_trxStatePending == state)
        {
            // Simply wait for the ongoing state switch.
            return;
        }
        else
        {
            NS_LOG_DEBUG("Cancel m_setTRXState");
            // Keep the transceiver state as the old state before the switching attempt.
            m_setTRXState.Cancel();
        }
    }
    if (m_trxStatePending != WbanPhyState::PHY_IDLE)
    {
        m_trxStatePending = WbanPhyState::PHY_IDLE;
    }
    if (state == m_trxState)
    {
        if (!m_phySetTRXStateConfirmCallback.IsNull())
        {
            m_phySetTRXStateConfirmCallback(state);
        }
        return;
    }

    if (((state == WbanPhyState::PHY_RX_ON) || (state == WbanPhyState::PHY_TRX_OFF)) &&
        (m_trxState == WbanPhyState::PHY_BUSY_TX))
    {
        NS_LOG_DEBUG("Phy is busy; setting state pending to " << state);
        m_trxStatePending = state;
        return; // Send PhySetTRXStateConfirm later
    }

    // specification talks about being in RX_ON and having received
    // a valid SFD.  Here, we are not modelling at that level of
    // granularity, so we just test for BUSY_RX state (any part of
    // a packet being actively received)
    if (state == WbanPhyState::PHY_TRX_OFF)
    {
        CancelEd(state);

        if ((m_trxState == WbanPhyState::PHY_BUSY_RX) && (m_currentRxPacket.first) &&
            (!m_currentRxPacket.second))
        {
            NS_LOG_DEBUG("Receiver has valid SFD; defer state change");
            m_trxStatePending = state;
            return; // Send PhySetTRXStateConfirm later
        }
        else if (m_trxState == WbanPhyState::PHY_RX_ON || m_trxState == WbanPhyState::PHY_TX_ON)
        {
            ChangeTrxState(WbanPhyState::PHY_TRX_OFF);
            if (!m_phySetTRXStateConfirmCallback.IsNull())
            {
                m_phySetTRXStateConfirmCallback(state);
            }
            return;
        }
    }

    if (state == WbanPhyState::PHY_TX_ON)
    {
        CancelEd(state);

        NS_LOG_DEBUG("turn on PHY_TX_ON");
        if ((m_trxState == WbanPhyState::PHY_BUSY_RX) || (m_trxState == WbanPhyState::PHY_RX_ON))
        {
            if (m_currentRxPacket.first)
            {
                // terminate reception if needed
                // incomplete reception -- force packet discard
                NS_LOG_DEBUG("force TX_ON, terminate reception");
                m_currentRxPacket.second = true;
            }
            // If CCA is in progress, cancel CCA and return BUSY.
            if (!m_ccaRequest.IsExpired())
            {
                m_ccaRequest.Cancel();
                if (!m_phyCcaConfirmCallback.IsNull())
                {
                    m_phyCcaConfirmCallback(WbanPhyState::PHY_BUSY);
                }
            }

            m_trxStatePending = WbanPhyState::PHY_TX_ON;

            // Delay for turnaround time

            Time setTime = Seconds((double)aTurnaroundTime / GetPsduDataOrSymbolRate(false));
            m_setTRXState = Simulator::Schedule(setTime, &WbanPhy::EndSetTRXState, this);
            return;
        }
        else if (m_trxState == WbanPhyState::PHY_BUSY_TX || m_trxState == WbanPhyState::PHY_TX_ON)
        {
            // We do NOT change the transceiver state here. We only report that
            // the transceiver is already in TX_ON state.
            if (!m_phySetTRXStateConfirmCallback.IsNull())
            {
                m_phySetTRXStateConfirmCallback(WbanPhyState::PHY_TX_ON);
            }
            return;
        }

        else if (m_trxState == WbanPhyState::PHY_TRX_OFF)
        {
            ChangeTrxState(WbanPhyState::PHY_TX_ON);
            if (!m_phySetTRXStateConfirmCallback.IsNull())
            {
                m_phySetTRXStateConfirmCallback(WbanPhyState::PHY_TX_ON);
            }

            return;
        }
    }

    if (state == WbanPhyState::PHY_FORCE_TRX_OFF)
    {
        if (m_trxState == WbanPhyState::PHY_TRX_OFF)
        {
            NS_LOG_DEBUG("force TRX_OFF, was already off");
        }
        else
        {
            NS_LOG_DEBUG("force TRX_OFF, SUCCESS");
            if (m_currentRxPacket.first)
            { // terminate reception if needed
              // incomplete reception -- force packet discard
                NS_LOG_DEBUG("force TRX_OFF, terminate reception");
                m_currentRxPacket.second = true;
            }
            if (m_trxState == WbanPhyState::PHY_BUSY_TX)
            {
                NS_LOG_DEBUG("force TRX_OFF, terminate transmission");
                m_currentTxPacket.second = true;
            }
            ChangeTrxState(WbanPhyState::PHY_TRX_OFF);
            // Clear any other state
            m_trxStatePending = WbanPhyState::PHY_IDLE;
        }
        if (!m_phySetTRXStateConfirmCallback.IsNull())
        {
            m_phySetTRXStateConfirmCallback(WbanPhyState::PHY_SUCCESS);
        }

        return;
    }
    if (state == WbanPhyState::PHY_RX_ON)
    {
        if (m_trxState == WbanPhyState::PHY_TX_ON || m_trxState == WbanPhyState::PHY_TRX_OFF)
        {
            // Turnaround delay

            m_trxStatePending = WbanPhyState::PHY_RX_ON;

            Time setTime = Seconds((double)aTurnaroundTime / GetPsduDataOrSymbolRate(false));
            m_setTRXState = Simulator::Schedule(setTime, &WbanPhy::EndSetTRXState, this);
            return;
        }
        else if (m_trxState == WbanPhyState::PHY_BUSY_RX)
        {
            if (!m_phySetTRXStateConfirmCallback.IsNull())
            {
                m_phySetTRXStateConfirmCallback(WbanPhyState::PHY_RX_ON);
            }
            return;
        }
    }

    NS_FATAL_ERROR("Unexpected transition from state " << m_trxState << " to state " << state);
}

bool
WbanPhy::ChannelSupported(uint8_t channel)
{
    NS_LOG_FUNCTION(this << channel);
    bool retValue = false;

    for (uint32_t i = 0; i < 31; i++)
    {
        if ((m_phyPIBAttributes.phyChannelsSupported[i] & (1 << channel)) != 0)
        {
            retValue = true;
            break;
        }
    }

    return retValue;
}

void
WbanPhy::PhySetAttributeRequest(WbanPibAttributeIdentifier id, WbanPhyPibAttributes* attribute)
{
    NS_LOG_FUNCTION(this << id << attribute);
    NS_ASSERT(attribute);
    WbanPhyState status = WbanPhyState::PHY_SUCCESS;

    switch (id)
    {
    case phyCurrentChannel: {
        SetRxSensitivity(-113.95);
        if (!ChannelSupported(attribute->phyCurrentChannel))
        {
            status = WbanPhyState::PHY_INVALID_PARAMETER;
        }
        if (m_phyPIBAttributes.phyCurrentChannel != attribute->phyCurrentChannel)
        {
            // Cancel a pending transceiver state change.
            // Switch off the transceiver.

            m_trxState = WbanPhyState::PHY_TRX_OFF;
            if (m_trxStatePending != WbanPhyState::PHY_IDLE)
            {
                m_trxStatePending = WbanPhyState::PHY_IDLE;
                m_setTRXState.Cancel();
                if (!m_phySetTRXStateConfirmCallback.IsNull())
                {
                    m_phySetTRXStateConfirmCallback(WbanPhyState::PHY_TRX_OFF);
                }
            }

            // Any packet in transmission or reception will be corrupted.
            if (m_currentRxPacket.first)
            {
                m_currentRxPacket.second = true;
            }
            if (PhyIsBusy())
            {
                m_currentTxPacket.second = true;
                m_phyDataRequest.Cancel();
                m_currentTxPacket.first = nullptr;
                if (!m_phyDataConfirmCallback.IsNull())
                {
                    m_phyDataConfirmCallback(WbanPhyState::PHY_TRX_OFF);
                }
            }
            m_phyPIBAttributes.phyCurrentChannel = attribute->phyCurrentChannel;
            // use the prev configured sensitivity before changing the channel
            SetRxSensitivity(WToDbm(m_rxSensitivity));
        }
        break;
    }
    case phyChannelsSupported: { // only the first element is considered in the array
        if ((attribute->phyChannelsSupported[0] & 0xf8000000) != 0)
        { // 5 MSBs reserved
            status = WbanPhyState::PHY_INVALID_PARAMETER;
        }
        else
        {
            m_phyPIBAttributes.phyChannelsSupported[0] = attribute->phyChannelsSupported[0];
        }
        break;
    }
    case phyTransmitPower: {
        if (attribute->phyTransmitPower & 0xC0)
        {
            NS_LOG_LOGIC("WbanPhy::PhySetAttributeRequest error - can not change read-only "
                         "attribute bits.");
            status = WbanPhyState::PHY_INVALID_PARAMETER;
        }
        else
        {
            m_phyPIBAttributes.phyTransmitPower = attribute->phyTransmitPower;
            WbanSpectrumValueHelper psdHelper;
            m_txPsd = psdHelper.CreateTxPowerSpectralDensity(
                GetNominalTxPowerFromPib(m_phyPIBAttributes.phyTransmitPower),
                m_phyPIBAttributes.phyCurrentChannel);
        }
        break;
    }
    case phyCCAMode: {
        if ((attribute->phyCCAMode < 1) || (attribute->phyCCAMode > 3))
        {
            status = WbanPhyState::PHY_INVALID_PARAMETER;
        }
        else
        {
            m_phyPIBAttributes.phyCCAMode = attribute->phyCCAMode;
        }
        break;
    }
    default: {
        status = WbanPhyState::PHY_UNSUPPORTED_ATTRIBUTE;
        break;
    }
    }
    if (!m_phySetAttributeConfirmCallback.IsNull())
    {
        m_phySetAttributeConfirmCallback(status, id);
    }
}

void
WbanPhy::SetPhyDataIndicationCallback(PhyDataIndicationCallback c)
{
    NS_LOG_FUNCTION(this);
    m_phyDataIndicationCallback = c;
}

void
WbanPhy::SetPhyDataConfirmCallback(PhyDataConfirmCallback c)
{
    NS_LOG_FUNCTION(this);
    m_phyDataConfirmCallback = c;
}

void
WbanPhy::SetPhyCcaConfirmCallback(PhyCcaConfirmCallback c)
{
    NS_LOG_FUNCTION(this);
    m_phyCcaConfirmCallback = c;
}

void
WbanPhy::SetPhyEdConfirmCallback(PhyEdConfirmCallback c)
{
    NS_LOG_FUNCTION(this);
    m_phyEdConfirmCallback = c;
}

void
WbanPhy::SetPhyGetAttributeConfirmCallback(PhyGetAttributeConfirmCallback c)
{
    NS_LOG_FUNCTION(this);
    m_phyGetAttributeConfirmCallback = c;
}

void
WbanPhy::SetPhySetTRXStateConfirmCallback(PhySetTRXStateConfirmCallback c)
{
    NS_LOG_FUNCTION(this);
    m_phySetTRXStateConfirmCallback = c;
}

void
WbanPhy::SetPhySetAttributeConfirmCallback(PhySetAttributeConfirmCallback c)
{
    NS_LOG_FUNCTION(this);
    m_phySetAttributeConfirmCallback = c;
}

void
WbanPhy::ChangeTrxState(WbanPhyState newState)
{
    NS_LOG_LOGIC(this << " state: " << m_trxState << " -> " << newState);
    m_trxStateLogger(Simulator::Now(), m_trxState, newState);
    m_trxState = newState;
}

bool
WbanPhy::PhyIsBusy() const
{
    NS_LOG_FUNCTION(this << m_trxState);
    return ((m_trxState == WbanPhyState::PHY_BUSY_TX) ||
            (m_trxState == WbanPhyState::PHY_BUSY_RX) || (m_trxState == WbanPhyState::PHY_BUSY));
}

void
WbanPhy::CancelEd(WbanPhyState state)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(state == WbanPhyState::PHY_TRX_OFF || state == WbanPhyState::PHY_TX_ON);

    if (!m_edRequest.IsExpired())
    {
        m_edRequest.Cancel();
        if (!m_phyEdConfirmCallback.IsNull())
        {
            m_phyEdConfirmCallback(state, 0);
        }
    }
}

void
WbanPhy::EndEd()
{
    NS_LOG_FUNCTION(this);

    m_edPower.averagePower +=
        WbanSpectrumValueHelper::TotalAvgPower(m_signal->GetSignalPsd(),
                                               m_phyPIBAttributes.phyCurrentChannel) *
        (Simulator::Now() - m_edPower.lastUpdate).GetTimeStep() /
        m_edPower.measurementLength.GetTimeStep();

    uint8_t energyLevel;

    double ratio = m_edPower.averagePower / m_rxSensitivity;
    ratio = 10.0 * log10(ratio);
    if (ratio <= 10.0)
    { // less than 10 dB
        energyLevel = 0;
    }
    else if (ratio >= 40.0)
    { // less than 40 dB
        energyLevel = 255;
    }
    else
    {
        energyLevel = static_cast<uint8_t>(((ratio - 10.0) / 30.0) * 255.0);
    }

    if (!m_phyEdConfirmCallback.IsNull())
    {
        m_phyEdConfirmCallback(WbanPhyState::PHY_SUCCESS, energyLevel);
    }
}

void
WbanPhy::EndCca()
{
    NS_LOG_FUNCTION(this);
    WbanPhyState sensedChannelState = WbanPhyState::PHY_UNSPECIFIED;

    // Update peak power.
    double power = WbanSpectrumValueHelper::TotalAvgPower(m_signal->GetSignalPsd(),
                                                          m_phyPIBAttributes.phyCurrentChannel);
    if (m_ccaPeakPower < power)
    {
        m_ccaPeakPower = power;
    }

    if (PhyIsBusy())
    {
        sensedChannelState = WbanPhyState::PHY_BUSY;
    }
    else if (m_phyPIBAttributes.phyCCAMode == 1)
    {
        // -- ED threshold at most 10 dB above receiver sensitivity.
        if (10 * log10(m_ccaPeakPower / m_rxSensitivity) >= 10.0)
        {
            sensedChannelState = WbanPhyState::PHY_BUSY;
        }
        else
        {
            sensedChannelState = WbanPhyState::PHY_IDLE;
        }
    }
    else if (m_phyPIBAttributes.phyCCAMode == 2)
    {
        if (m_trxState == WbanPhyState::PHY_BUSY_RX)
        {
            // We currently do not model PPDU reception in detail. Instead we model
            // packet reception starting with the first bit of the preamble.
            // Therefore, this code will never be reached, as PhyIsBusy() would
            // already lead to a channel busy condition.

            sensedChannelState = WbanPhyState::PHY_BUSY;
        }
        else
        {
            sensedChannelState = WbanPhyState::PHY_IDLE;
        }
    }
    else if (m_phyPIBAttributes.phyCCAMode == 3)
    {
        if ((10 * log10(m_ccaPeakPower / m_rxSensitivity) >= 10.0) &&
            m_trxState == WbanPhyState::PHY_BUSY_RX)
        {
            // Again, this code will never be reached, if we are already receiving
            // a packet, as PhyIsBusy() would already lead to a channel busy condition.

            sensedChannelState = WbanPhyState::PHY_BUSY;
        }
        else
        {
            sensedChannelState = WbanPhyState::PHY_IDLE;
        }
    }
    else
    {
        NS_ASSERT_MSG(false, "Invalid CCA mode");
    }

    NS_LOG_LOGIC(this << "channel sensed state: " << sensedChannelState);

    if (!m_phyCcaConfirmCallback.IsNull())
    {
        m_phyCcaConfirmCallback(sensedChannelState);
    }
}

void
WbanPhy::EndSetTRXState()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_IF((m_trxStatePending != WbanPhyState::PHY_RX_ON) &&
                (m_trxStatePending != WbanPhyState::PHY_TX_ON));
    ChangeTrxState(m_trxStatePending);
    m_trxStatePending = WbanPhyState::PHY_IDLE;

    if (!m_phySetTRXStateConfirmCallback.IsNull())
    {
        m_phySetTRXStateConfirmCallback(m_trxState);
    }
}

void
WbanPhy::EndTx()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_IF((m_trxState != WbanPhyState::PHY_BUSY_TX) &&
                (m_trxState != WbanPhyState::PHY_TRX_OFF));

    if (!static_cast<bool>(m_currentTxPacket.second))
    {
        NS_LOG_DEBUG("Packet successfully transmitted");
        m_phyTxEndTrace(m_currentTxPacket.first);
        if (!m_phyDataConfirmCallback.IsNull())
        {
            m_phyDataConfirmCallback(WbanPhyState::PHY_SUCCESS);
        }
    }
    else
    {
        NS_LOG_DEBUG("Packet transmission aborted");
        m_phyTxDropTrace(m_currentTxPacket.first);
        if (!m_phyDataConfirmCallback.IsNull())
        {
            // See if this is ever entered in another state
            NS_ASSERT(m_trxState == WbanPhyState::PHY_TRX_OFF);
            m_phyDataConfirmCallback(m_trxState);
        }
    }
    m_currentTxPacket.first = nullptr;
    m_currentTxPacket.second = false;

    // We may be waiting to apply a pending state change.
    if (m_trxStatePending != WbanPhyState::PHY_IDLE)
    {
        // Only change the state immediately, if the transceiver is not already
        // switching the state.
        if (!m_setTRXState.IsPending())
        {
            NS_LOG_LOGIC("Apply pending state change to " << m_trxStatePending);
            ChangeTrxState(m_trxStatePending);
            m_trxStatePending = WbanPhyState::PHY_IDLE;
            if (!m_phySetTRXStateConfirmCallback.IsNull())
            {
                m_phySetTRXStateConfirmCallback(WbanPhyState::PHY_SUCCESS);
            }
        }
    }
    else
    {
        if (m_trxState != WbanPhyState::PHY_TRX_OFF)
        {
            ChangeTrxState(WbanPhyState::PHY_TX_ON);
        }
    }
}

Time
WbanPhy::CalculateTxTime(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    // The preambles for all the sub-bands in the NarrowBand are 90 bits
    int preambleBits = 90;
    Time preambleTxTime = Seconds(preambleBits / GetPreambleDataOrSymbolRate(false));

    // The time of the rest of the packet (psdu)
    Time psduTxTime = Seconds((packet->GetSize() * 8.0) / GetPsduDataOrSymbolRate(false));
    Time txTime = GetPlcpHeaderTxTime() + preambleTxTime + psduTxTime;
    NS_LOG_DEBUG("tx time = " << txTime);
    return (txTime);
}

double
WbanPhy::GetPreambleDataOrSymbolRate(bool isData)
{
    auto phyOption = static_cast<uint32_t>(m_phyOption);
    NS_LOG_FUNCTION(this << isData);
    double preRate = 0.0;

    if (isData)
    {
        preRate = ModulationParameters[phyOption].preambleBitRate;
    }
    else
    {
        preRate = ModulationParameters[phyOption].symbolRate;
    }
    return (preRate * 1000.0);
}

double
WbanPhy::GetPsduDataOrSymbolRate(bool isData)
{
    NS_LOG_FUNCTION(this << isData);
    double rate = 0.0;
    auto phyOption = static_cast<uint32_t>(m_phyOption);

    if (isData)
    {
        rate = ModulationParameters[phyOption].bitRate;
    }
    else
    {
        rate = ModulationParameters[phyOption].symbolRate;
    }
    return (rate * 1000.0);
}

Time
WbanPhy::GetPlcpHeaderTxTime()
{
    NS_LOG_FUNCTION(this);
    // NS_LOG_DEBUG ("GET DATA" << GetPreambleDataOrSymbolRate (true));
    // PLCP Header uses the same data rate as the preamble and all the plcp headers
    // in the narrowBand have the same size
    // plcpHeaderBits = PHY header (15 bits) + HCS (4 bits) + BCH parity bits (12 bits)
    // GetSerializedSize considers only 24 bits of the header; hence we need to add more 7 bits of
    // the header here to make a complete 31 bits

    int plcpHeaderBits = 7;
    Time plcpTxTime = Seconds(plcpHeaderBits / GetPreambleDataOrSymbolRate(false));

    return (plcpTxTime);
}

double
WbanPhy::GetPsduDataRate(bool isData) const
{
    auto phyOption = static_cast<uint32_t>(m_phyOption);
    double dataRate = 0.0;
    // NS_ASSERT (m_phyOption < NB_INVALID_BAND);
    if (isData)
    {
        dataRate = (((ModulationParameters[phyOption].symbolRate *
                      log2(ModulationParameters[phyOption].m)) /
                     ModulationParameters[phyOption].s) *
                    ModulationParameters[phyOption].k / ModulationParameters[phyOption].n);
    }
    else
    {
        NS_FATAL_ERROR("invalid frequency selection for data rate calculation");
    }
    return (dataRate * 1000.0);
    NS_LOG_DEBUG("data rate = " << dataRate << "band option  = " << m_phyOption);
}

// IEEE802.15.6 Table 45 in section 8.6.2
void
WbanPhy::SetPhyOptions(WbanPhyOption m_phyOption)
{
    NS_LOG_FUNCTION(this);

    using enum WbanPhyOption;

    auto phyOption = static_cast<uint32_t>(m_phyOption);
    switch (phyOption)
    {
    case 0:
        m_phyPIBAttributes.phyCurrentChannel = 1;
        // IEEE 802.15.6 402 to 405 Mhz DBPSK Channels 0 to 9
        if (m_phyPIBAttributes.phyCurrentChannel <=
            (ModulationParameters[phyOption].channel)) // change to 9
        {
            NS_LOG_DEBUG("current channel set for  NB_402_MHZ_75_9 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_402_MHZ_75_9  band; Select channel "
                           "between 0-9 or change the band");
        }
        SetRxSensitivity(-119.20);
        break;
    case 1:
        m_phyPIBAttributes.phyCurrentChannel = 1;
        // IEEE 802.15.6 402 to 405 Mhz DBPSK Channels 0 to 9
        if (m_phyPIBAttributes.phyCurrentChannel <=
            (ModulationParameters[phyOption].channel)) // change to 9
        {
            NS_LOG_DEBUG("current channel set for NB_402_MHZ_151_8 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_402_MHZ_151_8 band; Select channel "
                           "between 0-9 or change the band");
        }
        SetRxSensitivity(-117.95);
        break;
    case 2:
        // IEEE 802.15.6 863 to 870 Mhz DBPSK Channels 0 to 11
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_863_MHZ_101_2 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_863_MHZ_101_2 band; Select channel "
                           "between 0-11 or change the band");
        }
        SetRxSensitivity(-117.95);
        break;
    case 3:
        // IEEE 802.15.6 863 to 870 Mhz DBPSK Channels 0 to 11
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_863_MHZ_202_4 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_863_MHZ_202_4 band; Select channel "
                           "between 0-11 or change the band");
        }
        SetRxSensitivity(-117.95);
        break;
    case 4:
        // IEEE 802.15.6 902 to 928 Mhz DBPSK Channels 0 to 13
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_902_MHZ_101_2 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_902_MHZ_101_2 band; Select channel "
                           "between 0-13 or change the band");
        }
        SetRxSensitivity(-117.95);
        break;
    case 5:
        // IEEE 802.15.6 902 to 928 Mhz DBPSK Channels 0 to 13
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_902_MHZ_202_4 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_902_MHZ_202_4 band; Select channel "
                           "between 0-13 or change the band");
        }
        SetRxSensitivity(-117.95);
        break;
    case 6:
        // IEEE 802.15.6 950 to 958 Mhz DBPSK Channels 0 to 15
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_950_MHZ_101_2 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_950_MHZ_101_2 band; Select channel "
                           "between 0-15 or change the band");
        }
        SetRxSensitivity(-117.95);
        break;
    case 7:
        // IEEE 802.15.6 950 to 958 Mhz DBPSK Channels 0 to 15
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_950_MHZ_202_4 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_950_MHZ_202_4 band; Select channel "
                           "between 0-15 or change the band");
        }
        SetRxSensitivity(-117.95);
        break;
    case 8:
        // IEEE 802.15.6 2360 to 2400 Mhz DBPSK Channels 0 to 38
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_2360_MHZ_121_4 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_2360_MHZ_121_4 band; Select channel "
                           "between 0-38 or change the band");
        }
        SetRxSensitivity(-113.97);
        break;
    case 9:
        // IEEE 802.15.6 2360 to 2400 Mhz DBPSK Channels 0 to 38
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_2360_MHZ_242_9 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_2360_MHZ_242_9 band; Select channel "
                           "between 0-38 or change the band");
        }
        SetRxSensitivity(-113.97);
        break;
    case 10:
        // IEEE 802.15.6 863 to 870 Mhz DBPSK Channels 0 to 78
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_2400_MHZ_121_4 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_2400_MHZ_121_4 band; Select channel "
                           "between 0-78 or change the band");
        }
        SetRxSensitivity(-113.97);
        break;
    case 11:
        // IEEE 802.15.6 863 to 870 Mhz DBPSK Channels 0 to 78
        m_phyPIBAttributes.phyCurrentChannel = 1;
        if (m_phyPIBAttributes.phyCurrentChannel <= (ModulationParameters[phyOption].channel))
        {
            NS_LOG_DEBUG("current channel set for NB_2400_MHZ_242_9 band");
        }
        else
        {
            NS_FATAL_ERROR("This channel does not exist for NB_2400_MHZ_242_9 band; Select channel "
                           "between 0-78 or change the band");
        }

        SetRxSensitivity(-113.97);
        break;
    case 12:
        NS_LOG_DEBUG("NB_INVALID_BAND, please select a band from WbanPhyOptions");
        break;
    default:
        NS_LOG_DEBUG("No such NarrowBand PHY option");
        break;
    }

    NS_LOG_DEBUG("band  = " << phyOption);
}

WbanPhyOption
WbanPhy::GetPhyOption() const
{
    return m_phyOption;
}

void
WbanPhy::SetRxSensitivity(double dbmSensitivity)
{
    auto phyOption = static_cast<uint32_t>(m_phyOption);
    NS_LOG_FUNCTION(this << dbmSensitivity << "dBm");
    if (dbmSensitivity > (ModulationParameters[phyOption].sensitivity))
    {
        NS_ABORT_MSG("The maximum Rx sensitivity for this band should be as defined in modulation "
                     "parameters");
    }

    //  Calculate the noise factor required to reduce the Rx sensitivity.
    //  The maximum possible sensitivity in the current modulation is used as a reference
    //  After Rx sensitivity is set, this becomes the new point where PER < 10 % for a
    //  PSDU of 255 bytes as described by the standard.
    double maxRxSensitivityW = DbmToW(dbmSensitivity);
    WbanSpectrumValueHelper psdHelper;
    m_txPsd = psdHelper.CreateTxPowerSpectralDensity(
        GetNominalTxPowerFromPib(m_phyPIBAttributes.phyTransmitPower),
        m_phyPIBAttributes.phyCurrentChannel);
    // Update thermal noise + noise factor added.
    long double noiseFactor = DbmToW(dbmSensitivity) / maxRxSensitivityW;
    psdHelper.SetNoiseFactor(noiseFactor);
    m_noise = psdHelper.CreateNoisePowerSpectralDensity(m_phyPIBAttributes.phyCurrentChannel);
    m_signal = Create<WbanInterferenceHelper>(m_noise->GetSpectrumModel());
    // Change receiver sensitivity from dBm to Watts
    m_rxSensitivity = DbmToW(dbmSensitivity);
}

double
WbanPhy::GetRxSensitivity()
{
    NS_LOG_FUNCTION(this);
    // Change receiver sensitivity from Watt to dBm
    return WToDbm(m_rxSensitivity);
}

void
WbanPhy::SetTxPowerSpectralDensity(Ptr<SpectrumValue> txPsd)
{
    NS_ASSERT(txPsd);
    m_txPsd = txPsd;
    NS_LOG_INFO("\t computed tx_psd: " << *txPsd << "\t stored tx_psd: " << *m_txPsd);
}

void
WbanPhy::SetNoisePowerSpectralDensity(Ptr<const SpectrumValue> noisePsd)
{
    NS_LOG_FUNCTION(this << noisePsd);
    NS_LOG_INFO("\t computed noise_psd: " << *noisePsd);
    NS_ASSERT(noisePsd);
    m_noise = noisePsd;
}

Ptr<const SpectrumValue>
WbanPhy::GetNoisePowerSpectralDensity()
{
    NS_LOG_FUNCTION(this);
    return m_noise;
}

int8_t
WbanPhy::GetNominalTxPowerFromPib(uint8_t phyTransmitPower)
{
    NS_LOG_FUNCTION(this << +phyTransmitPower);
    // The nominal Tx power is stored in the PIB as a 6-bit
    // twos-complement, signed number.
    // The 5 LSBs can be copied - as their representation
    // is the same for unsigned and signed integers.
    int8_t nominalTxPower = phyTransmitPower & 0x1F;
    // Now check the 6th LSB (the "sign" bit).
    // It's a twos-complement format, so the "sign"
    // bit represents -2^5 = -32.
    if (phyTransmitPower & 0x20)
    {
        nominalTxPower -= 32;
    }
    return nominalTxPower;
}

double
WbanPhy::WToDbm(double watt)
{
    return (10 * log10(1000 * watt));
}

double
WbanPhy::DbmToW(double dbm)
{
    return (pow(10.0, dbm / 10.0) / 1000.0);
}

int64_t
WbanPhy::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this);
    m_random->SetStream(stream);
    return 1;
}
} // namespace wban
} // namespace ns3
