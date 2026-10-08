/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#include "wban-propagation-model.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/pointer.h"
#include "ns3/string.h"

#include <cmath>
#include <math.h>

namespace ns3
{

namespace wban
{

static const BodyDielectricParameters DielectricParameters[] = {
    {3.1335,  // organConductivity
     54.527,  // organPermittivity
     0.01,    // organThickness
     1.705,   // muscleConductivity
     52.791,  // musclePermittivity
     0.012,   // muscleThickness
     1,       // muscleLayer
     0.10235, // fatConductivity
     5.2853,  // fatPermittivity
     0.046,   // fatThickness
     1,       // fatLayer
     1.4407,  // skinConductivity
     38.063,  // skinPermittivity
     0.0013,  // skinThickness
     2.4},    // frequency
    {2.1738,
     59.379,
     0.01,
     0.94861,
     54.994,
     0.012,
     1,
     0.051438,
     5.4594,
     0.046,
     1,
     0.87219,
     41.322,
     0.0013,
     916.5},
    {1, 1, 0, 1, 1, 0, 0, 0.10235, 5.2853, 0.046, 2, 1.4407, 38.063, 0.0013, 2.4},
    {1, 1, 0, 1, 1, 0, 0, 0.041151, 5.5789, 0.046, 2, 0.68892, 46.741, 0.0013, 402},
    {1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1.4407, 38.063, 0.0013, 2.4},
    {1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0.85451, 41.603, 0.0013, 863},
    {1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0.68892, 46.741, 0.0013, 402},
    {1.3739,
     51.877,
     0.02,
     1.705,
     52.791,
     0.012,
     1,
     0.10235,
     5.2853,
     0.046,
     1,
     1.4407,
     38.063,
     0.0013,
     2.4},
    {1.9035,
     66.086,
     0.01,
     0.79682,
     57.112,
     0.012,
     1,
     0.041151,
     5.5789,
     0.046,
     1,
     0.68892,
     46.741,
     0.0013,
     402},
    {2.2159,
     54.918,
     0.015,
     1.705,
     52.791,
     0.012,
     1,
     0.10235,
     5.2853,
     0.046,
     1,
     1.4407,
     38.063,
     0.0013,
     2.4},
    {2.3901,
     52.856,
     0.01,
     1.705,
     52.791,
     0.012,
     1,
     0.10235,
     5.2853,
     0.046,
     1,
     1.4407,
     38.063,
     0.0013,
     2.4},
    {0.96577,
     66.049,
     0.015,
     0.79682,
     57.112,
     0.012,
     1,
     0.041151,
     5.5789,
     0.046,
     1,
     0.68892,
     46.741,
     0.0013,
     402},
    {1.0958,
     66.361,
     0.01,
     0.79682,
     57.112,
     0.012,
     1,
     0.041151,
     5.5789,
     0.046,
     1,
     0.68892,
     46.741,
     0.0013,
     402}};

NS_LOG_COMPONENT_DEFINE("WbanPropagationLossModel");

NS_OBJECT_ENSURE_REGISTERED(BodyPropagationLossModel);

TypeId
BodyPropagationLossModel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::BodyPropagationLossModel")
                            .SetParent<PropagationLossModel>()
                            .SetGroupName("Propagation")
                            .AddConstructor<BodyPropagationLossModel>();
    return tid;
}

BodyPropagationLossModel::BodyPropagationLossModel()
{
    SetBodyOptions(BodyOrganOption::SMALL_INTESTINE_402_MHZ);
}

void
BodyPropagationLossModel::SetBodyOptions(BodyOrganOption bodyOption)
{
    m_bodyOption = bodyOption;
    auto index = static_cast<uint32_t>(m_bodyOption);
    m_parameters = DielectricParameters[index];
}

BodyOrganOption
BodyPropagationLossModel::GetBodyOption() const
{
    // NS_LOG_FUNCTION (this);
    return m_bodyOption;
}

void
BodyPropagationLossModel::SetFatLayer(uint32_t fatLayer)
{
    m_parameters.fatLayer = fatLayer;
    NS_LOG_DEBUG("new fat layer = " << fatLayer);
}

uint32_t
BodyPropagationLossModel::GetFatLayer() const
{
    return m_parameters.fatLayer;
}

void
BodyPropagationLossModel::SetMuscleLayer(uint32_t muscleLayer)
{
    m_parameters.muscleLayer = muscleLayer;
    NS_LOG_DEBUG("new muscle layer = " << muscleLayer);
}

uint32_t
BodyPropagationLossModel::GetMuscleLayer() const
{
    return m_parameters.muscleLayer;
}

double
BodyPropagationLossModel::DoCalcRxPower(double txPowerDbm,
                                        Ptr<MobilityModel> a,
                                        Ptr<MobilityModel> b) const
{
    auto bodyOption = static_cast<uint32_t>(m_bodyOption);

    double organDb = 0;
    double skinDb = 0;
    double muscleDb = 0;
    double fatDb = 0;
    double totalDb = 0;

    organDb = (520.8 * M_PI * DielectricParameters[bodyOption].organConductivity) /
              pow(DielectricParameters[bodyOption].organPermittivity, 0.5) *
              DielectricParameters[bodyOption].organThickness;

    muscleDb = (520.8 * M_PI * DielectricParameters[bodyOption].muscleConductivity) /
               pow(DielectricParameters[bodyOption].musclePermittivity, 0.5) *
               DielectricParameters[bodyOption].muscleThickness *
               DielectricParameters[bodyOption].muscleLayer * m_parameters.muscleLayer;

    fatDb = (520.8 * M_PI * DielectricParameters[bodyOption].fatConductivity) /
            pow(DielectricParameters[bodyOption].fatPermittivity, 0.5) *
            DielectricParameters[bodyOption].fatThickness *
            DielectricParameters[bodyOption].fatLayer * m_parameters.fatLayer;

    skinDb = (520.8 * M_PI * DielectricParameters[bodyOption].skinConductivity) /
             pow(DielectricParameters[bodyOption].skinPermittivity, 0.5) *
             DielectricParameters[bodyOption].skinThickness;

    NS_LOG_DEBUG("layer of fat = "
                 << DielectricParameters[bodyOption].fatLayer * m_parameters.fatLayer
                 << " & layer of muscle = "
                 << DielectricParameters[bodyOption].muscleLayer * m_parameters.muscleLayer);
    // for testing purpose
    NS_LOG_DEBUG("organ loss = " << organDb);
    NS_LOG_DEBUG("muscle loss = " << muscleDb);
    NS_LOG_DEBUG("fat loss = " << fatDb);
    NS_LOG_DEBUG("skin loss = " << skinDb);

    totalDb = organDb + skinDb + fatDb + muscleDb;

    NS_LOG_DEBUG("loss due to body  in db is = " << totalDb);

    return txPowerDbm - totalDb;
}

int64_t
BodyPropagationLossModel::DoAssignStreams(int64_t stream)
{
    return 0;
}
} // namespace wban
} // namespace ns3
