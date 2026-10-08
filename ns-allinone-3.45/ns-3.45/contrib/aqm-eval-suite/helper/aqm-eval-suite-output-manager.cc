/*
 * Copyright (c) 2024 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Authors: David Lin <davidzylin@gmail.com>
 *
 * Output Manager implementation for AQM Evaluation Suite
 */

#include "aqm-eval-suite-output-manager.h"

#include "ns3/log.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AqmEvalSuiteOutputManager");

OutputManager::OutputManager(const std::string& baseDir, bool useTimestamp)
    : m_baseDir(baseDir),
      m_useTimestamp(useTimestamp)
{
    if (m_useTimestamp)
    {
        m_timestamp = GenerateTimestamp();
        m_fullOutputPath = JoinPath({m_baseDir, m_timestamp});
    }
    else
    {
        m_timestamp = "";
        m_fullOutputPath = m_baseDir;
    }
}

bool
OutputManager::Initialize()
{
    NS_LOG_FUNCTION(this);

    // Create base directory
    if (!CreateDirectoryRecursive(m_baseDir))
    {
        NS_LOG_ERROR("Failed to create base directory: " << m_baseDir);
        return false;
    }

    // Create timestamped directory if needed
    if (m_useTimestamp && !CreateDirectoryRecursive(m_fullOutputPath))
    {
        NS_LOG_ERROR("Failed to create output directory: " << m_fullOutputPath);
        return false;
    }

    // Create gitignore
    if (!CreateGitIgnore())
    {
        NS_LOG_WARN("Failed to create .gitignore file");
    }

    return true;
}

bool
OutputManager::CreateScenarioDirectories(const std::string& scenarioName)
{
    NS_LOG_FUNCTION(this << scenarioName);

    std::vector<std::string> directories = {GetScenarioPath(scenarioName),
                                            GetDataPath(scenarioName),
                                            GetGraphPath(scenarioName)};

    for (const auto& dir : directories)
    {
        if (!CreateDirectoryRecursive(dir))
        {
            NS_LOG_ERROR("Failed to create directory: " << dir);
            return false;
        }
    }

    return true;
}

std::string
OutputManager::GetDataPath(const std::string& scenarioName) const
{
    return JoinPath({m_fullOutputPath, scenarioName, "data"});
}

std::string
OutputManager::GetGraphPath(const std::string& scenarioName) const
{
    return JoinPath({m_fullOutputPath, scenarioName, "graph"});
}

std::string
OutputManager::GetScenarioPath(const std::string& scenarioName) const
{
    return JoinPath({m_fullOutputPath, scenarioName});
}

std::string
OutputManager::GetBaseOutputPath() const
{
    return m_fullOutputPath;
}

bool
OutputManager::SaveRunConfiguration(const std::map<std::string, std::string>& config)
{
    NS_LOG_FUNCTION(this);

    std::string configPath = JoinPath({m_fullOutputPath, "run_config.json"});
    std::ofstream configFile(configPath);

    if (!configFile.is_open())
    {
        NS_LOG_ERROR("Failed to open config file: " << configPath);
        return false;
    }

    configFile << "{\n";
    configFile << "  \"timestamp\": \"" << m_timestamp << "\",\n";

    auto it = config.begin();
    for (; it != config.end(); ++it)
    {
        configFile << "  \"" << it->first << "\": \"" << it->second << "\"";
        if (std::next(it) != config.end())
        {
            configFile << ",";
        }
        configFile << "\n";
    }

    configFile << "}\n";
    configFile.close();

    NS_LOG_INFO("Saved run configuration to: " << configPath);
    return true;
}

bool
OutputManager::CreateFile(const std::string& filePath, const std::string& content)
{
    NS_LOG_FUNCTION(this << filePath);

    std::string fullPath = JoinPath({m_fullOutputPath, filePath});

    // Ensure parent directory exists
    std::filesystem::path parentPath = std::filesystem::path(fullPath).parent_path();
    if (!CreateDirectoryRecursive(parentPath.string()))
    {
        NS_LOG_ERROR("Failed to create parent directory for: " << fullPath);
        return false;
    }

    std::ofstream file(fullPath);
    if (!file.is_open())
    {
        NS_LOG_ERROR("Failed to create file: " << fullPath);
        return false;
    }

    file << content;
    file.close();
    return true;
}

bool
OutputManager::AppendToFile(const std::string& filePath, const std::string& content)
{
    NS_LOG_FUNCTION(this << filePath);

    std::string fullPath = JoinPath({m_fullOutputPath, filePath});
    std::ofstream file(fullPath, std::ios::app);

    if (!file.is_open())
    {
        NS_LOG_ERROR("Failed to open file for append: " << fullPath);
        return false;
    }

    file << content;
    file.close();
    return true;
}

std::string
OutputManager::GetTimestamp() const
{
    return m_timestamp;
}

int
OutputManager::CleanupOldOutputs(int daysToKeep)
{
    NS_LOG_FUNCTION(this << daysToKeep);

    if (!std::filesystem::exists(m_baseDir))
    {
        return 0;
    }

    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::hours(24 * daysToKeep);
    int removedCount = 0;

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(m_baseDir))
        {
            if (entry.is_directory())
            {
                auto dirTime = std::filesystem::last_write_time(entry);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    dirTime - std::filesystem::file_time_type::clock::now() +
                    std::chrono::system_clock::now());

                if (sctp < cutoffTime)
                {
                    std::filesystem::remove_all(entry);
                    removedCount++;
                    NS_LOG_INFO("Removed old output directory: " << entry.path().string());
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        NS_LOG_ERROR("Error during cleanup: " << e.what());
    }

    return removedCount;
}

bool
OutputManager::CreateGitIgnore()
{
    NS_LOG_FUNCTION(this);

    std::string gitignorePath = JoinPath({m_baseDir, ".gitignore"});

    // Check if .gitignore already exists
    if (std::filesystem::exists(gitignorePath))
    {
        return true;
    }

    std::ofstream gitignoreFile(gitignorePath);
    if (!gitignoreFile.is_open())
    {
        NS_LOG_ERROR("Failed to create .gitignore file: " << gitignorePath);
        return false;
    }

    gitignoreFile << "*\n";
    gitignoreFile.close();

    return true;
}

bool
OutputManager::ValidateStructure() const
{
    return std::filesystem::exists(m_fullOutputPath) &&
           std::filesystem::is_directory(m_fullOutputPath);
}

std::string
OutputManager::GenerateTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

bool
OutputManager::CreateDirectoryRecursive(const std::string& path) const
{
    try
    {
        std::filesystem::create_directories(path);
        return true;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        NS_LOG_ERROR("Failed to create directory " << path << ": " << e.what());
        return false;
    }
}

std::string
OutputManager::JoinPath(const std::vector<std::string>& components) const
{
    if (components.empty())
    {
        return "";
    }

    std::filesystem::path result = components[0];
    for (size_t i = 1; i < components.size(); ++i)
    {
        result /= components[i];
    }

    return result.string();
}

} // namespace ns3
