/*
 * Copyright (c) 2025 NITK Surathkal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Anirudh V Gubbi <anirudhvgubbi@gmail.com>
 *          Satyam Shukla <shuklasatyam774@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "quantum-helper.h"

#include "ns3/log.h"

using namespace qpp;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumHelper");
NS_OBJECT_ENSURE_REGISTERED(QuantumHelper);

const StateVector QuantumHelper::zero = qpp::st.z0;
const StateVector QuantumHelper::one = qpp::st.z1;
const StateVector QuantumHelper::plus = qpp::st.x0;
const StateVector QuantumHelper::minus = qpp::st.x1;

TypeId
QuantumHelper::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::QuantumHelper")
                            .SetParent<Object>()
                            .SetGroupName("QKD")
                            .AddConstructor<QuantumHelper>();
    return tid;
}

QBit
QuantumHelper::GenerateQBit(StateVector stateVector)
{
    QBit qbit;
    qbit.stateVector = stateVector;

    return qbit;
}

Bit
QuantumHelper::MeasureQBit(QBit& qbit, const Basis measurement_basis)
{
    NS_LOG_FUNCTION(this << measurement_basis);
    NS_LOG_DEBUG("State vector (before): " << qbit.stateVector);

    cmat U = (measurement_basis == 0) ? qpp::gt.Z : qpp::gt.H;

    // Measure in computational basis
    auto [result, probs, collapsed] = qpp::measure(qbit.stateVector, U);
    qbit.stateVector = collapsed[result];

    NS_LOG_DEBUG("Measurement result: " << result);
    NS_LOG_DEBUG("State vector (after): " << qbit.stateVector);

    return result;
}

} // namespace ns3
