/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#ifndef WBAN_CHANNEL_H
#define WBAN_CHANNEL_H

#include "ns3/channel.h"

namespace ns3
{

class NetDevice;
class PropagationLossModel;
class PropagationDelayModel;
class Packet;
class Time;

namespace wban
{
class WbanPhy;

/**
 * @brief a channel to interconnect ns3::YansWifiPhy objects.
 * @ingroup wifi
 *
 * This class is expected to be used in tandem with the ns3::YansWifiPhy
 * class and supports an ns3::PropagationLossModel and an
 * ns3::PropagationDelayModel.  By default, no propagation models are set;
 * it is the caller's responsibility to set them before using the channel.
 */
class WbanChannel : public Channel
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    WbanChannel();
    ~WbanChannel() override;

    std::size_t GetNDevices() const override;
    Ptr<NetDevice> GetDevice(std::size_t i) const override;

    /**
     * @param loss the new propagation loss model.
     */
    void SetWbanPropagationModel(const Ptr<PropagationLossModel> loss);

    /**
     * Assign a fixed random variable stream number to the random variables
     * used by this model.  Return the number of streams (possibly zero) that
     * have been assigned.
     *
     * @param stream first stream index to use
     *
     * @return the number of stream indices assigned by this model
     */
    int64_t AssignStreams(int64_t stream);

  private:
    /**
     * A vector of pointers to YansWifiPhy.
     */
    typedef std::vector<Ptr<WbanPhy>> PhyList;

    PhyList m_phyList;                //!< List of YansWifiPhys connected to this YansWifiChannel
    Ptr<PropagationLossModel> m_loss; //!< Propagation loss model
};
} // namespace wban
} // namespace ns3

#endif /* YANS_WIFI_CHANNEL_H */
