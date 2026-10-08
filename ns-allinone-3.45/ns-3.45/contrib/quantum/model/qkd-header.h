/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Anirudh V Gubbi <anirudhvgubbi@gmail.com>
 *          Akash Ravi <akashravi28055@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "ns3/header.h"

namespace ns3
{

enum QkdPhase
{                              // Handled by:
    KEY_SIFTING,               // Sender Side
    KEY_SIFTING_STOP,          // Receiver Side
    ERROR_ESTIMATION_REQ,      // Sender Side
    ERROR_ESTIMATION_RES,      // Receiver Side
    ERROR_ESTIMATION_SUCC,     // Sender Side
    PRIVACY_AMPLIFICATION_REQ, // Receiver Side
    PRIVACY_AMPLIFICATION_RES, // Sender Side
    ERROR_CORRECTION,          // - Future work
    KEY_GEN_SUCCESS,           // Receiver Side
    KEY_GEN_FAILURE            // Receiver Side
};

inline std::string
QkdPhaseToString(QkdPhase phase)
{
    static const std::array<std::string, 10> phaseStrings = {"KEY_SIFTING",
                                                             "KEY_SIFTING_STOP",
                                                             "ERROR_ESTIMATION_REQ",
                                                             "ERROR_ESTIMATION_RES",
                                                             "ERROR_ESTIMATION_SUCC",
                                                             "PRIVACY_AMPLIFICATION_REQ",
                                                             "PRIVACY_AMPLIFICATION_RES",
                                                             "ERROR_CORRECTION",
                                                             "KEY_GEN_SUCCESS",
                                                             "KEY_GEN_FAILURE"};

    if (phase >= 0 && phase < static_cast<QkdPhase>(phaseStrings.size()))
    {
        return phaseStrings[phase];
    }
    return "UNKNOWN_PHASE";
}

class QkdHeader : public Header
{
  public:
    QkdHeader();

    void SetPhase(QkdPhase phase);

    QkdPhase GetPhase() const;

    static TypeId GetTypeId();

    TypeId GetInstanceTypeId() const override;

    void Serialize(Buffer::Iterator start) const override;

    uint32_t GetSerializedSize() const override;

    uint32_t Deserialize(Buffer::Iterator start) override;

    void Print(std::ostream& os) const override;

  private:
    uint8_t m_phase;
};

} // namespace ns3
