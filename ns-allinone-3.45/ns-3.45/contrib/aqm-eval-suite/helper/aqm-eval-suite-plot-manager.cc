/*
 * Copyright (c) 2024 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Authors: David Lin <davidzylin@gmail.com>
 *
 * Plot Manager implementation for AQM Evaluation Suite
 */

#include "aqm-eval-suite-plot-manager.h"

#include "ns3/log.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AqmEvalSuitePlotManager");

PlotManager::PlotManager(OutputManager& outputManager)
    : m_outputManager(outputManager)
{
    m_utilsPath = GetUtilsPath();
}

bool
PlotManager::GeneratePlots(const std::string& scenarioName,
                           const std::vector<std::string>& aqmAlgorithms,
                           const std::string& queueDiscSuffix)
{
    NS_LOG_FUNCTION(this << scenarioName);

    // Step 1: Create initial gnuplot script with header only
    if (!CreateInitialGnuplotScript(scenarioName))
    {
        NS_LOG_ERROR("Failed to create initial gnuplot script for scenario " << scenarioName);
        return false;
    }

    // Step 2: Process data for each AQM algorithm (ellipsemaker will append labels)
    for (size_t i = 0; i < aqmAlgorithms.size(); ++i)
    {
        if (!ProcessDataForPlotting(scenarioName, aqmAlgorithms[i], queueDiscSuffix, i + 1))
        {
            NS_LOG_ERROR("Failed to process data for " << aqmAlgorithms[i] << " in scenario "
                                                       << scenarioName);
            return false;
        }
    }

    // Step 3: Append the plot command
    if (!AppendPlotCommand(scenarioName, aqmAlgorithms, queueDiscSuffix))
    {
        NS_LOG_ERROR("Failed to append plot command for scenario " << scenarioName);
        return false;
    }

    // Step 4: Execute gnuplot script
    if (!ExecuteGnuplotScript(scenarioName))
    {
        NS_LOG_ERROR("Failed to execute gnuplot script for scenario " << scenarioName);
        return false;
    }

    // Step 5: Convert to EPS format
    if (!ConvertToEPS(scenarioName))
    {
        NS_LOG_WARN("Failed to convert to EPS format for scenario " << scenarioName);
        // Don't return false here as this is not critical
    }

    NS_LOG_INFO("Successfully generated plots for scenario: " << scenarioName);
    return true;
}

bool
PlotManager::ProcessDataForPlotting(const std::string& scenarioName,
                                    const std::string& aqmAlgorithm,
                                    const std::string& queueDiscSuffix,
                                    int aqmIndex)
{
    NS_LOG_FUNCTION(this << scenarioName << aqmAlgorithm);

    std::string baseOutputDir = m_outputManager.GetBaseOutputPath();
    std::string aqmName = aqmAlgorithm + queueDiscSuffix;

    // Commands to process data
    std::vector<std::string> commands = {
        "python " + m_utilsPath + "/generate-ellipseinput.py " + scenarioName + " " + aqmName +
            " " + baseOutputDir,
        "python " + m_utilsPath + "/ellipsemaker " + scenarioName + " " + aqmName + " " +
            std::to_string(aqmIndex) + " " +
            baseOutputDir, // Index will be set properly in real implementation
        "python " + m_utilsPath + "/goodput_process.py " + scenarioName + " " + aqmName + " " +
            baseOutputDir,
        "python " + m_utilsPath + "/delay_process.py " + scenarioName + " " + aqmName + " " +
            baseOutputDir};

    // Add drop processing for RttFairness scenarios
    if (scenarioName.find("RttFairness") != std::string::npos)
    {
        commands.push_back("python " + m_utilsPath + "/drop_process.py " + scenarioName + " " +
                           aqmName + " " + baseOutputDir);
    }

    for (const auto& command : commands)
    {
        if (!SafeSystemCall(command))
        {
            NS_LOG_ERROR("Failed to execute command: " << command);
            return false;
        }
    }

    return true;
}

bool
PlotManager::CreateInitialGnuplotScript(const std::string& scenarioName)
{
    NS_LOG_FUNCTION(this << scenarioName);

    std::string scriptPath = scenarioName + "/data/plot-shell";
    std::string content = GetGnuplotHeader(scenarioName);

    return m_outputManager.CreateFile(scriptPath, content);
}

bool
PlotManager::ExecuteGnuplotScript(const std::string& scenarioName)
{
    NS_LOG_FUNCTION(this << scenarioName);

    std::string scriptPath =
        m_outputManager.GetBaseOutputPath() + "/" + scenarioName + "/data/plot-shell";
    std::string command = "gnuplot " + scriptPath;

    return SafeSystemCall(command);
}

bool
PlotManager::ConvertToEPS(const std::string& scenarioName)
{
    NS_LOG_FUNCTION(this << scenarioName);

    std::string imageMagickCmd = GetImageMagickCommand();
    if (imageMagickCmd.empty())
    {
        NS_LOG_WARN("ImageMagick not found - skipping EPS conversion for scenario "
                    << scenarioName);
        return false;
    }

    std::string basePath =
        m_outputManager.GetBaseOutputPath() + "/" + scenarioName + "/graph/qdel-goodput";
    std::string command = imageMagickCmd + " " + basePath + ".png " + basePath + ".eps";

    return SafeSystemCall(command);
}

bool
PlotManager::CheckPlottingDependencies()
{
    bool pythonOk = (system("python3 --version > /dev/null 2>&1") == 0);
    bool numpyOk = pythonOk && (system("python3 -c \"import numpy\" > /dev/null 2>&1") == 0);
    bool gnuplotOk = (system("which gnuplot > /dev/null 2>&1") == 0);

    // Check for ImageMagick (prefer magick command from v7, fallback to convert from v6)
    bool magickOk = (system("which magick > /dev/null 2>&1") == 0);
    bool convertOk = !magickOk && (system("which convert > /dev/null 2>&1") == 0);
    bool imageMagickOk = magickOk || convertOk;

    if (pythonOk && numpyOk && gnuplotOk && imageMagickOk)
    {
        return true;
    }
    else
    {
        NS_LOG_WARN("Plotting dependencies missing:");
        if (!pythonOk)
        {
            NS_LOG_WARN("  - Python 3 not found");
        }
        if (!numpyOk && pythonOk)
        {
            NS_LOG_WARN("  - NumPy not available (pip install numpy)");
        }
        if (!gnuplotOk)
        {
            NS_LOG_WARN("  - Gnuplot not found");
        }
        if (!imageMagickOk)
        {
            NS_LOG_WARN("  - ImageMagick not found (needed for EPS conversion)");
        }
        NS_LOG_WARN("  Simulations will run but plots may be affected.");
        return false;
    }
}

bool
PlotManager::SafeSystemCall(const std::string& command)
{
    int result = system(command.c_str());
    if (result != 0)
    {
        NS_LOG_WARN("Command failed with exit code " << result << ": " << command);
        return false;
    }
    return true;
}

std::string
PlotManager::GetGnuplotHeader(const std::string& scenarioName)
{
    std::string baseOutputPath = m_outputManager.GetBaseOutputPath();
    std::stringstream ss;

    ss << "set terminal png size 600, 350\n";
    ss << "set size .9, 1\n";
    ss << "set output \"" << baseOutputPath << "/" << scenarioName << "/graph/qdel-goodput.png\"\n";
    ss << "set xlabel \"Queue Delay (ms)\" font \"Verdana\"\n";
    ss << "set ylabel \"Goodput (Mbps)\" font \"Verdana\"\n";
    ss << "set xrange[] reverse\n";
    ss << "set grid\n";
    ss << "show grid\n";

    return ss.str();
}

std::string
PlotManager::BuildPlotCommand(const std::string& scenarioName,
                              const std::vector<std::string>& aqmAlgorithms,
                              const std::string& queueDiscSuffix)
{
    std::string baseOutputPath = m_outputManager.GetBaseOutputPath();
    std::stringstream plotCommand;
    plotCommand << "plot ";

    for (size_t i = 0; i < aqmAlgorithms.size(); ++i)
    {
        std::string aqmName = aqmAlgorithms[i] + queueDiscSuffix;
        plotCommand << "\"" << baseOutputPath << "/" << scenarioName << "/data/" << aqmName
                    << "-ellipse.dat\" notitle with lines";

        if (i < aqmAlgorithms.size() - 1)
        {
            plotCommand << ",";
        }
    }

    plotCommand << "\n";
    return plotCommand.str();
}

bool
PlotManager::AppendPlotCommand(const std::string& scenarioName,
                               const std::vector<std::string>& aqmAlgorithms,
                               const std::string& queueDiscSuffix)
{
    NS_LOG_FUNCTION(this << scenarioName);

    std::string scriptPath = scenarioName + "/data/plot-shell";
    std::string fullPath = m_outputManager.GetBaseOutputPath() + "/" + scriptPath;

    // Open file in append mode
    std::ofstream outFile(fullPath, std::ios::app);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Failed to open file for appending: " << fullPath);
        return false;
    }

    // Append the plot command
    std::string plotCommand = BuildPlotCommand(scenarioName, aqmAlgorithms, queueDiscSuffix);
    outFile << plotCommand;
    outFile.close();

    return true;
}

std::string
PlotManager::GetUtilsPath() const
{
    // Get the path of this source file
    std::string currentFile = __FILE__;
    std::filesystem::path currentPath(currentFile);

    // Get the parent directory (helper)
    std::filesystem::path helperDir = currentPath.parent_path();

    // Get the module root directory (one level up from helper)
    std::filesystem::path moduleRoot = helperDir.parent_path();

    // Construct path to utils directory
    std::filesystem::path utilsPath = moduleRoot / "utils";

    return utilsPath.string();
}

std::string
PlotManager::GetImageMagickCommand() const
{
    // Check for ImageMagick 7+ (magick command) first
    if (system("which magick > /dev/null 2>&1") == 0)
    {
        return "magick";
    }
    // Fallback to ImageMagick 6 (convert command)
    else if (system("which convert > /dev/null 2>&1") == 0)
    {
        return "convert";
    }
    // Neither available
    return "";
}

} // namespace ns3
