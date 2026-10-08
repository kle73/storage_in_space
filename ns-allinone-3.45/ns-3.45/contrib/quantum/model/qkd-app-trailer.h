/**
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Shashank G <shashankgirish07@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QKD_TRAILER_H
#define QKD_TRAILER_H

#include "ns3/trailer.h"

namespace ns3
{
/**
 * @ingroup quantum
 * @brief QKD Trailer
 *
 * This class represents the trailer for the QKD application packets
 * and contains the key generated.
 */
class QkdAppTrailer : public Trailer
{
  public:
    /**
     * @brief QKDTrailer constructor
     */
    QkdAppTrailer();

    /**
     * @brief QKDTrailer destructor
     */
    ~QkdAppTrailer() override;

    /**
     * Get the TypeId of this class.
     * @return The TypeId of this class.
     */
    static TypeId GetTypeId();

    /**
     * Gets the instance TypeId of this class.
     * @return The TypeId of this instance.
     */
    TypeId GetInstanceTypeId() const override;

    /**
     * Set the key.
     * @param key The key to be set.
     */
    void SetKeyOrData(const std::string& key);

    /**
     * Get the key.
     * @return The key.
     */
    std::string GetKeyOrData() const;

    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator end) const override;
    uint32_t Deserialize(Buffer::Iterator end) override;

  private:
    std::string m_keyOrData; //!< The quantum key generated.
};
} // namespace ns3

#endif /* QKD_TRAILER_H */
