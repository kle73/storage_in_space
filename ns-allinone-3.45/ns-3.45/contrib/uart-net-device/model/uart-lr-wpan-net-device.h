/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *    Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */
#ifndef UART_LR_WPAN_NET_DEVICE_H
#define UART_LR_WPAN_NET_DEVICE_H

#include "uart-lr-wpan-mac.h"

#include "ns3/net-device.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class Node;

namespace uart
{

/**
 * A special NetDevice to establish communication with Lr-Wpan hardware devices (physical devices)
 * compliant with devices implementing IEEE 802.15.4-2006 and IEEE 802.15.4-2011. This NetDevices
 * establishes a UART serial connection with a physical device described by the provided port.
 * Physical devices communicating with this NetDevice are required to run the designed Shim layer
 * (companion application installed in the MCU) in order to achieve this co-processor service.
 *
 */
class UartLrWpanNetDevice : public NetDevice
{
  public:
    /**
     * Get the type ID.
     *
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    UartLrWpanNetDevice();

    /**
     * The Uart NetDevice constructor with port device parameter used to establish a serial
     * connection with a NXP JN516x device.
     *
     * @param port The physical port identifier of the device to which this class instance will
     * connect.
     */
    UartLrWpanNetDevice(std::string port);
    ~UartLrWpanNetDevice() override;

    /**
     * Set the MAC to be used by this NetDevice.
     *
     * @param mac the MAC to be used
     */
    void SetMac(Ptr<UartLrWpanMac> mac);

    /**
     * Get the MAC used by this NetDevice.
     *
     * @return the MAC object
     */
    Ptr<UartLrWpanMac> GetMac() const;

    /**
     * This method is use to manually configure the coordinator through
     * which the device or coordinator is associated. When assigning a short address
     * the extended address must also be present.
     *
     * @param panId The id of the PAN used by the coordinator device.
     *
     * @param coordShortAddr The coordinator assigned short address through which this
     *                       device or coordinator is associated.
     *                       [FF:FF] address indicates that the value is unknown.
     *                       [FF:FE] indicates that the associated coordinator is using only
     *                       its extended address.
     *
     * @param assignedShortAddr The assigned short address for this device.
     *                          [FF:FF] address indicates that the device have no short address
     *                                  and is not associated.
     *                          [FF:FE] address indicates that the devices has associated but
     *                          has not been allocated a short address.
     */
    void SetPanAssociation(uint16_t panId,
                           Mac16Address coordShortAddr,
                           Mac16Address assignedShortAddr);

    // From class NetDevice
    void SetIfIndex(const uint32_t index) override;
    uint32_t GetIfIndex() const override;
    Ptr<Channel> GetChannel() const override;
    void SetAddress(Address address) override;
    Address GetAddress() const override;
    bool SetMtu(const uint16_t mtu) override;
    uint16_t GetMtu() const override;
    bool IsLinkUp() const override;
    void AddLinkChangeCallback(Callback<void> callback) override;
    bool IsBroadcast() const override;
    Address GetBroadcast() const override;
    bool IsMulticast() const override;
    Address GetMulticast(Ipv4Address multicastGroup) const override;
    Address GetMulticast(Ipv6Address addr) const override;
    bool IsBridge() const override;
    bool IsPointToPoint() const override;
    bool Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber) override;
    bool SendFrom(Ptr<Packet> packet,
                  const Address& source,
                  const Address& dest,
                  uint16_t protocolNumber) override;
    Ptr<Node> GetNode() const override;
    void SetNode(Ptr<Node> node) override;
    bool NeedsArp() const override;

    void SetReceiveCallback(NetDevice::ReceiveCallback cb) override;
    void SetPromiscReceiveCallback(PromiscReceiveCallback cb) override;
    bool SupportsSendFrom() const override;

  private:
    // Inherited from NetDevice/Object
    void DoDispose() override;
    void DoInitialize() override;

    /**
     * Mark NetDevice link as up.
     */
    void LinkUp();

    /**
     * Mark NetDevice link as down.
     */
    void LinkDown();

    /**
     * IEEE 802.15.4-2011 section 6.2.11.2
     * MLME-SET.confirm
     * Reports the result of an attempt to change a MAC PIB attribute.
     *
     * @param params The MLME-SET.confirm params
     */
    void MlmeSetConfirm(lrwpan::MlmeSetConfirmParams params);

    /**
     * The MAC for this NetDevice.
     */
    Ptr<UartLrWpanMac> m_mac;

    /**
     * The node associated with this NetDevice.
     */
    Ptr<Node> m_node;

    /**
     * Is the link/device currently up and running?
     */
    bool m_linkUp;

    /**
     * The interface index of this NetDevice.
     */
    uint32_t m_ifIndex;

    /**
     * Trace source for link up/down changes.
     */
    TracedCallback<> m_linkChanges;

    /**
     * Upper layer callback used for notification of new data packet arrivals.
     */
    ReceiveCallback m_receiveCallback;
};

} // namespace uart
} // namespace ns3

#endif /* UART_LR_WPAN_NET_DEVICE_H */
