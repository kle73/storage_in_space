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

#ifndef AQM_EVAL_SUITE_HELPER_H
#define AQM_EVAL_SUITE_HELPER_H

#include "ns3/eval-topology.h"

namespace ns3
{

/**
 * @brief An abstract class for scenario implementation
 *
 * This class contains the definition of methods commonly used in all
 * scenarios, and virtual declaration of scenario-specific methods.
 */
class ScenarioImpl
{
  public:
    /**
     * @brief Constructor
     */
    ScenarioImpl();

    /**
     * @brief Destructor
     */
    virtual ~ScenarioImpl();

    /**
     * @brief Run simulation for the specified duration
     *
     * @param simtime The simulation time
     * @param isBql Enable/Disable Byte Queue Limits
     */
    void RunSimulation(Time simtime, bool isBql);

    /**
     * @brief Adds the AQM to m_AQM list
     *
     * @param aqm TypeID of the AQM algorithm
     */
    void addAQM(std::string aqm);

    /**
     * @brief Helper to disconnect trace sources
     *
     * @param et The EvaluationTopology for which trace sources are disconnected
     */
    void DestroyTrace(EvaluationTopology et);

    /**
     * @brief Configure the queue discipline
     *
     * @param limit Maximum capacity of the queue discipline
     * @param pktsize Packet size
     * @param linkbw Bottleneck link bandwidth
     * @param linkdel Bottleneck link delay
     * @param mode Mode of operation for QueueDisc
     * @param ecn Enable/Disable Explicit Congestion Notification
     */
    virtual void ConfigureQueueDisc(uint32_t limit,
                                    uint32_t pktsize,
                                    std::string linkbw,
                                    std::string linkdel,
                                    std::string mode,
                                    bool ecn);

    /**
     * @brief Set the base output directory for results
     *
     * @param baseOutputDir The base output directory path
     */
    void SetBaseOutputDir(const std::string& baseOutputDir);

  protected:
    /**
     * @brief Create simulation scenario
     *
     * @param aqm TypeID of the AQM algorithm
     * @param isBql Enable/Disable Byte Queue Limits
     *
     * @return EvaluationTopology object representing the scenario
     */
    virtual EvaluationTopology CreateScenario(std::string aqm, bool isBql) = 0;

    /**
     * @brief Get the base output directory
     *
     * @return The base output directory path
     */
    std::string GetBaseOutputDir() const;

  private:
    std::vector<std::string> m_AQM; //!< List of AQM algorithms
    uint32_t m_nAQM;                //!< Number of AQM algorithms
    std::string m_baseOutputDir;    //!< Base output directory for results
};

} // namespace ns3

#endif /* AQM_EVAL_SUITE_HELPER_H */
