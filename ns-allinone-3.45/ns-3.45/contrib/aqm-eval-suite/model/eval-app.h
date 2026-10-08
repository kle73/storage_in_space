/*
 * Copyright (c) 2017 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Authors: Ankit Deepak <adadeepak8@gmail.com>
 *          Shravya K. S. <shravya.ks0@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef EVAL_APPLICATION_H
#define EVAL_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"

namespace ns3
{

/**
 * @brief Generates traffic in the suite
 *
 * This class generates TCP and UDP traffic in the suite. It has been
 * designed to overcome the challenges faced while configuring attributes
 * of TCP sockets, such as the initial congestion window.
 */
class EvalApp : public Application
{
  public:
    /**
     * @brief Constructor
     */
    EvalApp();

    /**
     * @brief Destructor
     */
    ~EvalApp() override;

    /**
     * @brief Configures the application
     *
     * @param socket The socket to be used by the application
     * @param address Remote address
     * @param packetSize The size of payload
     * @param maxByte The maximum amount of data to be sent by the application
     * @param stream The stream used by RNG
     * @param rate The rate at which application sends data
     */
    void Setup(Ptr<Socket> socket,
               Address address,
               uint32_t packetSize,
               uint64_t maxByte,
               uint64_t stream,
               DataRate rate);

    /**
     * @brief Get the flow completion time
     *
     * @return The flow completion time
     */
    Time GetFlowCompletionTime();

    friend class EvaluationTopology;

  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Starts the application
     */
    void StartApplication() override;

    /**
     * @brief Stops the application
     */
    void StopApplication() override;

    /**
     * @brief Cancel all pending events
     */
    void CancelEvents();

    /**
     * @brief Start an On period
     */

    void StartSending();

    /**
     * @brief Start an Off period
     */
    void StopSending();

    /**
     * @brief Send a packet
     */
    void SendPacket();

    Ptr<Socket> m_socket;                //!< Associated socket
    Address m_peer;                      //!< Peer address
    bool m_connected;                    //!< True if connected
    Ptr<RandomVariableStream> m_onTime;  //!< rng for On Time
    Ptr<RandomVariableStream> m_offTime; //!< rng for Off Time
    DataRate m_cbrRate;                  //!< Rate that data is generated
    DataRate m_cbrRateFailSafe;          //!< Rate that data is generated (check copy)
    uint32_t m_pktSize;                  //!< Size of packets
    uint32_t m_residualBits;             //!< Number of generated, but not sent, bits
    Time m_lastStartTime;                //!< Time last packet sent
    uint64_t m_maxBytes;                 //!< Limit total number of bytes sent
    uint64_t m_totBytes;                 //!< Total bytes sent so far
    EventId m_startStopEvent;            //!< Event id for next start or stop event
    EventId m_sendEvent;                 //!< Event id of pending "send packet" event

  private:
    Time m_flowStart; //!< Flow start time
    Time m_flowStop;  //!< Flow stop time

    /**
     * @brief Schedule the next packet transmission
     */
    void ScheduleNextTx();

    /**
     * @brief Schedule the next On period start
     */
    void ScheduleStartEvent();

    /**
     * @brief Schedule the next Off period start
     */
    void ScheduleStopEvent();

    /**
     * @brief Handle a Connection Succeed event
     *
     * @param socket The connected socket
     */
    void ConnectionSucceeded(Ptr<Socket> socket);

    /**
     * @brief Handle a Connection Failed event
     *
     * @param socket The not connected socket
     */
    void ConnectionFailed(Ptr<Socket> socket);
};

} // namespace ns3

#endif /* EVAL_APPLICATION_H */
