#ifndef CONSTELLATION_CONFIG_H
#define CONSTELLATION_CONFIG_H

#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ns3 {

/**
 * Per-constellation topology roles.
 * All satellite IDs are the ns3 node IDs assigned by storage_in_space.cc
 * (nodeId = orbit * satellitesPerOrbit + positionInOrbit).
 *
 * To add a new constellation: add a new branch in MakeConstellationConfig().
 */
struct ConstellationConfig {

    uint32_t seamRightUp;
    uint32_t seamLeftUp;
    uint32_t seamRightDown;
    uint32_t seamLeftDown;

    std::unordered_set<uint32_t> ringUp;    // satellites that act as ring-up relay
    std::unordered_set<uint32_t> ringDown;  // satellites that act as ring-down relay
    std::unordered_set<uint32_t> seamRight; // satellites whose RIGHT link is a seam link
    std::unordered_set<uint32_t> seamLeft;  // satellites whose LEFT  link is a seam link

    double safetyMarginS         {60.0};

    // true for Walker-Delta constellations (no seam): the rings are closed
    // loops, the scheduler also checks the ring ISL from the last to the
    // first orbit, and the routing is RoutingRingSwitchDelta.
    bool closedRing              {false};
};

/**
 * Returns the ConstellationConfig for the given constellation name.
 * Returns an empty (all-false) config for unknown names.
 */
ConstellationConfig MakeConstellationConfig (const std::string& name);

} // namespace ns3
#endif // CONSTELLATION_CONFIG_H
