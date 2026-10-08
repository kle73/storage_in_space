/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_APP_HELPER_H
#define QKD_APP_HELPER_H

#include "ns3/address.h"
#include "ns3/application-helper.h"
#include "ns3/qkd-device.h"

#include <vector>

namespace ns3
{
/**
 * @ingroup quantum
 * Helper to make it easier to instantiate a QkdKeyManagerApplication on a set of nodes.
 */
class QkdKeyManagerApplicationHelper : public ApplicationHelper
{
  public:
    /**
     * Create a QkdKeyManagerApplicationHelper to make it easier to work with
     * QkdKeyManagerApplication applications.
     * @param local The address of the local Key Manager Application (KMA).
     * @param peerKmaAddresses List of addresses of the Peer KMA applications
     */
    QkdKeyManagerApplicationHelper(const Address& local,
                                   const std::vector<Address> peerKmaAddresses,
                                   std::vector<Ptr<QkdDevice>> qkdDevices = {});

    /**
     * Install the QkdKeyManagerApplication on the given nodes.
     * @param c NodeContainer of the set of nodes on which an application will be installed.
     * @return Container of Ptr to the applications installed.
     */
    ApplicationContainer Install(Ptr<Node> node);

  private:
    std::vector<Address> m_peerKmaAddresses;  //!< Addresses of the Peer KMAs to connect to.
    std::vector<Ptr<QkdDevice>> m_qkdDevices; //!< List of QKD devices associated with this KMA
};

/**
 * @ingroup quantum
 * Helper to make it easier to instantiate a QkdSecureSender on a set of nodes.
 */
class QkdSecureSenderHelper : public ApplicationHelper
{
  public:
    /**
     * Create a QkdSecureSenderHelper to make it easier to work with
     * QkdSecureSender applications.
     * @param remote The address of the remote receiver to which the QKD sender will send packets.
     * @param local The local address of the QKD sender.
     * @param remoteKMA The address of the Key Manager Application (KMA) to which the QKD sender
     * will send packets.
     */
    QkdSecureSenderHelper(const Address& remote, const Address& local, const Address& remoteKMA);
};

/**
 * @ingroup quantum
 * Helper to make it easier to instantiate a QkdSecureReceiver on a set of nodes.
 */
class QkdSecureReceiverHelper : public ApplicationHelper
{
  public:
    /**
     * Create a QkdSecureReceiverHelper to make it easier to work with
     * QkdSecureReceiver applications.
     * @param local The local address of the QKD receiver.
     * @param remoteKMA The address of the Key Manager Application (KMA) to which the QKD receiver
     * will send packets.
     */
    QkdSecureReceiverHelper(const Address& local, const Address& remoteKMA);
};
} // namespace ns3

#endif /* QKD_APP_HELPER_H */
