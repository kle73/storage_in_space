
/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef SYMMETRIC_ENCRYPTION_H
#define SYMMETRIC_ENCRYPTION_H

#include "ns3/object.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace ns3
{
/**
 * @brief Enum to specify the symmetric encryption algorithm to be used
 */
enum SymmetricEncryptionAlgo
{
    AES_128_GCM,
    AES_256_GCM,
    DES_ECB,
};

/**
 * @ingroup security
 *
 * @brief Class to encrypt and decrypt data using various symmetric encryption algorithms
 */
class SymmetricEncryption
{
  public:
    /**
     * @brief Constructor of SymmetricEncryption class
     */
    SymmetricEncryption();

    /**
     * @brief Destructor of SymmetricEncryption class
     */
    ~SymmetricEncryption();

    /**
     * @brief Encrypts the given data using the specified symmetric encryption algorithm
     * @param data data to be encrypted
     * @param algo symmetric encryption algorithm to be used
     * @param key key to be used for encryption
     * @param iv initialization vector to be used for encryption
     * @returns encrypted data
     */
    std::string encrypt(std::string& data,
                        SymmetricEncryptionAlgo algo,
                        std::string key,
                        std::string iv);

    /**
     * @brief Decrypts the given data using the specified symmetric encryption algorithm
     * @param data data to be decrypted
     * @param algo symmetric encryption algorithm to be used
     * @param key key to be used for decryption
     * @param iv initialization vector to be used for decryption
     * @returns decrypted data
     */
    std::string decrypt(std::string& data,
                        SymmetricEncryptionAlgo algo,
                        std::string key,
                        std::string iv);

  private:
    /**
     * @brief Encrypts the given data using AES 128 bit symmetric encryption algorithm with
     * Galois/Counter Mode (GCM)
     * @param data data to be encrypted
     * @param key key to be used for encryption
     * @param iv initialization vector to be used for encryption
     * @returns encrypted data
     */
    std::string aes128Encrypt(std::string& data, std::string key, std::string iv);

    /**
     * @brief Decrypts the given data using AES 128 symmetric encryption algorithm with
     * Galois/Counter Mode (GCM)
     * @param data data to be decrypted
     * @param key key to be used for decryption
     * @param iv initialization vector to be used for decryption
     * @returns decrypted data
     */
    std::string aes128Decrypt(std::string& data, std::string key, std::string iv);

    /**
     * @brief Encrypts the given data using AES 256 bit symmetric encryption algorithm with
     * Galois/Counter Mode (GCM)
     * @param data data to be encrypted
     * @param key key to be used for encryption
     * @param iv initialization vector to be used for encryption
     * @returns encrypted data
     */
    std::string aes256Encrypt(std::string& data, std::string key, std::string iv);

    /**
     * @brief Decrypts the given data using AES 256 symmetric encryption algorithm with
     * Galois/Counter Mode (GCM)
     * @param data data to be decrypted
     * @param key key to be used for decryption
     * @param iv initialization vector to be used for decryption
     * @returns decrypted data
     */
    std::string aes256Decrypt(std::string& data, std::string key, std::string iv);

    /**
     * @brief Encrypts the given data using DES symmetric encryption algorithm with
     * Electronic Codebook (ECB) mode
     * @param data data to be encrypted
     * @param key key to be used for encryption
     * @returns encrypted data
     */
    std::string desEncrypt(std::string& data, std::string key);

    /**
     * @brief Decrypts the given data using DES symmetric encryption algorithm with
     * Electronic Codebook (ECB) mode
     * @param data data to be decrypted
     * @param key key to be used for decryption
     * @returns decrypted data
     */
    std::string desDecrypt(std::string& data, std::string key);
};
} // namespace ns3

#endif
