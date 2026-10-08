Wireless Body Area Network
==============================

This is the [ns-3](https://www.nsnam.org) WBAN module.
This module describes the implementation of the WBAN PHY layer in accordance with standard specifications. 
It also includes the BodyPropagationLossModel, which calculates path loss and evaluates the interference caused by the presence of a human body within the network.
The present module can be added to ns-3 in a plug-in fashion. For details about this module look at the documentation in the doc/ folder.


License
-------

This software is licensed under the terms of the GNU GPLv2, as those used by ns-3.
See the LICENSE file for more details.

Copyright (c) 2025 Ritsumeikan University, Japan
SPDX-License-Identifier: GPL-2.0-only

Papers
------

The following is a list of papers published in relation to this module:

[1. D. Oza, A. G. Ramonet, M. Yoshida and T. Noguchi, "IEEE 802.15.6: Physical Layer Implementation and Evaluation of Medical Bands for ns-3,"2023 28th Asia Pacific Conference on Communications (APCC), Sydney, Australia, 2023, pp. 99-106, doi: 10.1109/APCC60132.2023.10460708.](https://ieeexplore.ieee.org/document/10460708)

2. D. Oza, A. G. Ramonet, M. Yoshida and T. Noguchi, 2025, Inside Human Body Propagation Model for WBAN in ns-3 (To be published).

Author
------

Drishti Oza


Versions
--------

The following is table with the supported versions of ns-3
for each WBAN release, other versions might work but they
are untested:

| WBAN Version |  ns-3 version   | Build system |  Release date  |
|--------------|-----------------|--------------|-----------------
|     v1.0     |    ns-3.45      |    cmake     | July 11th 2025 |            
|     v1.0     |    ns-3.44      |    cmake     | July 11th 2025 |


Installation
------------

Download ns-3 and navigate to the contrib folder, then download the WBAN module
and switch to the desired WBAN version. For example::

    cd contrib
    git clone https://gitlab.com/DrishtiOza/wban.git
    cd wban
    git checkout v1.0
