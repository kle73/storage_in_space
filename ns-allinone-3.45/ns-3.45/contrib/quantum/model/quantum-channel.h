/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Satyam Shukla <shuklasatyam774@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QUANTUM_CHANNEL_H
#define QUANTUM_CHANNEL_H

#include "qkd-device.h"

#include <map>

namespace ns3
{
typedef std::pair<Ptr<QuantumDevice>, Time> QuantumDeviceDelayPair;
typedef std::pair<Mac48Address, Ptr<QuantumDevice>> QuantumDeviceIdPair;

/**
 * @ingroup channel
 * @brief A quantum channel, for communication between quantum devices.
 *
 * This channel doesn't check for packet collisions and it
 * does not introduce any noise.
 *
 * It sends the Qubits to all associated QuantumDevices at their specified delays.
 *
 * This channel is meant to be used by ns3::QuantumDevice
 */
class QuantumChannel : public Channel
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    QuantumChannel();

    /**
     * A packet is sent by a qkd device. A receive event will be
     * scheduled for all qkd devices connected to the channel other
     * than the qkd device who sent the packet at their specified delays.
     *
     * @param qbits qbits to be sent
     * @param senderId QuantumDevice who sent the packet
     * @note The senderId is the unique ID of the device.
     */
    virtual void Send(Ptr<QBit> qbit, Mac48Address senderId);

    /**
     * Add a qkd device as an eavesdropper to the channel.
     *
     * @param deviceId the Id of the device to be attached
     * @param Device qkd device to be added
     * @param delay delay for the device
     */
    void AddEavesdropper(Mac48Address deviceId, Ptr<QuantumDevice> device, Time delay = Seconds(0));

    /**
     * Add a trusted qkd device to the channel.
     *
     * @param deviceId the Id of the device.
     * @param device qkd device to be added
     * @param delay delay for the device
     */
    void AddDevice(Mac48Address deviceId, Ptr<QuantumDevice> Device, Time delay = Seconds(0));

    /**
     * Get the device for the device ID connected to the channel.
     *
     * @param deviceId the device id of the device.
     * @return the QuantumDevice corresponding to the Id.
     */
    Time GetDelay(Mac48Address deviceId);

    /**
     * Change the delay for the device ID.
     *
     * @param deviceId the id of the device.
     * @param t the new delay for the device.
     */
    void SetDelay(Mac48Address deviceId, Time t);

    /**
     * Get the device for a device ID.
     *
     * @param deviceId the id of the device.
     * @return the QuantumDevice.
     */
    Ptr<QuantumDevice> GetDevice(Mac48Address deviceId);

    /**
     * Get the number of devices connected to this Channel.
     *
     * @returns the number of devices connected to this Channel.
     */
    std::size_t GetNDevices() const override;

    /**
     * @param i index of NetDevice to retrieve
     * @returns one of the NetDevices connected to this channel.
     *
     * This method must be implemented by subclasses.
     */
    Ptr<NetDevice> GetDevice(std::size_t i) const override;

  private:
    std::pair<QuantumDeviceIdPair, QuantumDeviceIdPair>
        m_devices; //!< pair of trusted devices connected to the channel.
    std::map<Mac48Address, QuantumDeviceDelayPair>
        m_delays;               //!< Map between deviceId and DeviceDelayPair.
    Time m_trustedDevicesDelay; //!< Delay for the second trusted device.
};

} // namespace ns3

#endif /* QUANTUM_CHANNEL_H */
