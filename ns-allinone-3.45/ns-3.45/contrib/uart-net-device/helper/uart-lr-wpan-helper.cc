/*
 * Copyright (c) 2025 Tokushima University, Japan
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */
#include "uart-lr-wpan-helper.h"

#include "ns3/log.h"
#include "ns3/uart-lr-wpan-net-device.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UartLrWpanHelper");

UartLrWpanHelper::UartLrWpanHelper()
{
}

UartLrWpanHelper::~UartLrWpanHelper()
{
}

NetDeviceContainer
UartLrWpanHelper::Install(NodeContainer c)
{
    NetDeviceContainer devices;
    for (auto i = c.Begin(); i != c.End(); i++)
    {
        Ptr<Node> node = *i;
        size_t index = std::distance(c.Begin(), i);
        std::string port = "/dev/ttyUSB" + std::to_string(index);

        Ptr<uart::UartLrWpanNetDevice> uartNetDevice =
            CreateObject<uart::UartLrWpanNetDevice>(port);

        NS_LOG_DEBUG("Adding UartLrWpanNetDevice on port "
                     << port << " to Node with ID: " << node->GetId());

        node->AddDevice(uartNetDevice);
        // uartNetDevice->SetNode(node);
        devices.Add(uartNetDevice);
    }
    return devices;
}

void
UartLrWpanHelper::EnablePcapInternal(std::string prefix,
                                     Ptr<NetDevice> nd,
                                     bool promiscuous,
                                     bool explicitFilename)
{
}

} // namespace ns3
