/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#ifndef WBAN_LQI_TAG_H
#define WBAN_LQI_TAG_H

#include "ns3/tag.h"

namespace ns3
{
namespace wban
{
/**
 * @ingroup wban
 * Represent the LQI (Link Quality Estination).
 *
 * The LQI Tag is added to each received packet, and can be
 * used by upper layers to estimate the channel conditions.
 *
 * The LQI is the total packet success rate scaled to 0-255.
 */

class WbanLqiTag : public Tag
{
  public:
    /**
     * Get the type ID.
     *
     * @return the object TypeId
     */

    static TypeId GetTypeId();

    TypeId GetInstanceTypeId() const override;

    /**
     * Create a WbanLqiTag with the default LQI 0.
     */

    WbanLqiTag();
    /**
     * Create a WbanLqiTag with the given LQI value.
     * @param lqi The LQI.
     */

    WbanLqiTag(uint8_t lqi);

    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer i) const override;
    void Deserialize(TagBuffer i) override;
    void Print(std::ostream& os) const override;

    /**
     * Set the LQI to the given value.
     *
     * @param lqi the value of the LQI to set
     */

    void Set(uint8_t lqi);

    /**
     * Get the LQI value.
     *
     * @return the LQI value
     */

    uint8_t Get() const;

  private:
    /**
     * The current LQI value of the tag.
     */

    uint8_t m_lqi;
};
} // namespace wban
} // namespace ns3
#endif /* WBAN_LQI_TAG_H */
