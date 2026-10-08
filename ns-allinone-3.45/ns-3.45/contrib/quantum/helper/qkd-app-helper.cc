/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Shashank G <shashankgirish07@gmail.com>
 *         Mohit P. Tahiliani <ahiliani@nitk.edu.in>
 */

#include "qkd-app-helper.h"

#include "ns3/address-utils.h"
#include "ns3/object-ptr-container.h"
#include "ns3/qkd-key-manager-application.h"

namespace ns3
{

// QKD Key Manager Application Helper Implementation
QkdKeyManagerApplicationHelper::QkdKeyManagerApplicationHelper(
    const Address& local,
    const std::vector<Address> peerKmaAddresses,
    std::vector<Ptr<QkdDevice>> qkdDevices)
    : ApplicationHelper("ns3::QkdKeyManagerApplication"),
      m_peerKmaAddresses(peerKmaAddresses),
      m_qkdDevices(qkdDevices)
{
    m_factory.Set("Local", AddressValue(local));
}

ApplicationContainer
QkdKeyManagerApplicationHelper::Install(Ptr<Node> node)
{
    ApplicationContainer apps = ApplicationHelper::Install(node);
    Ptr<QkdKeyManagerApplication> app = DynamicCast<QkdKeyManagerApplication>(apps.Get(0));
    if (app)
    {
        app->SetDevicesAndPeerKMAs(m_peerKmaAddresses, m_qkdDevices);
    }
    return apps;
}

/// QKD Secure Sender Helper Implementation
QkdSecureSenderHelper::QkdSecureSenderHelper(const Address& remote,
                                             const Address& local,
                                             const Address& remoteKMA)
    : ApplicationHelper("ns3::QkdSecureSender")
{
    m_factory.Set("Remote", AddressValue(remote));
    m_factory.Set("Local", AddressValue(local));
    m_factory.Set("RemoteKMA", AddressValue(remoteKMA));
}

/// QKD Secure Receiver Helper Implementation
QkdSecureReceiverHelper::QkdSecureReceiverHelper(const Address& local, const Address& remoteKMA)
    : ApplicationHelper("ns3::QkdSecureReceiver")
{
    m_factory.Set("Local", AddressValue(local));
    m_factory.Set("RemoteKMA", AddressValue(remoteKMA));
}

} // namespace ns3
