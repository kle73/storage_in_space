/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#ifndef LR_WPAN_HELPER_H
#define LR_WPAN_HELPER_H

#include "ns3/node-container.h"
#include "ns3/trace-helper.h"
#include "ns3/wban-phy.h"

namespace ns3
{

class SpectrumChannel;
class MobilityModel;

/**
 * @ingroup wban
 *
 * @brief helps to manage and create IEEE 802.15.6 NetDevice objects
 *
 * This class can help to create IEEE 802.15.6 NetDevice objects
 * and to configure their attributes during creation.  It also contains
 * additional helper functions used by client code.
 *
 * Only one channel is created, and all devices attached to it.  If
 * multiple channels are needed, multiple helper objects must be used,
 * or else the channel object must be replaced.
 */

class WbanHelper
{
  public:
    /**
     * @brief Create a Wban helper in an empty state.
     */
    WbanHelper();

    /**
     * @brief Create a LrWpan helper in an empty state.
     * @param useMultiModelSpectrumChannel use a MultiModelSpectrumChannel if true, a
     * SingleModelSpectrumChannel otherwise
     */
    WbanHelper(bool useMultiModelSpectrumChannel);

    ~WbanHelper();

    // Delete copy constructor and assignment operator to avoid misuse
    WbanHelper(const WbanHelper&) = delete;
    WbanHelper& operator=(const WbanHelper&) = delete;

    /**
     * @brief Get the channel associated to this helper
     * @returns the channel
     */
    Ptr<SpectrumChannel> GetChannel();

    /**
     * @brief Set the channel associated to this helper
     * @param channel the channel
     */
    void SetChannel(Ptr<SpectrumChannel> channel);

    /**
     * @brief Set the channel associated to this helper
     * @param channelName the channel name
     */
    void SetChannel(std::string channelName);

    /**
     * @tparam Ts \deduced Argument types
     * @param name the name of the model to set
     * @param [in] args Name and AttributeValue pairs to set.
     *
     * Add a propagation loss model to the set of currently-configured loss models.
     */
    template <typename... Ts>
    void AddPropagationLossModel(std::string name, Ts&&... args);

    /**
     * @tparam Ts \deduced Argument types
     * @param name the name of the model to set
     * @param [in] args Name and AttributeValue pairs to set.
     *
     * Add a propagation loss model to the set of currently-configured loss models.
     */
    template <typename... Ts>
    void Add(std::string name, Ts&&... args);

    /**
     * @tparam Ts \deduced Argument types
     * @param name the name of the model to set
     * @param [in] args Name and AttributeValue pairs to set.
     *
     * Configure a propagation delay for this channel.
     */
    template <typename... Ts>
    void SetPropagationDelayModel(std::string name, Ts&&... args);

    /**
     * @brief Add mobility model to a physical device
     * @param phy the physical device
     * @param m the mobility model
     */
    void AddMobility(Ptr<wban::WbanPhy> phy, Ptr<MobilityModel> m);

    /**
     * @brief Install a WbanNetDevice and the associated structures (e.g., channel) in the nodes.
     *
     * If the channel is not already initialized, it will be created as either a
     * SingleModelSpectrumChannel or a MultiModelSpectrumChannel, depending on the
     * useMultiModelSpectrumChannel flag. Additionally, a ConstantSpeedPropagationDelayModel will be
     * set as the default delay model if no delay model is specified, and a
     * LogDistancePropagationLossModel will be added to the channel if no propagation loss models
     * are defined.
     *
     * If the channel is already initialized but lacks either a PropagationDelayModel or a
     * PropagationLossModel, an error will be raised.
     * @param c a set of nodes
     * @returns A container holding the added net devices.
     */
    NetDeviceContainer Install(NodeContainer c);
    /**
     * Helper to enable all Wban log components with one statement
     */
    void EnableLogComponents();
    /**
     * Assign a fixed random variable stream number to the random variables
     * used by this model. Return the number of streams that have been
     * assigned. The Install() method should have previously been
     * called by the user.
     *
     * @param c NetDeviceContainer of the set of net devices for which the
     *          CsmaNetDevice should be modified to use a fixed stream
     * @param stream first stream index to use
     * @return the number of stream indices assigned by this helper
     */
    int64_t AssignStreams(NetDeviceContainer c, int64_t stream);

  private:
    Ptr<SpectrumChannel> m_channel;      //!< channel to be used for the devices
    bool m_useMultiModelSpectrumChannel; //!< indicates whether a MultiModelSpectrumChannel is used
    std::vector<ObjectFactory> m_propagationLoss; ///< vector of propagation loss models
    ObjectFactory m_propagationDelay;             ///< propagation delay model
};

/***************************************************************
 *  Implementation of the templates declared above.
 ***************************************************************/

template <typename... Ts>
void
WbanHelper::AddPropagationLossModel(std::string name, Ts&&... args)
{
    m_propagationLoss.push_back(ObjectFactory(name, std::forward<Ts>(args)...));
}

template <typename... Ts>
void
WbanHelper::SetPropagationDelayModel(std::string name, Ts&&... args)
{
    m_propagationDelay = ObjectFactory(name, std::forward<Ts>(args)...);
}

} // namespace ns3

#endif /* LR_WPAN_HELPER_H */
