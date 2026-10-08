/*
 * Copyright (c) 2024 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Authors: David Lin <davidzylin@gmail.com>
 *
 * Plot Manager for AQM Evaluation Suite
 * Handles plot generation and gnuplot script creation
 */

#ifndef AQM_EVAL_SUITE_PLOT_MANAGER_H
#define AQM_EVAL_SUITE_PLOT_MANAGER_H

#include "aqm-eval-suite-output-manager.h"

#include "ns3/core-module.h"

#include <string>
#include <vector>

namespace ns3
{

/**
 * @ingroup aqm-eval-suite
 * @brief Manages plot generation for AQM evaluation scenarios
 */
class PlotManager
{
  public:
    /**
     * Constructor
     * @param outputManager Reference to output manager
     */
    PlotManager(OutputManager& outputManager);

    /**
     * Destructor
     */
    ~PlotManager() = default;

    /**
     * Generate plots for a scenario
     * @param scenarioName Name of the scenario
     * @param aqmAlgorithms List of AQM algorithms
     * @param queueDiscSuffix Suffix for queue disc files
     * @return true if successful, false otherwise
     */
    bool GeneratePlots(const std::string& scenarioName,
                       const std::vector<std::string>& aqmAlgorithms,
                       const std::string& queueDiscSuffix = "QueueDisc");

    /**
     * Process data for plotting (calls Python utilities)
     * @param scenarioName Name of the scenario
     * @param aqmAlgorithm AQM algorithm name
     * @param queueDiscSuffix Suffix for queue disc files
     * @param aqmIndex Index of the AQM algorithm (for label positioning)
     * @return true if successful, false otherwise
     */
    bool ProcessDataForPlotting(const std::string& scenarioName,
                                const std::string& aqmAlgorithm,
                                const std::string& queueDiscSuffix,
                                int aqmIndex);

    /**
     * Create initial gnuplot script with header only
     * @param scenarioName Name of the scenario
     * @return true if successful, false otherwise
     */
    bool CreateInitialGnuplotScript(const std::string& scenarioName);

    /**
     * Append plot command to existing gnuplot script
     * @param scenarioName Name of the scenario
     * @param aqmAlgorithms List of AQM algorithms
     * @param queueDiscSuffix Suffix for queue disc files
     * @return true if successful, false otherwise
     */
    bool AppendPlotCommand(const std::string& scenarioName,
                           const std::vector<std::string>& aqmAlgorithms,
                           const std::string& queueDiscSuffix);

    /**
     * Execute gnuplot script
     * @param scenarioName Name of the scenario
     * @return true if successful, false otherwise
     */
    bool ExecuteGnuplotScript(const std::string& scenarioName);

    /**
     * Convert PNG to EPS format
     * @param scenarioName Name of the scenario
     * @return true if successful, false otherwise
     */
    bool ConvertToEPS(const std::string& scenarioName);

    /**
     * Check if plotting dependencies are available
     * @return true if dependencies are available, false otherwise
     */
    static bool CheckPlottingDependencies();

  private:
    OutputManager& m_outputManager; //!< Reference to output manager
    std::string m_utilsPath;        //!< Path to utilities directory

    /**
     * Execute a system command safely
     * @param command Command to execute
     * @return true if successful, false otherwise
     */
    bool SafeSystemCall(const std::string& command);

    /**
     * Get the gnuplot script header
     * @param scenarioName Name of the scenario
     * @return Gnuplot script header
     */
    std::string GetGnuplotHeader(const std::string& scenarioName);

    /**
     * Build plot command for gnuplot
     * @param scenarioName Name of the scenario
     * @param aqmAlgorithms List of AQM algorithms
     * @param queueDiscSuffix Suffix for queue disc files
     * @return Plot command string
     */
    std::string BuildPlotCommand(const std::string& scenarioName,
                                 const std::vector<std::string>& aqmAlgorithms,
                                 const std::string& queueDiscSuffix);

    /**
     * Get the path to the utilities directory
     * @return Path to utilities directory
     */
    std::string GetUtilsPath() const;

    /**
     * Get the available ImageMagick command (magick or convert)
     * @return ImageMagick command name, or empty string if not available
     */
    std::string GetImageMagickCommand() const;
};

} // namespace ns3

#endif /* PLOT_MANAGER_H */
