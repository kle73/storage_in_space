/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#ifndef WBAN_SPECTRUM_VALUE_HELPER_H
#define WBAN_SPECTRUM_VALUE_HELPER_H

#include "ns3/ptr.h"

namespace ns3
{

class SpectrumValue;

namespace wban
{
/**
 * @ingroup wban
 * @brief Helper class used to automatically initialize the Wban Spectrum Model objects
 */
class WbanSpectrumValueHelper
{
  public:
    WbanSpectrumValueHelper();
    virtual ~WbanSpectrumValueHelper();

    /**
     * @brief create spectrum value
     * @param txPower the power transmission in dBm
     * @param channel the channel number per IEEE802.15.6
     * @return a Ptr to a newly created SpectrumValue instance
     */
    Ptr<SpectrumValue> CreateTxPowerSpectralDensity(double txPower, uint32_t channel);
    /**
     * @brief create spectrum value for noise
     * @param channel the channel number per IEEE802.15.6
     * @return a Ptr to a newly created SpectrumValue instance
     */
    Ptr<SpectrumValue> CreateNoisePowerSpectralDensity(uint32_t channel);

    /**
     * Set the noise factor added to the thermal noise.
     * @param f A dimensionless ratio (i.e. Not in dB)
     */
    void SetNoiseFactor(double f);

    /**
     * @brief total average power of the signal is the integral of the PSD using
     * the limits of the given channel
     * @param psd spectral density
     * @param channel the channel number per IEEE802.15.6
     * @return total power (using composite trap. rule to numerically integrate)
     */
    static double TotalAvgPower(Ptr<const SpectrumValue> psd, uint32_t channel);

  private:
    /**
     * A scaling factor for the noise power.
     * It specifies how much additional noise the device
     * contribute to the thermal noise (floor noise).
     */
    double m_noiseFactor;
};
} // namespace wban
} // namespace ns3

#endif /*  WBAN_SPECTRUM_VALUE_HELPER_H */
