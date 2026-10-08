/*
 * Copyright (c) 2024 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Authors: David Lin <davidzylin@gmail.com>
 *
 * Output Manager for AQM Evaluation Suite
 * Provides centralized output directory and file management
 */

#ifndef AQM_EVAL_SUITE_OUTPUT_MANAGER_H
#define AQM_EVAL_SUITE_OUTPUT_MANAGER_H

#include "ns3/core-module.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ns3
{

/**
 * @ingroup aqm-eval-suite
 * @brief Centralized output management for AQM evaluation suite
 */
class OutputManager
{
  public:
    /**
     * Constructor
     * @param baseDir Base directory for all outputs (default: "aqm-eval-output")
     * @param useTimestamp Whether to create timestamped subdirectories
     */
    OutputManager(const std::string& baseDir = "aqm-eval-output", bool useTimestamp = true);

    /**
     * Destructor
     */
    ~OutputManager() = default;

    /**
     * Initialize the output directory structure
     * @return true if successful, false otherwise
     */
    bool Initialize();

    /**
     * Create directory structure for a scenario
     * @param scenarioName Name of the scenario
     * @return true if successful, false otherwise
     */
    bool CreateScenarioDirectories(const std::string& scenarioName);

    /**
     * Get the full path for a scenario's data directory
     * @param scenarioName Name of the scenario
     * @return Full path to data directory
     */
    std::string GetDataPath(const std::string& scenarioName) const;

    /**
     * Get the full path for a scenario's graph directory
     * @param scenarioName Name of the scenario
     * @return Full path to graph directory
     */
    std::string GetGraphPath(const std::string& scenarioName) const;

    /**
     * Get the full path for a scenario's root directory
     * @param scenarioName Name of the scenario
     * @return Full path to scenario directory
     */
    std::string GetScenarioPath(const std::string& scenarioName) const;

    /**
     * Get the base output directory path
     * @return Full path to base output directory
     */
    std::string GetBaseOutputPath() const;

    /**
     * Save run configuration to JSON file
     * @param config Configuration parameters
     * @return true if successful, false otherwise
     */
    bool SaveRunConfiguration(const std::map<std::string, std::string>& config);

    /**
     * Create a file with specified content
     * @param filePath Relative path from base output directory
     * @param content File content
     * @return true if successful, false otherwise
     */
    bool CreateFile(const std::string& filePath, const std::string& content);

    /**
     * Append content to an existing file
     * @param filePath Relative path from base output directory
     * @param content Content to append
     * @return true if successful, false otherwise
     */
    bool AppendToFile(const std::string& filePath, const std::string& content);

    /**
     * Get the timestamp used for this run
     * @return Timestamp string
     */
    std::string GetTimestamp() const;

    /**
     * Clean up old output directories
     * @param daysToKeep Number of days of outputs to keep
     * @return Number of directories removed
     */
    int CleanupOldOutputs(int daysToKeep = 7);

    /**
     * Create a gitignore file in the output directory
     * @return true if successful, false otherwise
     */
    bool CreateGitIgnore();

    /**
     * Check if all required directories exist
     * @return true if all directories exist, false otherwise
     */
    bool ValidateStructure() const;

  private:
    std::string m_baseDir;        //!< Base directory name
    std::string m_timestamp;      //!< Timestamp for this run
    std::string m_fullOutputPath; //!< Full path to output directory
    bool m_useTimestamp;          //!< Whether to use timestamped directories

    /**
     * Generate timestamp string
     * @return Formatted timestamp
     */
    std::string GenerateTimestamp() const;

    /**
     * Create directory recursively
     * @param path Directory path to create
     * @return true if successful, false otherwise
     */
    bool CreateDirectoryRecursive(const std::string& path) const;

    /**
     * Join path components safely
     * @param components Vector of path components
     * @return Joined path
     */
    std::string JoinPath(const std::vector<std::string>& components) const;
};

} // namespace ns3

#endif /* OUTPUT_MANAGER_H */
