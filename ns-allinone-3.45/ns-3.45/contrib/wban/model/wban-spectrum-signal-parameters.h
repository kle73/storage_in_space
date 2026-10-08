/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#ifndef WBAN_SPECTRUM_SIGNAL_PARAMETERS_H
#define WBAN_SPECTRUM_SIGNAL_PARAMETERS_H

#include "ns3/spectrum-signal-parameters.h"

namespace ns3
{

class PacketBurst;

namespace wban
{

/**
 * @ingroup wban
 *
 * Signal parameters for Wban.
 */
struct WbanSpectrumSignalParameters : public SpectrumSignalParameters
{
    Ptr<SpectrumSignalParameters> Copy() const override;

    /**
     * default constructor
     */
    WbanSpectrumSignalParameters();

    /**
     * copy constructor
     * @param p the object to copy from.
     */
    WbanSpectrumSignalParameters(const WbanSpectrumSignalParameters& p);

    /**
     * The packet burst being transmitted with this signal
     */
    Ptr<PacketBurst> packetBurst;
};
} // namespace wban
} // namespace ns3

#endif /* WBAN_SPECTRUM_SIGNAL_PARAMETERS_H */
