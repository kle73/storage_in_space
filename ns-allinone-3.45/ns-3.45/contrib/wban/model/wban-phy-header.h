/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#ifndef WBAN_PHY_HEADERS_H
#define WBAN_PHY_HEADERS_H

#include "ns3/header.h"
#include "ns3/log.h"

namespace ns3
{

namespace wban
{
/**
 * @class NarrowBandPhyHeader
 * @brief Header for NarrowBand PHY layer packets in the WBAN module.
 *
 * This header carries PHY-level metadata such as data rate, header length,
 * and control flags for NarrowBand communications in the WBAN PHY layer.
 */
class NarrowBandPhyHeader : public Header
{
  public:
    /*NarrowBand Standard PPDU structure - see IEEE 802.15.6 Figure 116 in Section 8
     *PHY header 0-14 bits
     */
    uint32_t m_phyHeaderRate;   //!< PHY Header bits 0-2
    uint32_t m_phyHeaderLength; //!< PHY Header bits 4-11
    bool m_phyHeaderBurstMode;  //!< PHY Header bits 13
    bool m_phyHeaderSeed;       //!< PHY Header bits 14
    uint8_t m_headerHcs;        //!< HCS (4 bits)
    uint32_t m_headerBchParity; //!< BCH Parity (12 bits)
    uint16_t m_nMacHeader = 56; //!< 7 octets of mac header
    uint16_t m_nMacFrame = 32;  //!< 4 octets of mac frame
    uint16_t m_nFcs = 16;       //!< 2 octets of FCS

    /**
     * Get the Frame fields
     * @return the Frame fields
     */
    uint32_t GetFrame() const;
    /**
     * Get the Header Rate
     * @return the Header Rate
     */
    uint32_t GetRate() const;
    /**
     * Get the Header Length
     * @return the Header Length
     */
    uint32_t GetLength() const;
    /**
     * Check if the next packet is part of a burst
     * @return true if Burst bit is enabled
     */
    bool IsBurst() const;
    /**
     * @return seed value chosen for the data scrambler
     */
    bool IsSeed() const;
    /**
     * @brief Get the Header HCS
     * @return the Header Hcs
     */
    uint8_t GetHcs() const;
    /**
     * @brief Get Header Bch Parity bits
     * @return the Header Bch Parity bits
     */
    uint32_t GetBchParity() const;
    /**
     * @brief Set the whole Frame field
     * @param frame the Frame field
     */
    void SetFrame(uint32_t frame);
    /**
     * @brief Set the Header Rate
     * @param rate Header Rate
     */
    void SetRate(uint32_t rate);
    /**
     * @brief Set the Header Length
     * @param length Header Length
     */
    void SetLength(uint32_t length);
    /**
     * @brief Set the field "Burst Mode" bit to true
     */
    void SetBurstEnable();
    /**
     * @brief Set the field "Burst Mode" bit to false
     */
    void SetBurstDisable();
    /**
     * Set the field "seed" bit to 1
     */
    void SetSeedEnable();
    /**
     * @brief Set the field "Seed" bit to 0
     */
    void SetSeedDisable();
    /**
     * Set the field for 2 bit HCS
     * @param hcs Header Check Sequence (HCS) value.
     */
    void SetHcs(uint8_t hcs);
    /**
     * @brief Set the field for 12 bit Parity
     * @param parity The BCH parity value.
     */
    void SetBchParity(uint32_t parity);
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    TypeId GetInstanceTypeId() const override;

    /**
     * @brief Print function for frame
     * @param os The output stream to print to.
     */
    void Print(std::ostream& os) const override;
    /**
     * @brief Get the size of the serialized NarrowBand fields.
     * @return the size of the serialized fields.
     */
    uint32_t GetSerializedSize() const override;
    /**
     * @brief Serialize the entire narrowBand fields.
     * @param i an iterator which points to where the super-frame specification field should be
     * written.
     */
    void Serialize(Buffer::Iterator i) const override;
    /**
     * @brief Deserialize the entire narrowBand fields.
     * @param i an iterator which points to where the super-frame specification field should be
     * read. @return an iterator.
     */
    uint32_t Deserialize(Buffer::Iterator i) override;
};
} // namespace wban
} // namespace ns3
#endif /* WBAN_PHY_HEADERS_H */
