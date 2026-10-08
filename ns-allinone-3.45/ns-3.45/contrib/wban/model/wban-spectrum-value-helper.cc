/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#include "wban-spectrum-value-helper.h"

#include "ns3/log.h"
#include "ns3/spectrum-value.h"

#include <cmath>

// FOR 2360 to 2400 MHz & 2400 to 2483.5 MHz band of WBAN
namespace ns3
{

namespace wban
{
NS_LOG_COMPONENT_DEFINE("WbanSpectrumValueHelper");

Ptr<SpectrumModel>
    g_WbanSpectrumModel; //!< Global object used to initialize the Wban Spectrum Model

/**
 * @ingroup wban
 * @brief Helper class used to automatically initialize the Wban Spectrum Model objects
 */
class WbanSpectrumModelInitializer
{
  public:
    WbanSpectrumModelInitializer()
    {
        NS_LOG_FUNCTION(this);

        Bands bands;
        // 1 MHz resolution, with center frequency of 2400, 2401, ... 2483
        // overall frequency span of 2399.5 MHz through 2483.5 MHz (83 bands)
        for (int i = 0; i < 81; i++)
        {
            BandInfo bi;
            bi.fl = 2401.0e6 + i * 1.0e6;
            bi.fh = 2401.5e6 + (i + 1) * 1.0e6;
            bi.fc = (bi.fl + bi.fh) / 2;
            bands.push_back(bi);
        }
        g_WbanSpectrumModel = Create<SpectrumModel>(bands);
    }

} g_WbanSpectrumModelInitializerInstance; //!< Global object used to initialize the LrWpan Spectrum
                                          //!< Model

WbanSpectrumValueHelper::WbanSpectrumValueHelper()
{
    NS_LOG_FUNCTION(this);
    m_noiseFactor = 1.0;
}

WbanSpectrumValueHelper::~WbanSpectrumValueHelper()
{
    NS_LOG_FUNCTION(this);
}

Ptr<SpectrumValue>
WbanSpectrumValueHelper::CreateTxPowerSpectralDensity(double txPower, uint32_t channel)
{
    NS_LOG_FUNCTION(this);
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue>(g_WbanSpectrumModel);

    // txPower is expressed in dBm. We must convert it into natural unit (W).
    txPower = pow(10., (txPower - 30) / 10);

    // The effective occupied bandwidth of the signal is modelled to be 1 MHz.
    // 99.5% of power is within +/- 1MHz of center frequency, and 0.5% is outside.
    // There are 5 bands containing signal power.  The middle (center) band
    // contains half of the power.  The two inner side bands contain 49.5%.
    // The two outer side bands contain roughly 0.5%.
    double txPowerDensity = txPower / 3.e5; // 30% of total bandwidth

    // NS_ASSERT_MSG ((channel >= 0 && channel <= 78), "Invalid channel numbers");
    //
    // see section 8.6.1 of WBAN standard for channel assessment, and relation between channel
    // number and center frequency center frequency for 2.4 GHz = 2402.00 + 1.00 * channel 1 MHz is
    // the total bandwidth
    //
    (*txPsd)[2402 + 1 * channel - 2400 - 2] = txPowerDensity * 0.0025;
    (*txPsd)[2402 + 1 * channel - 2400 - 1] = txPowerDensity * 0.2475;
    (*txPsd)[2402 + 1 * channel - 2400] = txPowerDensity; // center
    (*txPsd)[2402 + 1 * channel - 2400 + 1] = txPowerDensity * 0.2475;
    (*txPsd)[2402 + 1 * channel - 2400 + 2] = txPowerDensity * 0.0025;

    // If more power is allocated to more sub bands in future revisions of
    // this model, make sure to re-normalize so that the integral of the
    // txPsd still equals txPower

    return txPsd;
}

Ptr<SpectrumValue>
WbanSpectrumValueHelper::CreateNoisePowerSpectralDensity(uint32_t channel)
{
    NS_LOG_FUNCTION(this);
    Ptr<SpectrumValue> noisePsd = Create<SpectrumValue>(g_WbanSpectrumModel);

    static const double BOLTZMANN = 1.3803e-23;
    // Nt  is the power of thermal noise in W
    double Nt = BOLTZMANN * 290.0;
    // noise Floor (W) which accounts for thermal noise and non-idealities of the receiver
    double noisePowerDensity = m_noiseFactor * Nt;

    // NS_ASSERT_MSG ((channel >= 0 && channel <= 78), "Invalid channel numbers");
    (*noisePsd)[2402 + 1 * channel - 2400 - 2] = noisePowerDensity;
    (*noisePsd)[2402 + 1 * channel - 2400 - 1] = noisePowerDensity;
    (*noisePsd)[2402 + 1 * channel - 2400] = noisePowerDensity;
    (*noisePsd)[2402 + 1 * channel - 2400 + 1] = noisePowerDensity;
    (*noisePsd)[2402 + 1 * channel - 2400 + 2] = noisePowerDensity;

    return noisePsd;
}

void
WbanSpectrumValueHelper::SetNoiseFactor(double f)
{
    m_noiseFactor = f;
}

double
WbanSpectrumValueHelper::TotalAvgPower(Ptr<const SpectrumValue> psd, uint32_t channel)
{
    NS_LOG_FUNCTION(psd);
    double totalAvgPower = 0.0;

    NS_ASSERT(psd->GetSpectrumModel() == g_WbanSpectrumModel);

    // numerically integrate to get area under psd using 1 MHz resolution

    totalAvgPower += (*psd)[2402 + 1 * channel - 2400 - 2];
    totalAvgPower += (*psd)[2402 + 1 * channel - 2400 - 1];
    totalAvgPower += (*psd)[2402 + 1 * channel - 2400];
    totalAvgPower += (*psd)[2402 + 1 * channel - 2400 + 1];
    totalAvgPower += (*psd)[2402 + 1 * channel - 2400 + 2];
    totalAvgPower *= 1.745e5; // 1.64E5
    return totalAvgPower;
}
} // namespace wban
} // namespace ns3

// TODO: Implement all six band spectrum value helper in one function

//--------FOR WBAN 863 to 870 MHz or 902 to 928 MHZ or 950 to 958 MHz band--------------

/*
namespace ns3
{
namespace wban
{

NS_LOG_COMPONENT_DEFINE ("WbanSpectrumValueHelper");

Ptr<SpectrumModel> g_WbanSpectrumModel; //!< Global object used to initialize the Wban Spectrum
Model

//
 // ingroup Wban
 // brief Helper class used to automatically initialize the Wban Spectrum Model objects
 //
class WbanSpectrumModelInitializer
{
public:
  WbanSpectrumModelInitializer (void)
  {
    NS_LOG_FUNCTION (this);

    Bands bands;
    // 1 MHz resolution, with center frequency of 2400, 2401, ... 2483
    // overall frequency span of 2399.5 MHz through 2483.5 MHz (83 bands)
    for (int i = 0; i < 81; i++)
      {
        BandInfo bi;
        bi.fl =  863.20e6+ i * 4.0e5;
        bi.fh =  863.20e6 + (i + 1) * 4.0e5;
        bi.fc = (bi.fl +  bi.fh) / 2;
        bands.push_back (bi);
      }
    g_WbanSpectrumModel = Create<SpectrumModel> (bands);
  }

} g_WbanSpectrumModelInitializerInstance; //!< Global object used to initialize the LrWpan Spectrum
Model

WbanSpectrumValueHelper::WbanSpectrumValueHelper (void)
{
  NS_LOG_FUNCTION (this);
  m_noiseFactor = 1.0;
}

WbanSpectrumValueHelper::~WbanSpectrumValueHelper (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<SpectrumValue>
WbanSpectrumValueHelper::CreateTxPowerSpectralDensity (double txPower, uint32_t channel)
{
  NS_LOG_FUNCTION (this);
  Ptr<SpectrumValue> txPsd = Create <SpectrumValue> (g_WbanSpectrumModel);

  // txPower is expressed in dBm. We must convert it into natural unit (W).
  txPower = pow (10., (txPower - 30) / 10);

  // The effective occupied bandwidth of the signal is modelled to be 1 MHz.
  // 99.5% of power is within +/- 1MHz of center frequency, and 0.5% is outside.
  // There are 5 bands containing signal power.  The middle (center) band
  // contains half of the power.  The two inner side bands contain 49.5%.
  // The two outer side bands contain roughly 0.5%.

  double txPowerDensity = txPower / 244.0e3; // 61% of total power

  //NS_ASSERT_MSG ((channel >= 0 && channel <= 78), "Invalid channel numbers");
   //
   // see section 8.6.1 of WBAN standard for channel assessment, and relation between channel number
and center frequency
   // center frequency for 402 HHz = 402.15 + 0.30 * channel
   // 300 KHz is the total bandwidth
   //
  (*txPsd)[863.20 + 0.40 * channel - 863.20 - 2] = txPowerDensity * 0.000500;
  (*txPsd)[863.20 + 0.40 * channel - 863.20 - 1] = txPowerDensity * 0.049500;
  (*txPsd)[863.20 + 0.40 * channel - 863.20] = txPowerDensity; // center
  (*txPsd)[863.20 + 0.40 * channel - 863.20 + 1] = txPowerDensity * 0.049500;
  (*txPsd)[863.20 + 0.40 * channel - 863.20 + 2] = txPowerDensity * 0.000500;

  // If more power is allocated to more sub bands in future revisions of
  // this model, make sure to re-normalize so that the integral of the
  // txPsd still equals txPower

  return txPsd;
}

Ptr<SpectrumValue>
WbanSpectrumValueHelper::CreateNoisePowerSpectralDensity (uint32_t channel)
{
  NS_LOG_FUNCTION (this);
  Ptr<SpectrumValue> noisePsd = Create <SpectrumValue> (g_WbanSpectrumModel);

  static const double BOLTZMANN = 1.3803e-23;
  // Nt  is the power of thermal noise in W
  double Nt = BOLTZMANN * 290.0;
  // noise Floor (W) which accounts for thermal noise and non-idealities of the receiver
  double noisePowerDensity = m_noiseFactor * Nt;

  NS_ASSERT_MSG ((channel >= 0 && channel <= 78), "Invalid channel numbers");
  (*noisePsd)[863.20 + 0.40 * channel - 863.20 - 2] = noisePowerDensity;
  (*noisePsd)[863.20 + 0.40 * channel - 863.20 - 1] = noisePowerDensity;
  (*noisePsd)[863.20 + 0.40 * channel - 863.20] = noisePowerDensity;
  (*noisePsd)[863.20 + 0.40 * channel - 863.20 + 1] = noisePowerDensity;
  (*noisePsd)[863.20 + 0.40 * channel - 863.20 + 2] = noisePowerDensity;

  return noisePsd;
}


void
WbanSpectrumValueHelper::SetNoiseFactor(double f)
{
    m_noiseFactor = f;
}

double
WbanSpectrumValueHelper::TotalAvgPower (Ptr<const SpectrumValue> psd, uint32_t channel)
{
  NS_LOG_FUNCTION (psd);
  double totalAvgPower = 0.0;

  NS_ASSERT (psd->GetSpectrumModel () == g_WbanSpectrumModel);

  // numerically integrate to get area under psd using 1 MHz resolution

  totalAvgPower += (*psd)[863.20 + 0.40 * channel - 863.20 - 2];
  totalAvgPower += (*psd)[863.20 + 0.40 * channel - 863.20 - 1];
  totalAvgPower += (*psd)[863.20 + 0.40 * channel - 863.20];
  totalAvgPower += (*psd)[863.20 + 0.40 * channel - 863.20 + 1];
  totalAvgPower += (*psd)[863.20 + 0.40 * channel - 863.20 + 2];
  totalAvgPower *= 65.0e3;

  return totalAvgPower;
}
}//namespace wban
}// namespace ns3
*/

//--------------FOR WBAN 402 to 405 MHz band--------------

/*
namespace ns3 {
namespace wban
{

NS_LOG_COMPONENT_DEFINE ("WbanSpectrumValueHelper");

Ptr<SpectrumModel> g_WbanSpectrumModel; //!< Global object used to initialize the Wban Spectrum
Model

//
 // ingroup Wban
 // brief Helper class used to automatically initialize the Wban Spectrum Model objects
 //
class WbanSpectrumModelInitializer
{
public:
  WbanSpectrumModelInitializer (void)
  {
    NS_LOG_FUNCTION (this);

    Bands bands;
    // 1 MHz resolution, with center frequency of 2400, 2401, ... 2483
    // overall frequency span of 2399.5 MHz through 2483.5 MHz (83 bands)
    for (int i = 0; i < 81; i++)
      {
        BandInfo bi;
        bi.fl =  402.15e6+ i * 3.0e5;
        bi.fh = 402.15e6 + (i + 1) * 3.0e5;
        bi.fc = (bi.fl +  bi.fh) / 2;
        bands.push_back (bi);
      }
    g_WbanSpectrumModel = Create<SpectrumModel> (bands);
  }

} g_WbanSpectrumModelInitializerInstance; //!< Global object used to initialize the LrWpan Spectrum
Model

WbanSpectrumValueHelper::WbanSpectrumValueHelper (void)
{
  NS_LOG_FUNCTION (this);
  m_noiseFactor = 1.0;
}

WbanSpectrumValueHelper::~WbanSpectrumValueHelper (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<SpectrumValue>
WbanSpectrumValueHelper::CreateTxPowerSpectralDensity (double txPower, uint32_t channel)
{
  NS_LOG_FUNCTION (this);
  Ptr<SpectrumValue> txPsd = Create <SpectrumValue> (g_WbanSpectrumModel);

  // txPower is expressed in dBm. We must convert it into natural unit (W).
  txPower = pow (10., (txPower - 30) / 10);

  // The effective occupied bandwidth of the signal is modelled to be 1 MHz.
  // 99.5% of power is within +/- 1MHz of center frequency, and 0.5% is outside.
  // There are 5 bands containing signal power.  The middle (center) band
  // contains half of the power.  The two inner side bands contain 49.5%.
  // The two outer side bands contain roughly 0.5%.

  double txPowerDensity = txPower / 180.0e3; // 60% of total power

  //NS_ASSERT_MSG ((channel >= 0 && channel <= 78), "Invalid channel numbers");
   //
   // see section 8.6.1 of WBAN standard for channel assessment, and relation between channel number
and center frequency
   // center frequency for 402 HHz = 402.15 + 0.30 * channel
   // 300 KHz is the total bandwidth
   //
  (*txPsd)[402.15 + 0.30 * channel - 402.15 - 2] = txPowerDensity * 0.000375;
  (*txPsd)[402.15 + 0.30 * channel - 402.15 - 1] = txPowerDensity * 0.037125;
  (*txPsd)[402.15 + 0.30 * channel - 402.15] = txPowerDensity; // center
  (*txPsd)[402.15 + 0.30 * channel - 402.15 + 1] = txPowerDensity * 0.037125;
  (*txPsd)[402.15 + 0.30 * channel - 402.15 + 2] = txPowerDensity * 0.000375;

  // If more power is allocated to more sub bands in future revisions of
  // this model, make sure to re-normalize so that the integral of the
  // txPsd still equals txPower

  return txPsd;
}

Ptr<SpectrumValue>
WbanSpectrumValueHelper::CreateNoisePowerSpectralDensity (uint32_t channel)
{
  NS_LOG_FUNCTION (this);
  Ptr<SpectrumValue> noisePsd = Create <SpectrumValue> (g_WbanSpectrumModel);

  static const double BOLTZMANN = 1.3803e-23;
  // Nt  is the power of thermal noise in W
  double Nt = BOLTZMANN * 290.0;
  // noise Floor (W) which accounts for thermal noise and non-idealities of the receiver
  double noisePowerDensity = m_noiseFactor * Nt;

  NS_ASSERT_MSG ((channel >= 0 && channel <= 78), "Invalid channel numbers");
  (*noisePsd)[402.15 + 0.30 * channel - 402.15 - 2] = noisePowerDensity;
  (*noisePsd)[402.15 + 0.30 * channel - 402.15 - 1] = noisePowerDensity;
  (*noisePsd)[402.15 + 0.30 * channel - 402.15] = noisePowerDensity;
  (*noisePsd)[402.15 + 0.30 * channel - 402.15 + 1] = noisePowerDensity;
  (*noisePsd)[402.15 + 0.30 * channel - 402.15 + 2] = noisePowerDensity;

  return noisePsd;
}


void
WbanSpectrumValueHelper::SetNoiseFactor(double f)
{
    m_noiseFactor = f;
}

double
WbanSpectrumValueHelper::TotalAvgPower (Ptr<const SpectrumValue> psd, uint32_t channel)
{
  NS_LOG_FUNCTION (psd);
  double totalAvgPower = 0.0;

  NS_ASSERT (psd->GetSpectrumModel () == g_WbanSpectrumModel);

  // numerically integrate to get area under psd using 1 MHz resolution

  totalAvgPower += (*psd)[402.15 + 0.30 * channel - 402.15 - 2];
  totalAvgPower += (*psd)[402.15 + 0.30 * channel - 402.15 - 1];
  totalAvgPower += (*psd)[402.15 + 0.30 * channel - 402.15];
  totalAvgPower += (*psd)[402.15 + 0.30 * channel - 402.15 + 1];
  totalAvgPower += (*psd)[402.15 + 0.30 * channel - 402.15 + 2];
  totalAvgPower *= 48.0e3;

  return totalAvgPower;
}
}//namespace wban
} // namespace ns3
*/
