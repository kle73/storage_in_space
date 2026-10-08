#include "constellation-config.h"

namespace ns3 {

ConstellationConfig MakeConstellationConfig (const std::string& name)
{
    ConstellationConfig cfg;

    if (name == "iridium") {
        // ── Current ring positions (before any switch) ────────────────────────
        cfg.ringUp    = {62,  8, 19, 30, 41, 52};  // orbit5:pos6, orbit0-4:pos8
        cfg.ringDown  = {47, 57, 14, 25, 36, 3};  // orbit0-4:pos2, orbit5:pos0
        // cfg.ringDown  = {46, 56, 13, 24, 35, 2};  // orbit0-4:pos2, orbit5:pos0

        // Seam satellites
        cfg.seamRight = {44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54};
        cfg.seamLeft  = {55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65};


        cfg.seamRightUp   = 52;  // orbit4 pos7
        cfg.seamLeftUp    = 62;  // orbit5 pos5
        cfg.seamRightDown = 47;  // orbit4 pos1
        cfg.seamLeftDown  = 57;  // orbit5 pos10V

        cfg.safetyMarginS     = 50.0;


    } else if (name == "oneweb") {
        // ── OneWeb constellation parameters (from tle-oneweb.txt) ─────────────
        // 12 orbital planes × 49 satellites = 588 total.
        cfg.ringUp   = {12, 61, 110, 159, 208, 257, 306, 355, 404, 453, 502, 551};
        cfg.ringDown = {37, 86, 135, 184, 233, 282, 331, 380, 429, 478, 527, 576};
        // Seam right = orbit11 pos (ring-up and ring-down initiators)
        // Seam left  = orbit0  pos (ring-up and ring-down terminuses)
        cfg.seamRight = {551, 576};   // orbit11: pos12 (ring-up), pos37 (ring-down)
        cfg.seamLeft  = {12,   37};   // orbit0:  pos12 (ring-up), pos37 (ring-down)

        cfg.safetyMarginS     = 50.0;

    } else if (name == "starlink"){
        // Walker-Delta (36 orbits x 20 satellites): no seam, closed rings.
        // ringUp / ringDown list the EXIT satellite of every orbit, all at the
        // same position (UP: position 0, DOWN: position 10). The entry of the
        // kink orbit (UP: orbit 0, position 1; DOWN: orbit 35, position 9)
        // follows from the ISL shift between orbit 35 and orbit 0 and is not
        // listed (see routing-ring-switch-walker-delta.cc, overview 2).
        cfg.closedRing = true;
        for (uint32_t o = 0; o < 36; ++o) {
            cfg.ringUp.insert   (o * 20 + 0);
            cfg.ringDown.insert (o * 20 + 10);
        }

        // RingSwitchScheduler (closed ring): seamLeft* = first satellite of
        // the ring ISL walk (right partners, all 36 ring ISLs), the switch is
        // fired on the down neighbour of seamRight* (= new exit of the kink
        // orbit, see routing-ring-switch-walker-delta.cc, overview 5).
        //   UP  : ring ISLs (0,0)->(1,0)->...->(35,0)->(0,1); kink orbit 0,
        //         exit sat 0 -> fired on sat 19, 18, ...
        //   DOWN: ring ISLs (35,9)->(0,10)->(1,10)->...->(35,10); kink orbit
        //         35, exit sat 710 -> fired on sat 709, 708, ...
        cfg.seamRightUp   = 0;
        cfg.seamLeftUp    = 0;
        cfg.seamRightDown = 35 * 20 + 10;   // 710
        cfg.seamLeftDown  = 35 * 20 + 9;    // 709

        cfg.safetyMarginS     = 10.0;

    }

    return cfg;
}

} // namespace ns3
