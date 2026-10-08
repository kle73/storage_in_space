/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Shashank G <shashankgirish07@gmail.com>
 *         Navaneet Y V N <navaneetyvn.work@gmail.com>
 *         Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_KEY_MANAGEMENT_LAYER_H
#define QKD_KEY_MANAGEMENT_LAYER_H

#include "qkd-device.h"
#include "qkd-protocol.h"

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

/**
 * @ingroup quantum
 * @brief Represents the parameters for a key in the QKD system.
 */
struct KeyMetadata
{
    uint16_t size;            ///< The size of the key
    ns3::Time creationTime;   ///< The time when the key was created
    ns3::Time expirationTime; ///< The time when the key will expire
};

/**
 * @ingroup quantum
 * @brief Represents a key management layer for Quantum Key Distribution (QKD).
 *
 * This class manages the key generation process, including the initiation of key
 */
class QkdKeyManagementLayer : public Object
{
  public:
    /**
     * @brief Constructs a QkdKeyManagementLayer object.
     * @param qkdDevice The QKD device associated with this layer.
     * @param bufferSize The size of the buffer for key storage.
     */
    QkdKeyManagementLayer(Ptr<QkdDevice> qkdDevice, uint16_t bufferSize);

    /**
     * @brief Copy constructor for QkdKeyManagementLayer.
     * @param o The QkdKeyManagementLayer object to copy.
     */
    QkdKeyManagementLayer(const QkdKeyManagementLayer& o);

    /**
     * @brief Destructor for QkdKeyManagementLayer.
     */
    ~QkdKeyManagementLayer() override;

    /**
     * @brief Get the type ID of this class.
     * @return The type ID of this class.
     */
    static TypeId GetTypeId(void);

    /**
     * @brief Initialize the Buffer with the given size.
     * @param size The size of the buffer to initialize.
     */
    void ResizeBuffer(uint16_t size);

    /**
     * @brief Get the QKD device associated with this layer.
     * @return The QKD device associated with this layer.
     */
    Ptr<QkdDevice> GetQkdDevice() const;

    /**
     * @brief Set the QKD device associated with this layer.
     * @param qkdDevice The QKD device to set.
     */
    void SetQkdDevice(Ptr<QkdDevice> qkdDevice);

    /**
     * @brief Handle the key generation callback.
     * This function is called when the key generation process is completed.
     * @param keyGenerationData The data for key generation, including the status and generated key.
     * @note This function is responsible for handling the key generation callback.
     */
    void HandleKeyGenerationCallback(KeyGenerationData keyGenerationData);

    /**
     * @brief Schedule the key generation process.
     */
    void ScheduleKeyGeneration();

    /**
     * @brief Get the key from the buffer.
     * @return The key from the buffer.
     */
    std::pair<std::string, std::pair<Address, KeyMetadata>> GetKeyFromBuffer();

    /**
     * @brief Wait for the key generation process to complete.
     * @param address The address of the destination device.
     * @param cb The callback to invoke when the key generation is complete.
     * @param retryInterval The time interval to wait before retrying if the key generation is not
     * complete.
     * @note This function blocks until the key generation is complete.
     */
    void WaitForKeyGeneration(
        const Address& address,
        Callback<void, std::pair<std::string, std::pair<Address, KeyMetadata>>> cb,
        Time retryInterval);

    /**
     * @brief Get the current size of the buffer.
     * @return The current size of the buffer.
     */
    uint16_t GetCurrentBufferSize() const;

  private:
    /**
     * @brief Add a key to the buffer.
     * @param key The key to add to the buffer.
     * @param destination The destination for the key.
     * @param keyParams The parameters for the key.
     */
    void AddKeyToBuffer(std::string key, const Address& destinationAddress, KeyMetadata keyParams);

    /**
     * @brief Get the size of the buffer.
     * @return The size of the buffer.
     */
    uint16_t GetBufferMaxSize() const;

    mutable std::mutex m_mutex;        ///< Mutex for thread safety
    std::condition_variable m_condVar; ///< Condition variable for synchronization
    Ptr<QkdDevice> m_qkdDevice;        ///< Pointer to the QKD device
    std::vector<std::pair<std::string, std::pair<Address, KeyMetadata>>>
        m_keyBuffer;       ///< Buffer to store keys
    uint16_t m_bufferSize; ///< Size of the buffer
    bool m_running;        ///< Flag to indicate if the thread is running
};
}; // namespace ns3

#endif /* QKD_KEY_MANAGEMENT_LAYER_H */
