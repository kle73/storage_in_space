/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Anirudh V Gubbi <anirudhvgubbi@gmail.com>
 *          Satyam Shukla <shuklasatyam774@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#ifndef QUANTUM_HELPER_H
#define QUANTUM_HELPER_H

#include "qpp/qpp.hpp"

#include "ns3/object.h"

#include <complex>
#include <tuple>
#include <vector>

namespace ns3
{

// typedefs used in the quantum simulation
typedef short int Bit;
typedef short int Basis;
typedef qpp::ket StateVector;

/**
 * @ingroup quantum
 *
 * @brief Represents a quantum bit (QBit) in a quantum computing system.
 *
 * The QBit struct models a vector of quantum bits. It uses a vector of complex
 * amplitudes to represent the quantum state of the QBit.
 */
struct QBit : public Object
{
    // The state vectors of the current Qbit.
    StateVector stateVector;
};

/**
 * @ingroup quantum
 *
 * @brief Helper class that provides an interface over quantum operations.
 *
 * The current implementation uses qpp library internally to perform qunatum
 * operations.
 */
class QuantumHelper : public Object
{
  public:
    /**
     * Constructor
     */
    QuantumHelper()
    {
    }

    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId(void);

    /**
     * Produces a QBit based on the given bit and a basis specified by its angle.
     * @param bit the bit to encode in the resulting QBit
     * @param basis the basis for the polarized bit.
     * @returns a `QBit` with the encoded bit
     */
    QBit GenerateQBit(StateVector stateVector);

    /**
     * @brief Measures a quantum bit (QBit) in a given measurement basis.
     *
     * This function performs a measurement on the specified QBit using the
     * provided measurement basis, which is represented as a collection of
     * complex-valued vectors.
     *
     * @param qbit the QBit to be measured
     * @param measurement_basis the measurement basis to be used during measuring.
     * @return measured QBit
     *
     */
    Bit MeasureQBit(QBit& qbit, const Basis measurement_basis);

    static const StateVector zero;
    static const StateVector one;
    static const StateVector plus;
    static const StateVector minus;
};

// typedefs used in the quantum simulation
typedef qpp::cmat Gate;

} // namespace ns3

#endif /* QUANTUM_HELPER_H */
