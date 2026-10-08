/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wban-module.h"

#include <iostream>

using namespace ns3;
using namespace ns3::wban;

int
main(int argc, char* argv[])
{
    Packet::EnablePrinting();
    Packet::EnableChecking();
    NarrowBandPhyHeader nb;
    nb.SetRate(00);
    nb.SetLength(50);
    nb.SetBurstEnable();
    nb.SetSeedDisable();
    nb.SetHcs(0);
    nb.SetBchParity(00);
    Ptr<Packet> p = Create<Packet>(120); // 20 bytes of dummy data
    p->AddHeader(nb);
    nb.Print(std::cout);

    return 0;
}
