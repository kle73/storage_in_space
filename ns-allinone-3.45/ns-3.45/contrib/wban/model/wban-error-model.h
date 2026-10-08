/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Author: Drishti Oza
 */
#ifndef WBAN_ERROR_MODEL_H
#define WBAN_ERROR_MODEL_H

#include "ns3/object.h"

namespace ns3
{
namespace wban
{
/**
 * @ingroup wban
 *
 * Model the error rate for IEEE 802.15.6 2.6 GHz AWGN channel for DBPSK
 * the model description can be found in IEEE Std 802.15.6
 */
class WbanErrorModel : public Object
{
  public:
    /**
     * Get the type ID.
     *
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    WbanErrorModel();

    /**
     * Return uncoded DBPSK chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRateTest(double snr, uint32_t nbits) const;
    /**
     *402 MHZ to 405 MHz  with PLCP data rate of 57.5 Kbps
     * Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate402Mhz57KbpsUncoded(double snr, uint32_t nbits) const;
    /**
     *402 MHZ to 405 MHz  with PSDU data rate of 75.9 Kbps
     * Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate402Mhz75KbpsUncoded(double snr, uint32_t nbits) const;
    /**
     *402 MHZ to 405 MHz  with PSDU data rate of 151.8 Kbps
     * Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate402Mhz151KbpsUncoded(double snr, uint32_t nbits) const;
    /**
     *402 MHZ to 405 MHz with PLCP data rate of 57.5 Kbps
     * Return BCH(31,19) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate402Mhz57KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *402 MHZ to 405 MHz  with PSDU data rate of 75.9 Kbps
     * Return BCH(63,51) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate402Mhz75KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *402 MHZ to 405 MHz with PSDU data rate of 151.8 Kbps
     * Return BCH(63,51) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate402Mhz151KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz with PLCP data rate of 76.6
     *Kbps Return BCH(31,19) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate863Mhz76KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz with data rate of 101.2 Kbps
     * Return BCH(63,51) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate863Mhz101KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz with PSDU data rate of 101.2
     *Kbps Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate863Mhz101KbpsUncoded(double snr, uint32_t nbits) const;
    /**
     *863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz with PSDU data rate of 202.4
     *Kbps Return BCH(63,51) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate863Mhz202KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz with PSDU data rate of 202.4
     *Kbps Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate863Mhz202kbpsUncoded(double snr, uint32_t nbits) const;
    /**
     *863 MHz to 870 MHz or 902 MHz to 928 MHz or 950 MHz to 958 MHz with PLCP data rate of 76.6
     *Kbps Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate863Mhz76KbpsUncoded(double snr, uint32_t nbits) const;

    /**
     *2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz with PLCP data rate of 91.9 Kbps
     * Return BCH(31,19) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate2400Mhz91KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz with PSDU data rate of 121.4 Kbps
     * Return BCH(63,51) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate2400Mhz121KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz with PSDU  data rate of 121.4 Kbps
     * Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate2400Mhz121KbpsUncoded(double snr, uint32_t nbits) const;

    /**
     *2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz with PSDU data rate of 242.9 Kbps
     * Return BCH(63,51) chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate2400Mhz242KbpsCoded(double snr, uint32_t nbits) const;
    /**
     *2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz with PSDU data rate of 242.9 Kbps
     * Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate2400Mhz242KbpsUncoded(double snr, uint32_t nbits) const;
    /**
     *2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz with PLCP data rate of 91.9 Kbps
     * Return uncoded chunk success rate for given SNR.
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate2400Mhz91KbpsUncoded(double snr, uint32_t nbits) const;

    /**
     * Return chunk success rate for given SNR
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */

    double GetChunkSuccessRatePervsRxSignalExample(double snr, uint32_t nbits) const;

    /**
     *2400 MHz to 2483.5 MHz or 2360 MHz to 2400 MHz
     * Return chunk success rate for given SNR irrespective of datarate
     * @return success rate (i.e. 1 - chunk error rate)
     * @param snr SNR expressed as a power ratio (i.e. not in dB)
     * @param nbits number of bits in the chunk
     */
    double GetChunkSuccessRate2400Mhz(double snr, uint32_t nbits) const;

    /**
     *Returns factorial for the given value
     * Returns the calculated value to m_binomialCoefficients value for each array index
     * @param n The value to compute the factorial of.
     * @return The factorial of the input value.
     */
    double fact(double n) const;

  private:
    /**
     * Array of precalculated binomial coefficients for PSDU data rates.
     */
    double m_binomialCoefficients[64];
    /**
     * Array of precalculated binomial coefficients for PLCP data rates.
     */
    double m_binomialCoefficientsPlcp[32];
};
} // namespace wban
} // namespace ns3

#endif /* WBAN_ERROR_MODEL_H */
