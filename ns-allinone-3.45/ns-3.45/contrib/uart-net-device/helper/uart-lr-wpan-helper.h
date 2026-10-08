/*
 * Copyright (c) 2025 Tokushima University, Japan
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */
#ifndef UART_LR_WPAN_HELPER_H
#define UART_LR_WPAN_HELPER_H

#include "ns3/node-container.h"
#include "ns3/trace-helper.h"

namespace ns3
{

/**
 * @ingroup uart
 *
 * @brief helps to manage and create IEEE 802.15.4 UART NetDevice objects
 *
 * This class can help to create IEEE 802.15.4 UART NetDevice objects
 * and relate them to real MCUs (IEEE 802.15.4 PHY/MAC stack) physically
 * connected to a Host.
 *
 */

class UartLrWpanHelper : public PcapHelperForDevice
{
  public:
    /**
     * @brief UartLrWpan helper constructor.
     */
    UartLrWpanHelper();

    ~UartLrWpanHelper() override;

    /**
     * @brief Install UartLrWpanNetDevices and associate them to each node.
     *
     * This function will create a new UartLrWpanNetDevice for each of the nodes in
     * the NodeContainer. Internally, each one of the newly created NetDevices will
     * be associated to a serial port with the prefix /dev/ttyUSBx. Where x is replaced
     * by a number in ascending order starting from the number 0.
     *
     * If there is no physical device on the assigned port, an error is issued on
     * run time.
     *
     * @param c a set of nodes
     * @returns A container holding the added net devices.
     */
    NetDeviceContainer Install(NodeContainer c);

    /**
     * @brief Enable pcap output on the indicated net device.
     *
     * NetDevice-specific implementation mechanism for hooking the trace and
     * writing to the trace file.
     *
     * @param prefix Filename prefix to use for pcap files.
     * @param nd Net device for which you want to enable tracing.
     * @param promiscuous If true capture all possible packets available at the device.
     * @param explicitFilename Treat the prefix as an explicit filename if true
     */
    void EnablePcapInternal(std::string prefix,
                            Ptr<NetDevice> nd,
                            bool promiscuous,
                            bool explicitFilename) override;
};

} // namespace ns3

#endif /* UART_LR_WPAN_HELPER_H */
