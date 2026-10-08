/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */

#ifndef WBAN_PROPAGATION_MODEL_H
#define WBAN_PROPAGATION_MODEL_H

#include "ns3/object.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/random-variable-stream.h"

#include <unordered_map>

namespace ns3
{
/**
 * @ingroup propagation
 * @ingroup tests
 * @defgroup propagation-tests Propagation module tests
 */

class MobilityModel;

namespace wban
{
/**
 *
 * BodyDielectricParameters structure specifies the dielectric properties
 * for calculation of attenuation loss for BodyPropagationLossModel
 */
struct BodyDielectricParameters
{
    double organConductivity;  //!< The ability to transfer heat, electricity, for organ
    double organPermittivity;  //!< The ability to hold an electrical charge, for organ
    double organThickness;     //!< The thickness of the organ in m.
    double muscleConductivity; //!< conductivity of muscle for the given frequency
    double musclePermittivity; //!< Permittivity of muscle for the given frequency
    double muscleThickness;    //!< The thickness of the muscle in m.
    double muscleLayer;        //!< number of layers of muscle , n  layer is 'thickness * n'
    double fatConductivity;    //!< conductivity of fat for the given frequency
    double fatPermittivity;    //!< Permittivity of fat for the given frequency
    double fatThickness;       //!< The thickness of the fat in m.
    double fatLayer;           //!< number of layers of fat , n  layer is 'thickness * n'
    double skinConductivity;   //!< conductivity of skin for the given frequency
    double skinPermittivity;   //!< Permittivity of skin for the given frequency
    double skinThickness;      //!< The thickness of the skin in m.
    double frequency;          //!< frequency at which the calculations happen
};

enum class BodyOrganOption
{
    SMALL_INTESTINE_2400_MHZ = 0,
    SMALL_INTESTINE_916_5_MHZ = 1,
    FAT_2400_MHZ = 2,
    FAT_402_MHZ = 3,
    SKIN_2400_MHZ = 4,
    SKIN_863_MHZ = 5,
    SKIN_402_MHZ = 6,
    LARGE_INTESTINE_2400_MHZ = 7,
    SMALL_INTESTINE_402_MHZ = 8,
    HEART_2400_MHZ = 9,
    KIDNEY_2400_MHZ = 10,
    HEART_402_MHZ = 11,
    KIDNEY_402_MHZ = 12
};

/**
 * @class BodyPropagationLossModel
 * @brief Models the calculations for attenuation loss
 */
class BodyPropagationLossModel : public PropagationLossModel
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    BodyPropagationLossModel();

    // Delete copy constructor and assignment operator to avoid misuse
    BodyPropagationLossModel(const BodyPropagationLossModel&) = delete;
    BodyPropagationLossModel& operator=(const BodyPropagationLossModel&) = delete;
    /**
     * The currently configured body organ.
     */
    BodyOrganOption m_bodyOption;
    /**
     * The currently configured number of fat layer
     */
    BodyDielectricParameters m_fatLayer;
    /**
     * The currently configured DielectricParameters index
     */
    BodyDielectricParameters m_parameters;

    /**
     *  select the body organ option
     *  @param bodyOption is the current body option
     */
    void SetBodyOptions(BodyOrganOption bodyOption);
    /**
     * Get the currently configured Body Option(organ)
     * @return the Body option
     */
    BodyOrganOption GetBodyOption() const;
    /**
     * Set number of fat layer
     * by default 1
     * @param fatLayer the number of fat layers
     */
    void SetFatLayer(uint32_t fatLayer);
    /**
     * Get the changed number of fat layers from default
     * @return The number of fat layers
     */
    uint32_t GetFatLayer() const;
    /**
     * @brief  Set number of muscle layer
     * by default 1
     * @param MuscleLayer the number of muscle layers
     */
    void SetMuscleLayer(uint32_t MuscleLayer);
    /**
     * Get the changed number of muscle layers from default
     * @return The number of muscle layers
     */
    uint32_t GetMuscleLayer() const;

  private:
    /* calculations for interference due to body (attenuation constant)
     * attenuation constant = (520.8𝜋𝜃 / √𝜖𝑟) * d
     *  𝜃 is the conductivity of human tissue,
     * 𝜖𝑟 is the relative permittivity of human tissue,
     * d is the thickness of the tissues in the path of the signal.
     */
    double DoCalcRxPower(double txPowerDbm,
                         Ptr<MobilityModel> a,
                         Ptr<MobilityModel> b) const override;

    int64_t DoAssignStreams(int64_t stream) override;
};

} // namespace wban
} // namespace ns3
#endif /*WBAN_PHY_H*/
