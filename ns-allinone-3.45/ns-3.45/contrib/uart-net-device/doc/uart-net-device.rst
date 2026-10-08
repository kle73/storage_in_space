UartLrWpanNetDevice
===================

.. include:: replace.txt
.. highlight:: cpp

.. heading hierarchy:
   ============= Module Name
   ------------- Section (#.#)
   ~~~~~~~~~~~~~ Subsection (#.#.#)

The ``UartLrwpanNetDevice`` is a NetDevice designed to provide the means for |ns3| to interact with real IEEE 802.15.4 hardware.
More specifically, it provides the necessary wrapping code to interact with real devices using a network co-processor (i.e., An MCU).
This co-processor communicates with |ns3| via Universal Asynchronous Receive-Transmitter (UART) protocol.::

                                           +-------- Z-wave      |
                                           |                     |
    +-------------------------------+      +-------- Thread      | Handled by the Host processor
    | Ns-3 protocol stacks and Apps | -----|                     |      (A device running ns-3)
    +-------------------------------+      +-------- Zigbee      |
    |       UartLrWpanNetDevice     |                            |
    +-------------------------------+
             ^
             |  UART
             |
    +-------------------------------+      |
    |        Common Shim layer      |      |
    +-------------------------------+      |
    |  L2: IEEE 802.15.4-2006 MAC   |      |   Handled by the NXP MCU processor
    +-------------------------------+      |          (An NXP JN5169 device)
    |  L1: IEEE 802.15.4-2006 PHY   |      |
    +-------------------------------+      |

In its present form, the ``UartLrWpanNetDevice`` is able to communicate only with NXP JN5169 devices which include the ``Common Shim Layer``.
Support for other IEEE 802.15.4 MCUs is planned for future releases.
The ``Common Shim layer`` handles the UART communication on the MCU side while ``UartLrWpanNetDevice`` manage the UART communication on the |ns3| side.
Source code and detailed instructions of how to prepare an NXP JN5169 device with the ``Common Shim Layer`` are provided in [2_].


Both, ``UartLrWpanNetDevice`` and the ``Common Shim Layer`` were developed by Alberto Gallegos Ramonet, Tokushima University, Japan, 2025.

The intention of this proposal is to allow |ns3| development of more accurate IoT protocols and applications that can effectively operate on both simulated or real hardware.
A significant advantage that is result of this proposal is that it facilitates the creation of portable IoT devices testbeds on the fly either using a single host processor or multiple host processors.
For more details please consult [1].

The source code for the new module lives in the directory ``src/uart-net-device``.


Scope and Limitations
---------------------

* Not all IEEE 802.15.4 primitives are supported. For example, less used primitives such as disassociation or GTS related primitives are not supported.


Requirements
------------

This model requires ns-3.44 or later to work and requires the external library libboost 1.74 or later to work.
This is because ``boost:asio`` library is used to perform the serial communications used by this model.

The present model has been designed to work on top of NXP JN5169 containing our original `CommonShimLayer`.
For instructions of how to prepare these devices see [2_].

Usage
-----

Usage is simple and similar to other ``NetDevice`` available in |ns3|.

.. sourcecode:: cpp

   // We are using real devices, therefore, make sure you are using the real time simulator
   GlobalValue::Bind("SimulatorImplementationType",StringValue("ns3::RealtimeSimulatorImpl"));

   // Data Source Device
   Ptr<Node> node1 = CreateObject<Node>();
   Ptr<UartLrWpanNetDevice> dev1 =
   CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
   node1->AddDevice(dev1);
   dev1->GetMac()->McpsDataRequest(/*<Placeholder, data params here>*/);

   // Data Destination Device
   Ptr<Node> node2 = CreateObject<Node>();
   Ptr<UartLrWpanNetDevice> dev2 =
   CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB1");
   node2->AddDevice(dev2);

In this example, two real NXP JN5169 transceiver devices are physically connected to a single host with ports ``/dev/ttyUSB0`` and ``/dev/ttyUSB1`` respectively.
This information is declared in the constructor of the ``UartLrWpanNetDevice`` object. Allocation of the Node objects have no impact in the results.
Nodes are use simply for reference and context.


Besides of what is explained above, installation of |ns3| protocol stacks and applications is like any other ns-3 script.

Helpers
~~~~~~~

While it is not necessary to use a helper to setup a ``UartLrWpanNetDevice`` as shown in the previous section;
occasionally, it is convenient to use a helper to quickly setup multiple physical devices connected to a single host.
A simple setup process can be achieved using the provided helper.

.. sourcecode:: cpp

   // ...
   // Install and configure x amount of UartLrWpanNetDevices
   // in a single host according to the number of nodes:
   // /dev/ttyUSB0 .. /dev/ttyUSBn
   NodeContainer nodes;
   nodes.Create(numNodes);
   NetDeviceContainer netDevices;
   UartLrWpanHelper uartLrWpanHelper;
   netDevices = uartLrWpanHelper.Install(nodes);

In the example shown above, the helper will setup a ``UartLrWpanNetDevice`` for each ``Node`` found in the ``NodeContainer``.
The port assigned to each device will be the prefix ``/dev/ttyUSB`` and an index starting from 0 and increasing in ascending order.
Naturally, this assumes there is a corresponding number of nodes with the amount of physical JN5169 devices plugged to the host computer running |ns3|.

Attributes
~~~~~~~~~~

Not applicable.

Traces
~~~~~~

Not applicable.

Examples and Tests
------------------

The following examples are provided:

* ``uart-energy-scan.cc``: Demonstrates the simple use of the model by issuing an energy scan with the IEEE 802.15.4 MLME-SCAN.request primitive (1 device required).
* ``uart-orphan-scan.cc``: Similar to the previous example but using an orphan scan (2 devices required).
* ``uart-example.cc``: Demeonstrates an IEEE 802.15.4 association process and data transmission (2 devices required).
* ``uart-zigbee-example.cc``: Use real devices on top of |ns3| Zigbee networks. In the example, network initialization, join devices, routing and data transmission  is shown (2 or more devices required).
* ``uart-sensor.cc``: Obtain sensor data directly from JN5169 devices that supports this feature (i.e. Twelite ARIA, Twelite CUE).

No unit test are included in this project because real devices might not be connected and available during the test checking phase.

Validation
----------

Experimental validation has been done in [1].

References
----------

[1] Alberto Gallegos Ramonet, Tommaso Pecorella, Kazuhiko Kinoshita. 2025. Network Co-Processor Solutions for IoT Designs on ns-3 (To be published).

[`2 <https://gitlab.com/shattered.feelings/jn51xx>`_] Common Shim layer for NXP JN5169, Alberto Gallegos Ramonet, 2025.
