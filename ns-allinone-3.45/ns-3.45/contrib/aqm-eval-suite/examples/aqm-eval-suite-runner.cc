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

#include "ns3/aqm-eval-suite-output-manager.h"
#include "ns3/aqm-eval-suite-plot-manager.h"
#include "ns3/core-module.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AqmEvalSuiteRunner");

/**
 * Configuration structure for AQM evaluation
 */
struct AqmEvalConfig
{
    std::vector<std::string> aqmAlgorithms = {"PfifoFast",
                                              "CoDel",
                                              "FqCoDel",
                                              "Pie",
                                              "FqPie",
                                              "Red",
                                              "AdaptiveRed",
                                              "FengAdaptiveRed",
                                              "NonLinearRed",
                                              "Cobalt",
                                              "FqCobalt"}; //!< List of AQM algorithms to evaluate
    std::string aggressiveTcp = ""; //!< Whether to use aggressive TCP variant
    std::string queueDiscMode = "QUEUE_DISC_MODE_PACKETS"; //!< Queue discipline mode configuration
    std::string isBql = "false"; //!< Whether Byte Queue Limits (BQL) is enabled
    std::string ecn = "false";   //!< Whether Explicit Congestion Notification is enabled
    std::string queueDiscSuffix = "QueueDisc"; //!< Suffix for queue discipline configuration
};

/**
 * Scenario runner class for executing AQM evaluation scenarios
 */
class ScenarioRunner
{
  public:
    /**
     * Constructor for ScenarioRunner
     * @param outputManager Reference to output manager for handling results
     * @param plotManager Reference to plot manager for generating plots
     * @param config Configuration parameters for the evaluation
     */
    ScenarioRunner(OutputManager& outputManager,
                   PlotManager& plotManager,
                   const AqmEvalConfig& config)
        : m_outputManager(outputManager),
          m_plotManager(plotManager),
          m_config(config)
    {
    }

    /**
     * Run a single scenario
     * @param scenarioName Name of the scenario to run
     * @return true if successful, false otherwise
     */
    bool RunScenario(const std::string& scenarioName)
    {
        NS_LOG_FUNCTION(this << scenarioName);

        // Create output directories
        if (!m_outputManager.CreateScenarioDirectories(scenarioName))
        {
            NS_LOG_ERROR("Failed to create directories for scenario: " << scenarioName);
            return false;
        }

        // Build and execute simulation command
        std::string command = BuildSimulationCommand(scenarioName);
        if (!SafeSystemCall(command))
        {
            NS_LOG_ERROR("Failed to execute simulation for scenario: " << scenarioName);
            return false;
        }

        // Generate plots
        if (!m_plotManager.GeneratePlots(scenarioName,
                                         m_config.aqmAlgorithms,
                                         m_config.queueDiscSuffix))
        {
            NS_LOG_WARN("Failed to generate plots for scenario: " << scenarioName);
            // Don't return false as simulation succeeded
        }

        NS_LOG_INFO("Successfully completed scenario: " << scenarioName);
        return true;
    }

    /**
     * Run RTT Fairness scenarios (1-15)
     * @return true if successful, false otherwise
     */
    bool RunRttFairnessScenarios()
    {
        NS_LOG_FUNCTION(this);

        // Create directories for all RTT fairness scenarios
        for (uint32_t i = 1; i <= 15; ++i)
        {
            std::string scenarioName = "RttFairness" + std::to_string(i);
            if (!m_outputManager.CreateScenarioDirectories(scenarioName))
            {
                NS_LOG_ERROR("Failed to create directories for scenario: " << scenarioName);
                return false;
            }
        }

        // Execute single RTT fairness simulation (generates data for all 15 scenarios)
        std::string command = BuildRttFairnessCommand();
        if (!SafeSystemCall(command))
        {
            NS_LOG_ERROR("Failed to execute RTT fairness simulation");
            return false;
        }

        // Generate plots for each RTT fairness scenario
        for (uint32_t i = 1; i <= 15; ++i)
        {
            std::string scenarioName = "RttFairness" + std::to_string(i);
            if (!m_plotManager.GeneratePlots(scenarioName,
                                             m_config.aqmAlgorithms,
                                             m_config.queueDiscSuffix))
            {
                NS_LOG_WARN("Failed to generate plots for scenario: " << scenarioName);
            }
        }

        NS_LOG_INFO("Successfully completed RTT fairness scenarios");
        return true;
    }

  private:
    OutputManager& m_outputManager; //!< Reference to output manager for handling simulation results
    PlotManager& m_plotManager;    //!< Reference to plot manager for generating visualization plots
    const AqmEvalConfig& m_config; //!< Configuration parameters for the AQM evaluation

    /**
     * Build simulation command for a scenario
     * @param scenarioName Name of the scenario
     * @return Command string
     */
    std::string BuildSimulationCommand(const std::string& scenarioName)
    {
        std::string baseCmd = "./ns3 run \"" + scenarioName;

        // Add aggressive TCP variant if needed
        if (!m_config.aggressiveTcp.empty() && scenarioName == "AggressiveTransportSender")
        {
            baseCmd += " --TcpVariant=ns3::" + m_config.aggressiveTcp;
        }

        // Add common parameters
        baseCmd += " --QueueDiscMode=" + m_config.queueDiscMode;
        baseCmd += " --isBql=" + m_config.isBql;
        baseCmd += " --ecn=" + m_config.ecn;
        baseCmd += " --BaseOutputDir=" + m_outputManager.GetBaseOutputPath();
        baseCmd += "\"";

        return baseCmd;
    }

    /**
     * Build command for RTT fairness scenarios
     * @return Command string
     */
    std::string BuildRttFairnessCommand()
    {
        std::string command = "./ns3 run \"RttFairness";
        command += " --QueueDiscMode=" + m_config.queueDiscMode;
        command += " --isBql=" + m_config.isBql;
        command += " --BaseOutputDir=" + m_outputManager.GetBaseOutputPath();
        command += "\"";
        return command;
    }

    /**
     * Execute system command safely
     * @param command Command to execute
     * @return true if successful, false otherwise
     */
    bool SafeSystemCall(const std::string& command)
    {
        int result = system(command.c_str());
        if (result != 0)
        {
            NS_LOG_ERROR("Command failed with exit code " << result << ": " << command);
            return false;
        }
        return true;
    }
};

/**
 * Scenario mapping for RFC numbers to scenario names
 */
static const std::map<std::string, std::string> RFC_SCENARIO_MAP = {
    {"5.1.1", "TCPFriendlySameInitCwnd"},
    {"5.1.2", "TCPFriendlyDifferentInitCwnd"},
    {"5.2", "AggressiveTransportSender"},
    {"5.3.1", "UnresponsiveTransport"},
    {"5.3.2", "UnresponsiveWithFriendly"},
    {"5.4", "LbeTransportSender"},
    {"8.2.2", "MildCongestion"},
    {"8.2.3", "MediumCongestion"},
    {"8.2.4", "HeavyCongestion"},
    {"8.2.5", "VaryingCongestion"},
    {"8.2.6.1", "VaryingBandwidthUno"},
    {"8.2.6.2", "VaryingBandwidthDuo"}};

/**
 * Get all valid scenario names (including special ones)
 * @return Set of all valid scenario names
 */
std::set<std::string>
GetValidScenarioNames()
{
    std::set<std::string> validNames = {"All", "RttFairness"};

    // Add all RFC scenario names
    for (const auto& pair : RFC_SCENARIO_MAP)
    {
        validNames.insert(pair.second);
    }

    return validNames;
}

/**
 * Resolve and validate scenario specification
 * @param scenarioName Input scenario name (may be empty)
 * @param scenarioNumber Input scenario number (may be empty)
 * @param config Configuration for additional validation
 * @return Resolved scenario name, empty string if invalid
 */
std::string
ResolveScenario(const std::string& scenarioName,
                const std::string& scenarioNumber,
                const AqmEvalConfig& config)
{
    std::string resolvedName = scenarioName;

    // If no name provided, try to resolve from number
    if (resolvedName.empty())
    {
        if (scenarioNumber.empty())
        {
            NS_LOG_ERROR("Error: Must specify either scenario name or number");
            return "";
        }

        auto it = RFC_SCENARIO_MAP.find(scenarioNumber);
        if (it == RFC_SCENARIO_MAP.end())
        {
            NS_LOG_ERROR("Error: Invalid scenario number '" << scenarioNumber << "'");
            return "";
        }
        resolvedName = it->second;
    }
    else
    {
        // Validate provided scenario name
        std::set<std::string> validNames = GetValidScenarioNames();
        if (validNames.find(resolvedName) == validNames.end())
        {
            NS_LOG_ERROR("Error: Invalid scenario name '" << resolvedName << "'");
            return "";
        }
    }

    // Validate special cases
    if (resolvedName == "UnresponsiveTransport" && config.ecn == "true")
    {
        NS_LOG_ERROR("Error: ECN cannot be enabled for UnresponsiveTransport scenario (uses UDP)");
        return "";
    }

    return resolvedName;
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("AqmEvalSuiteRunner", LOG_LEVEL_INFO);
    LogComponentEnable("AqmEvalSuiteOutputManager", LOG_LEVEL_INFO);
    LogComponentEnable("AqmEvalSuitePlotManager", LOG_LEVEL_INFO);
    
    // Check plotting dependencies
    if (!PlotManager::CheckPlottingDependencies())
    {
        NS_LOG_ERROR("Dependency check failed. Exiting.");
        exit(-1);
    }

    // Initialize configuration
    AqmEvalConfig config;

    // Parse command line arguments
    std::string scenarioName = "";
    std::string scenarioNumber = "";

    CommandLine cmd(__FILE__);
    cmd.AddValue("number", "Scenario number from RFC", scenarioNumber);
    cmd.AddValue("name", "Name of the scenario (eg: TCPFriendlySameInitCwnd)", scenarioName);
    cmd.AddValue("AggressiveTcp", "Variant of the Aggressive TCP", config.aggressiveTcp);
    cmd.AddValue("QueueDiscMode", "Determines the unit for QueueLimit", config.queueDiscMode);
    cmd.AddValue("isBql", "Enables/Disables Byte Queue Limits", config.isBql);
    cmd.AddValue("ecn", "Enables/Disables ECN", config.ecn);

    cmd.Parse(argc, argv);

    // Resolve and validate scenario
    std::string resolvedScenario = ResolveScenario(scenarioName, scenarioNumber, config);
    if (resolvedScenario.empty())
    {
        exit(-1);
    }
    scenarioName = resolvedScenario;

    // Initialize output and plot managers
    OutputManager outputManager("aqm-eval-output", true);
    if (!outputManager.Initialize())
    {
        NS_LOG_ERROR("Failed to initialize output manager");
        exit(-1);
    }

    PlotManager plotManager(outputManager);

    // Save run configuration
    std::map<std::string, std::string> runConfig = {{"scenarioName", scenarioName},
                                                    {"scenarioNumber", scenarioNumber},
                                                    {"aggressiveTcp", config.aggressiveTcp},
                                                    {"queueDiscMode", config.queueDiscMode},
                                                    {"isBql", config.isBql},
                                                    {"ecn", config.ecn}};

    if (!outputManager.SaveRunConfiguration(runConfig))
    {
        NS_LOG_ERROR("Failed to save run configuration");
        exit(-1);
    }

    // Initialize scenario runner
    ScenarioRunner runner(outputManager, plotManager, config);

    // Display run information
    NS_LOG_INFO("Starting AQM evaluation run at " << outputManager.GetTimestamp());
    NS_LOG_INFO("Output will be saved to: " << outputManager.GetBaseOutputPath());

    // Execute scenarios
    bool success = false;
    if (scenarioName == "All")
    {
        // Run all scenarios
        success = runner.RunRttFairnessScenarios();
        if (success)
        {
            for (const auto& scenario : RFC_SCENARIO_MAP)
            {
                if (scenario.second != "RttFairness")
                {
                    if (!runner.RunScenario(scenario.second))
                    {
                        success = false;
                        break;
                    }
                }
            }
        }
    }
    else if (scenarioName == "RttFairness")
    {
        success = runner.RunRttFairnessScenarios();
    }
    else
    {
        success = runner.RunScenario(scenarioName);
    }

    if (success)
    {
        NS_LOG_INFO("AQM evaluation completed successfully!");
        return 0;
    }
    else
    {
        NS_LOG_ERROR("AQM evaluation failed!");
        return -1;
    }
}
