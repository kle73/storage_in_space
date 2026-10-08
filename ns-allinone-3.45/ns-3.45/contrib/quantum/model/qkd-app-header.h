/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Navaneet Y V N <navaneetyvn.work@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_APP_HEADER_H
#define QKD_APP_HEADER_H

#include "ns3/address.h"
#include "ns3/header.h"
#include "ns3/symmetric-encryption.h"

namespace ns3
{

/**
 * @ingroup quantum
 * @brief QKD Key Manager Header Type Enum
 */
enum QkdAppHeaderType
{
    QKD_KEY_MANAGER_OPEN_CONNECT = 0,
    QKD_KEY_MANAGER_NEW_APP = 1,
    QKD_APP_SEND_KSID = 2,
    QKD_KEY_MANAGER_GET_KEY = 3,
    QKD_KEY_MANAGER_CLOSE = 4,
    QKD_APP_IV = 5,
};

enum QkdSocketType_t
{
    QKD_APP_SOCKET = 0,     //!< Socket for QKD Application (QKD sender or receiver)
    QKD_PEER_KMA_SOCKET = 1 //!< Socket for Peer KMA connection
};

/**
 * @ingroup quantum
 * @brief QKD Application Header
 *
 * This class represents the header for the QKD application.
 * It contains the data to be sent in the QKD application.
 */
class QkdAppHeader : public Header
{
  public:
    /**
     * @brief QkdAppHeader constructor
     */
    QkdAppHeader();

    /**
     * @brief QKDAppHeader destructor
     */
    ~QkdAppHeader() override;

    /**
     * Set the header data.
     * @param data The data.
     */
    void SetKSID(uint32_t data);
    /**
     * Get the header data.
     * @return The data.
     */
    uint32_t GetKSID() const;

    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * @brief Get the instance type ID.
     * @return the object TypeId
     */
    TypeId GetInstanceTypeId() const override;

    /**
     * @brief Print the header data.
     * @param os The output stream.
     */
    void Print(std::ostream& os) const override;

    /**
     * @brief Serialize the header data.
     * @param start The buffer iterator to start serialization.
     */
    void Serialize(Buffer::Iterator start) const override;

    /**
     * @brief Deserialize the header data.
     * @param start The buffer iterator to start deserialization.
     * @return The number of bytes deserialized.
     */
    uint32_t Deserialize(Buffer::Iterator start) override;

    /**
     * @brief Get the size of the serialized header.
     * @return The size of the serialized header.
     */
    uint32_t GetSerializedSize() const override;

    /**
     * @brief Set the source application ID.
     * @param source The source application ID.
     */
    void SetSource(Address source);

    /**
     * @brief Get the source application ID.
     * @return The source application ID.
     */
    Address GetSource() const;

    /**
     * @brief Set the destination application ID.
     * @param destination The destination application ID.
     */
    void SetDestination(Address destination);

    /**
     * @brief Get the destination application ID.
     * @return The destination application ID.
     */
    Address GetDestination() const;

    /**
     * @brief Set the type of the header.
     * @param type The type of the header.
     */
    void SetHeaderType(QkdAppHeaderType type);

    /**
     * @brief Get the type of the header.
     * @return The type of the header.
     */
    QkdAppHeaderType GetHeaderType() const;

    /**
     * @brief Set the initialization vector for encryption.
     * @param iv The initialization vector.
     */
    void SetIV(std::string iv);

    /**
     * @brief Get the initialization vector for encryption.
     * @return The initialization vector.
     */
    std::string GetIV() const;

    /**
     * @brief Set the symmetric encryption algorithm used for the header.
     * @param algo The symmetric encryption algorithm.
     */
    void SetEncryptionAlgo(SymmetricEncryptionAlgo algo);

    /**
     * @brief Get the symmetric encryption algorithm used for the header.
     * @return The symmetric encryption algorithm.
     */
    SymmetricEncryptionAlgo GetEncryptionAlgo() const;

  private:
    uint32_t m_ksid;                          //!< Header data
    Address m_source;                         //!< Source application ID
    Address m_destination;                    //!< Destination application ID
    std::string m_iv;                         //!< Initialization vector for encryption
    SymmetricEncryptionAlgo m_encryptionAlgo; //!< Symmetric encryption object for the header
    QkdAppHeaderType m_type;                  //!< Type of the header
};
} // namespace ns3

#endif /* QKD_APP_HEADER_H */
