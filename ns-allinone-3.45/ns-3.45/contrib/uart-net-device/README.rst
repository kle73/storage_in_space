UART NetDevice module
=====================

This is the `ns-3 <https://www.nsnam.org>`_ uart-net-device module
used to add co-processor capabilities to the ns-3 network simulator. The present
module can be added to ns-3 in a plug-in fashion. For details about this module look at the documentation in the doc/ folder.

License
-------

This software is licensed under the terms of the GNU GPLv2, as those used by ns-3.
See the LICENSE file for more details.

Copyright (c) 2025 Tokushima University, Japan.
SPDX-License-Identifier: GPL-2.0-only

Papers
------

The following is a list of papers published in relation to this module:

Alberto Gallegos Ramonet, Tommaso Pecorella and Kazuhiko Kinoshita. 2025. Network Co-Processor Solutions for IoT Designs on ns-3


Author
------

Alberto Gallegos Ramonet


Versions
--------

The following is table with the supported versions of ns-3
for each uart-net-device release, other versions might work but they
are untested:

.. table::
   :align: center
   :widths: auto

   +--------------------------+------------------+---------------+--------------------+------------------+
   | uart-net-device version  | ns-3 version     | Build system  | CommonShim version |  Release date    |
   +==========================+==================+===============+====================+==================+
   | v1.0                     | ns-3.45          | cmake         | v1.0               | June 2nd, 2025   |
   +--------------------------+------------------+---------------+--------------------+------------------+
   | v1.0                     | ns-3.44          | cmake         | v1.0               | June 2nd, 2025   |
   +--------------------------+------------------+---------------+--------------------+------------------+

Prerequisites
-------------

This model requires ns-3.44 or later to work and requires the external library libboost 1.74 or later to work. This is because ``boost:asio`` library is used to perform the serial communications used by this model.

You can install the Boost library in a variety of ways, for instance using the ``apt-get`` command::

    sudo apt-get install libboost1.74-dev

The present model has been designed to work on top of real NXP JN5169 device containing our original `CommonShimLayer`. For instructions of how to prepare JN5169 devices with the CommonShimLayer follow the instructions in the CommonShimLayer repo:

https://gitlab.com/shattered.feelings/jn51xx.git

**Note**: It is not necessary to compile the CommonShimLayer on your own, you can simply use the precompiled ``CommonShim_JN5169.bin`` file and installing it into your JN5169 device by using one the available flash programmers.

Installation
------------

Download ns-3 and navigate to the contrib folder, then download the uart-net-device module
and switch to the desired uart-net-device version. For example::

    cd contrib
    git clone https://gitlab.com/shattered.feelings/uart-net-device.git
    cd uart-net-device
    git checkout v1.0

