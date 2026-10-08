/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Satyam Shukla <shuklasatyam774@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */
#ifndef QUANTUM_DEVICE_H
#define QUANTUM_DEVICE_H

#include "ns3/quantum-helper.h"
#include "ns3/simple-net-device.h"

namespace ns3
{

class QuantumChannel;

/**
 * @ingroup quantum
 * @class QuantumDevice
 * @brief This class represents a Quantum device in ns-3.
 *
 * A QuantumDevice handles sending and receiving data through
 * quantum and classical channels.
 */
class QuantumDevice : public SimpleNetDevice
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId(void);

    /**
     * @brief QuantumDevice destructor.
     */
    ~QuantumDevice();

    /**
     * @brief Attach a quantum channel to this device.
     * @param channel the QuantumChannel to attach
     */
    void SetQuantumChannel(Ptr<QuantumChannel> channel);

    /**
     * @brief Retrieve the attached quantum channel.
     * @return the QuantumChannel pointer
     */
    Ptr<QuantumChannel> GetQuantumChannel() const;

    /**
     * @brief Handle receiving a qubit from the quantum channel.
     * @param qbit the received QBit
     * @param sourceDeviceAddress the address of the sender
     */
    virtual void ReceiveQubit(Ptr<QBit> qbit, Mac48Address sourceDeviceAddress) = 0;

    /**
     * @brief Send a qubit after a delay.
     * @param qbit the qubit to send
     * @param sourceDeviceAddress the source device address
     */
    virtual void SendQubit(Ptr<QBit> qbit, Mac48Address sourceDeviceAddress) = 0;

  protected:
    QuantumHelper m_helper;        //!< Helper for quantum operations
    Ptr<QuantumChannel> m_channel; //!< Pointer to the attached quantum channel
};

} // namespace ns3

#endif // QUANTUM_DEVICE_H
