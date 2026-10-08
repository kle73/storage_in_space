/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#include "wban-spectrum-signal-parameters.h"

#include "ns3/log.h"
#include "ns3/packet-burst.h"
#include "ns3/ptr.h"

namespace ns3
{
namespace wban
{
NS_LOG_COMPONENT_DEFINE("WbanSpectrumSignalParameters");

WbanSpectrumSignalParameters::WbanSpectrumSignalParameters()
{
    NS_LOG_FUNCTION(this);
}

WbanSpectrumSignalParameters::WbanSpectrumSignalParameters(const WbanSpectrumSignalParameters& p)
    : SpectrumSignalParameters(p)
{
    NS_LOG_FUNCTION(this << &p);
    packetBurst = p.packetBurst->Copy();
}

Ptr<SpectrumSignalParameters>
WbanSpectrumSignalParameters::Copy() const
{
    NS_LOG_FUNCTION(this);
    return Create<WbanSpectrumSignalParameters>(*this);
}
} // namespace wban
} // namespace ns3
