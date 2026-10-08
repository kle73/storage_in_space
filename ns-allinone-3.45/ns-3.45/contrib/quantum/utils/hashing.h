/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Navaneet Y V N <navaneetyvn.work@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef HASHING_H
#define HASHING_H

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace ns3
{

/**
 * @brief Enum to specify the hashing algorithm to be used
 */
enum HashingAlgo
{
    SHA1,
    SHA256,
    SHA512
};

/**
 * @ingroup security
 *
 * @brief Class to hash data using various hashing algorithms
 */
class HashingAlgorithm
{
  public:
    /**
     * @brief Constructor of HashingAlgorithm class
     */
    HashingAlgorithm();

    /**
     * @brief Destructor of HashingAlgorithm class
     */
    ~HashingAlgorithm();

    /**
     * @brief Hashes the given data using the specified hashing algorithm
     * @param data data to be hashed
     * @param algo hashing algorithm to be used
     * @returns hashed data
     */
    std::string HashData(std::string data, HashingAlgo algo);

  private:
    /**
     * @brief Hashes the given data using SHA1 hashing algorithm
     * @param data data to be hashed
     * @returns hashed data
     */
    std::string sha1(std::string data);

    /**
     * @brief Hashes the given data using SHA256 hashing algorithm
     * @param data data to be hashed
     * @returns hashed data
     */
    std::string sha256(std::string data);

    /**
     * @brief Hashes the given data using SHA512 hashing algorithm
     * @param data data to be hashed
     * @returns hashed data
     */
    std::string sha512(std::string data);
};
} // namespace ns3

#endif // HASHING_ALGO_H
