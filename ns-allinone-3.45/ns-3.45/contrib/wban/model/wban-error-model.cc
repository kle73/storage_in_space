/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#include "wban-error-model.h"

#include "ns3/log.h"

#include <cmath>
#include <iostream>

namespace ns3
{

namespace wban
{

NS_LOG_COMPONENT_DEFINE("WbanErrorModel");

NS_OBJECT_ENSURE_REGISTERED(WbanErrorModel);

TypeId
WbanErrorModel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::WbanErrorModel")
                            .SetParent<Object>()
                            .SetGroupName("Wban")
                            .AddConstructor<WbanErrorModel>();
    return tid;
}

// assigned 63 values to the array m_binomialCoefficient
WbanErrorModel::WbanErrorModel()
{
    for (uint32_t r = 3; r <= 63; r++)
    {
        double sum = fact(63) / (fact(r) * fact(63 - r));
        m_binomialCoefficients[r] = sum;

        if (r <= 31)
        {
            double sum = fact(31) / (fact(r) * fact(31 - r));
            m_binomialCoefficientsPlcp[r] = sum;
        }
    }
}

double
WbanErrorModel::fact(double n) const
{
    double f = 1;
    for (double i = 1; i <= n; i++)
    {
        f = f * i;
    }
    return f;
}

double ber = 0;
double p = 0;

/* BER calculation for uncoded DBPSK where,
 * BER = 1/2 e^(- SNR[lin])
 */
double
WbanErrorModel::GetChunkSuccessRateTest(double snr, uint32_t nbits) const
{
    ber = 0.5 * exp(-(snr));
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);

    return retval;
}

/* All the following functions calculate BER using the formula
 * BER = 1/2 e^(- SNR[lin] * (bandwidth/data rate))
 * bandwidth in Hertz and data rate in bps
 */

/* BER calculation for 402-405 MHz frequency for PLCP header
 * bandwidth = 300 KHz
 * datarate = 57.5 Kbps
 */
double
WbanErrorModel::GetChunkSuccessRate402Mhz57KbpsUncoded(double snr, uint32_t nbits) const
{
    ber = 0.5 * exp(-snr * 300000 / 57500);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* BER calculation for 402-405 MHz frequency for PSDU
 * bandwidth = 300 KHz
 * datarate = 75.9 Kbps
 */
double
WbanErrorModel::GetChunkSuccessRate402Mhz75KbpsUncoded(double snr, uint32_t nbits) const
{
    ber = 0.5 * exp(-snr * 300000 / 75900);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* BER calculation for 402-405 MHz frequency for PSDU
 * bandwidth = 300 KHz
 * datarate = 151.8 Kbps
 */
double
WbanErrorModel::GetChunkSuccessRate402Mhz151KbpsUncoded(double snr, uint32_t nbits) const
{
    ber = 0.5 * exp(-snr * 300000 / 151800);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for 402-405 MHz frequency for PLCP header
 * BCH(31, 19)
 * bandwidth = 300 Khz (300000 HERTZ)
 * PLCP data rate = 57.5 kbps (57500 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate402Mhz57KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t l = 3; l <= 31; l++)
    {
        p = 0.5 * exp(-(19.0 / 31.0) * snr * 300000 / 57500);
        ber += (1.0 / 31.0) * m_binomialCoefficientsPlcp[l] * l * (pow(p, l)) *
               (pow((1 - p), (31 - l)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for 402-405 MHz frequency for PSDU
 * BCH (63, 51)
 * bandwidth = 300 Khz (300000 HERTZ)
 * PSDU data rate = 75.9 kbps (75900 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate402Mhz75KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t k = 3; k <= 63; k++)
    {
        p = 0.5 * exp(-(51.0 / 63.0) * snr * 300000 / 75900);
        ber +=
            (1.0 / 63.0) * m_binomialCoefficients[k] * k * (pow(p, k)) * (pow((1 - p), (63 - k)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for 402-405 MHz frequency for PSDU
 * BCH (63, 51)
 * bandwidth = 300 Khz (300000 HERTZ)
 * PSDU data rate = 151.8 kbps (151800 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate402Mhz151KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t k = 3; k <= 63; k++)
    {
        p = 0.5 * exp(-(51.0 / 63.0) * snr * 300000 / 151800);
        ber +=
            (1.0 / 63.0) * m_binomialCoefficients[k] * k * (pow(p, k)) * (pow((1 - p), (63 - k)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz PLCP
 * header BCH (31, 19) bandwidth = 400 Khz (400000 HERTZ) PLCP data rate = 76.6 kbps (76600 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate863Mhz76KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t l = 3; l <= 31; l++)
    {
        p = 0.5 * exp(-(19.0 / 31.0) * snr * (400000 / 76600));
        ber += (1.0 / 31.0) * m_binomialCoefficientsPlcp[l] * l * (pow(p, l)) *
               (pow((1 - p), (31 - l)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz PSDU
 * BCH (63, 51)
 * bandwidth= 400 Khz (400000 HERTZ)
 * PSDU data rate = 101.2 kbps (101200 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate863Mhz101KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t k = 3; k <= 63; k++)
    {
        p = 0.5 * exp(-snr * 400000 / 101200 * 51.0 / 63.0);
        ber +=
            (1.0 / 63.0) * m_binomialCoefficients[k] * k * (pow(p, k)) * (pow((1 - p), (63 - k)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz PSDU
 * BCH (63, 51)
 * bandwidth = 400 Khz (400000 HERTZ)
 * PSDU data rate = 202.4 kbps (202400 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate863Mhz202KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t k = 3; k <= 63; k++)
    {
        p = 0.5 * exp(-(51.0 / 63.0) * snr * (400000 / 202400));
        ber +=
            (1.0 / 63.0) * m_binomialCoefficients[k] * k * (pow(p, k)) * (pow((1 - p), (63 - k)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/*BER calculation for 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz PSDU
 * bandwidth = 400 Khz (400000 HERTZ)
 * PSDU data rate = 202.4 kbps (202400 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate863Mhz202kbpsUncoded(double snr, uint32_t nbits) const
{
    ber = 0.5 * exp(-snr * 400000 / 202400);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/*BER calculation for 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz PSDU
 * bandwidth = 400 Khz (400000 HERTZ)
 * PSDU data rate = 101.2 kbps (101200 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate863Mhz101KbpsUncoded(double snr, uint32_t nbits)
    const // check if bandwidth is true
{
    ber = 0.5 * exp(-snr * 400000 / 101200);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/*BER calculation for 863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz PSDU
 * bandwidth = 400 Khz (400000 HERTZ)
 * PSDU data rate = 76.6 kbps (76600 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate863Mhz76KbpsUncoded(double snr, uint32_t nbits) const
{
    ber = 0.5 * exp(-snr * 400000 / 76600);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for 2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz PLCP header
 * BCH(31, 19)
 * bandwidth = 1 Mhz (1000000 HERTZ)
 * PLCP data rate = 91.9 kbps (91900 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate2400Mhz91KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t l = 3; l <= 31; l++)
    {
        p = 0.5 * exp(-(19.0 / 31.0) * snr * 1000000 / 91900);
        ber += (1.0 / 31.0) * m_binomialCoefficientsPlcp[l] * l * (pow(p, l)) *
               (pow((1 - p), (31 - l)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for  2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz PSDU
 * BCH (63, 51)
 * bandwidth = 1 Mhz (1000000 HERTZ)
 * PSDU data rate = 121.4 kbps (121400 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate2400Mhz121KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t k = 3; k <= 63; k++)
    {
        p = 0.5 * exp(-(51.0 / 63.0) * snr * 1000000 / 121400);
        ber +=
            (1.0 / 63.0) * m_binomialCoefficients[k] * k * (pow(p, k)) * (pow((1 - p), (63 - k)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* Coded BER calculation for  2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz PSDU
 * BCH (63, 51)
 * bandwidth = 1 Mhz (1000000 HERTZ)
 * PSDU data rate = 242.9 kbps (242900 bps)
 */
double
WbanErrorModel::GetChunkSuccessRate2400Mhz242KbpsCoded(double snr, uint32_t nbits) const
{
    for (uint32_t k = 3; k <= 63; k++)
    {
        p = 0.5 * exp(-(51.0 / 63.0) * snr * 1000000 / 242900);
        ber +=
            (1.0 / 63.0) * m_binomialCoefficients[k] * k * (pow(p, k)) * (pow((1 - p), (63 - k)));
    }

    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/* BER calculation 2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz
 * BER without considering bandwidth and datarate
 * used to calculate PER for the entire frequency band
 */
double
WbanErrorModel::GetChunkSuccessRate2400Mhz(double snr, uint32_t nbits) const
{
    for (uint32_t k = 3; k <= 63; k++)
    {
        p = 0.5 * exp(-(51.0 / 63.0) * snr);
        ber +=
            (1.0 / 63.0) * m_binomialCoefficients[k] * k * (pow(p, k)) * (pow((1 - p), (63 - k)));
    }
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

// following are the three function that can be used to test experimental PER for 2.4GHz band
// by using GetChunkSuccessRate2400Mhz242KbpsUn we get correct range for "noise
// floor-----maxSensitivity------minSensitivity" GetChunkSuccessRate2400Mhz91KbpsUn is the data rate
// for preamble transmission only GetChunkSuccessRate2400Mhz121KbpsUn and
// GetChunkSuccessRate2400Mhz242KbpsUn are data rates for PSDU transmission.

double
WbanErrorModel::GetChunkSuccessRate2400Mhz242KbpsUncoded(double snr, uint32_t nbits) const
{
    ber = 0.5 * exp(-snr * 1000000 / 242900);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

double
WbanErrorModel::GetChunkSuccessRate2400Mhz121KbpsUncoded(double snr, uint32_t nbits) const
{
    ber = 0.5 * exp(-snr * 1000000 / 121400);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

double
WbanErrorModel::GetChunkSuccessRate2400Mhz91KbpsUncoded(double snr, uint32_t nbits) const //
{
    ber = 0.5 * exp(-snr * 1000000 / 91000);
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

/*
 * Following BER calculation is used to test the PER in example wban-per-vs-rx-signal.
 */
double
WbanErrorModel::GetChunkSuccessRatePervsRxSignalExample(double snr, uint32_t nbits) const
{
    for (uint32_t k = 3; k <= 63; k++)
    {
        ber = 0.5 * exp(-(51.0 / 63.0) * snr);
    }
    NS_LOG_DEBUG(ber << "= (BER)");
    ber = std::min(ber, 1.0);
    double retval = pow(1.0 - ber, nbits);
    return retval;
}

} // namespace wban
} // namespace ns3
