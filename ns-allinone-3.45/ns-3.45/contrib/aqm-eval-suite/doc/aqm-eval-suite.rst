AQM Evaluation Suite
====================

.. include:: replace.txt
.. highlight:: cpp

The AQM Evaluation Suite is a comprehensive automated framework for evaluating and
comparing Active Queue Management (AQM) algorithms in ns-3. The suite implements
standardized evaluation scenarios described in RFC 7928 and provides systematic
performance analysis through automated simulation setup, topology creation, traffic
generation, execution, results collection, and visualization.

**Key Features:**

* **RFC 7928 Compliance**: Implements standardized AQM evaluation scenarios
* **Automated Pipeline**: Complete workflow from setup to visualization
* **Multi-AQM Support**: Simultaneous evaluation of 11+ queue disciplines
* **Rich Metrics**: Queue delay, goodput, throughput, and drop statistics
* **Visual Analytics**: Automatic ellipse plots and time-series graphs
* **Extensible Design**: Easy integration of custom AQM algorithms

The AQM Evaluation Suite is organized into three main components:

**Model Layer:**
Implements core evaluation classes including ``EvaluationTopology`` for network setup,
``EvalApp`` for traffic generation, and ``EvalTimestampTag`` for metrics collection.

**Helper Layer:**
Provides the ``ScenarioImpl`` base class that standardizes scenario creation and
execution across all evaluation scenarios.

**Utilities Layer:**
Contains Python scripts for post-processing simulation results and generating
visualization plots according to RFC 7928 guidelines.

The ``src/aqm-eval-suite/model`` directory contains three primary classes:

* **class EvaluationTopology**: This class has three major functionalities:

  * Creating the topology: It sets up a point-to-point dumbbell topology by using
    ``PointToPointDumbbellHelper`` with required number of nodes, and
    configures the data rate for all the links. It also installs the desired queue
    discipline on the router node.

  * Installing application on nodes: it provides an API for configuration of
    applications. This API takes the application parameters such as data rate,
    packet size, transport protocol, initial congestion window in case of TCP,
    maximum bandwidth and one way delay of the channels.

  * Getting the metrics of interest from the experiment: It uses the trace
    sources provided by different classes of ns-3 for the calculation of metrics.
    The metrics recommended in the RFC are queue-delay, goodput, throughput and
    number of drops.

* **class EvalApp**: This class is based on ``OnOffApplication`` and is used for
  generating TCP and UDP traffic in the suite. The native class ``OnOffApplication``
  in ns-3 creates sockets at the start time of an application. Thus, to configure
  different values for parameters like initial congestion window in TCP is non-trivial,
  since they cannot be configured before the socket is created and after the application
  starts. To overcome this, ``EvalApp`` is implemented on the same principles as that
  of the ``OnOffApplication`` in which a socket is created and the application is
  started only after its parameters are configured.

* **class EvalTimestampTag**: This is a subclass of ``Tag`` and has been developed
  to fetch the queue delay information from ``QueueDisc``. When a packet is enqueued
  by the QueueDisc, this tag is added with a timestamp (as the enqueue time) and when
  the packet is dequeued, the queue delay is computed as the difference between the
  dequeue time and the enqueue time.

The ``src/aqm-eval-suite/helper`` directory contains the ``ScenarioImpl`` class that
implements the following two methods:

* ``ScenarioImpl::CreateScenario()``: This is a virtual function implemented by
  each scenario according to the topology and traffic profiles mentioned in the RFC.

* ``ScenarioImpl::RunSimulation()``: This method takes the scenario created by
  each subclass and runs them with all the queue disciplines available in ns-3.

The ``src/aqm-eval-suite/utils`` directory provides four Python scripts that take
performance metrics computed in the suite as input, and generate a graph with
Queuing Delay as the X-axis against Goodput as the Y-axis. The graph depicts an
ellipse which is plotted as per the guidelines mentioned in the RFC. The
covariance between the queuing delay and goodput is determined by the
orientation of the ellipse, and helps to analyze the effect of traffic load on
Goodput and Queuing Delay.

.. figure:: ../figures/qdel-goodput.png
   :alt: Queue delay vs Goodput ellipse plot
   :align: center
   
   Example ellipse plot showing the relationship between queue delay and goodput for different AQM algorithms

Scope and Limitations
---------------------

The current implementation has the following limitations:

* Limited to point-to-point dumbbell topology
* Requires external dependencies (Python, Gnuplot, ImageMagick)

The ``src/aqm-eval-suite/utils`` directory provides four Python scripts that take
performance metrics computed in the suite as input, and generate a graph with
Queuing Delay as the X-axis against Goodput as the Y-axis. The graph depicts an
ellipse which is plotted as per the guidelines mentioned in the RFC. The
covariance between the queuing delay and goodput is determined by the
orientation of the ellipse, and helps to analyze the effect of traffic load on
Goodput and Queuing Delay.

Supported Scenarios
-------------------

The suite implements standardized evaluation scenarios from RFC 7928:

**Basic AQM Scenarios (Section 5):**

* 5.1.1 - TCPFriendlySameInitCwnd: TCP flows with identical initial congestion windows
* 5.1.2 - TCPFriendlyDifferentInitCwnd: TCP flows with varying initial congestion windows
* 5.2 - AggressiveTransportSender: Single aggressive TCP flow (CUBIC variant)
* 5.3.1 - UnresponsiveTransport: Single UDP flow without congestion control
* 5.3.2 - UnresponsiveWithFriendly: UDP flow competing with TCP traffic
* 5.4 - LbeTransportSender: Low Bandwidth-Delay Product transport evaluation

**Congestion Level Scenarios (Section 8):**

* 8.2.2 - MildCongestion: Light traffic load (0.02 × BDP flows)
* 8.2.3 - MediumCongestion: Moderate traffic load (0.06 × BDP flows)
* 8.2.4 - HeavyCongestion: Heavy traffic load (0.114 × BDP flows)
* 8.2.5 - VaryingCongestion: Dynamic congestion level changes
* 8.2.6.1 - VaryingBandwidthUno: Single flow with varying available capacity
* 8.2.6.2 - VaryingBandwidthDuo: Two flows with varying available capacity

**RTT Fairness Scenarios (Section 6):**

The RTT fairness evaluation consists of 15 different scenarios with varying
round-trip times to assess how AQM algorithms handle flows with different
propagation delays.

Usage
-----

The AQM Evaluation Suite provides a user-friendly interface for running individual
scenarios or comprehensive evaluation campaigns.

Prerequisites
~~~~~~~~~~~~~

**System Dependencies:**

.. code-block:: bash

   sudo apt update
   sudo apt install gnuplot-qt imagemagick

**Python Dependencies:**

.. code-block:: bash

   pip3 install numpy

**Note:** Please refer to the **"Using Python to Run |ns3|"** documentation for complete Python environment setup instructions.

Running Scenarios
~~~~~~~~~~~~~~~~~~

**Single Scenario by RFC Section:**

.. code-block:: bash

   ./ns3 run "aqm-eval-suite-runner --number=5.2"

**Single Scenario by Name:**

.. code-block:: bash

   ./ns3 run "aqm-eval-suite-runner --name=AggressiveTransportSender"

**All Scenarios:**

.. code-block:: bash

   ./ns3 run "aqm-eval-suite-runner --name=All"

**With Custom Parameters:**

.. code-block:: bash

   ./ns3 run "aqm-eval-suite-runner --name=MildCongestion --ecn=true --isBql=true"

Extension and Customization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The AQM Evaluation Suite is designed for extensibility, allowing researchers to
integrate custom AQM algorithms and create new evaluation scenarios.

**Creating Custom Scenarios**

To implement a new evaluation scenario, inherit from ``ScenarioImpl`` and override
the ``CreateScenario()`` method:

.. code-block:: cpp

   class MyCustomScenario : public ScenarioImpl
   {
   protected:
       EvaluationTopology CreateScenario(std::string aqm, bool isBql) override
       {
           // Define network parameters
           PointToPointHelper pointToPoint;
           pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
           pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

           // Create topology with custom flow count
           uint32_t nflows = 8;
           EvaluationTopology et("CustomScenario", nflows, pointToPoint,
                                 aqm, 1500, isBql, GetBaseOutputDir());

           // Configure traffic flows
           for (uint32_t i = 0; i < nflows; ++i)
           {
               ApplicationContainer ac = et.CreateFlow(
                   StringValue("1ms"), StringValue("1ms"),
                   StringValue("1000Mbps"), StringValue("1000Mbps"),
                   "ns3::TcpNewReno", 0, DataRate("10Mbps"), 10);

               ac.Start(Seconds(i * 5.0));
               ac.Stop(Seconds(300));
           }

           return et;
       }
   };

**Adding New AQM Algorithms**

The suite automatically tests all available queue disciplines in ns-3. To evaluate
a custom AQM algorithm:

1. Implement your AQM as a QueueDisc subclass in ns-3
2. Register it with the AqmEvalConfig and ScenarioImpl
3. The evaluation suite will automatically include it in test runs

Helpers
~~~~~~~

The primary helper class is ``ScenarioImpl`` which provides the base interface
for creating and running evaluation scenarios:

.. code-block:: cpp

   // Create a custom scenario
   class MyCustomScenario : public ScenarioImpl
   {
   public:
       MyCustomScenario();
       ~MyCustomScenario() override;

   protected:
       EvaluationTopology CreateScenario(std::string aqm, bool isBql) override;
   };

The ``EvaluationTopology`` helper manages network topology creation and configuration:

.. code-block:: cpp

   // Configure topology parameters
   PointToPointHelper pointToPoint;
   pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
   pointToPoint.SetChannelAttribute("Delay", StringValue("25ms"));

   // Create evaluation topology
   uint32_t nflows = 5;
   EvaluationTopology et("ScenarioName", nflows, pointToPoint,
                         aqm, 1500, isBql, outputDir);

Attributes
~~~~~~~~~~

The main configurable attributes are:

* **QueueDiscMode**: Queue discipline operating mode (QUEUE_DISC_MODE_PACKETS, QUEUE_DISC_MODE_BYTES)
* **isBql**: Enable Byte Queue Limits (true/false)
* **ecn**: Enable Explicit Congestion Notification (true/false)
* **BaseOutputDir**: Base directory for output files (default: aqm-eval-output)
* **number**: Run scenario by RFC section number
* **name**: Run scenario by descriptive name

Traces
~~~~~~

The suite collects the following trace sources:

* **Queue delay**: Measured using EvalTimestampTag at enqueue/dequeue points
* **Goodput**: Application-layer throughput measurement
* **Throughput**: Network-layer data transmission rate
* **Drop statistics**: Packet drop counts and rates from queue disciplines

Examples and Tests
------------------

The ``src/aqm-eval-suite/examples`` directory provides example programs for each
RFC 7928 scenario:

* ``aqm-eval-suite-runner.cc``: Main runner program that executes all scenarios
* ``aggressive-transport-sender.cc``: Demonstrates RFC 5.2 aggressive transport evaluation
* ``tcp-friendly-same-initcwnd.cc``: Shows RFC 5.1.1 TCP fairness with identical initial windows
* ``tcp-friendly-different-initcwnd.cc``: Implements RFC 5.1.2 TCP fairness with different initial windows
* ``unresponsive-transport.cc``: Examples for UDP-based unresponsive transport scenarios
* ``mild-congestion.cc``: Light congestion scenario implementation
* ``medium-congestion.cc``: Moderate congestion scenario example
* ``heavy-congestion.cc``: Heavy congestion evaluation example
* ``rtt-fairness.cc``: RTT fairness evaluation across multiple delay configurations
* ``vary-available-capacity-*.cc``: Dynamic bandwidth variation scenarios

Validation
----------

The AQM Evaluation Suite has been validated against RFC 7928 specifications through:

* **Scenario Compliance**: All implemented scenarios follow RFC 7928 guidelines for topology, traffic patterns, and evaluation metrics
* **Metric Verification**: Queue delay, goodput, throughput, and drop statistics are collected according to standardized methodologies

.. figure:: ../figures/RedQueueDisc-delay.png
   :alt: RED Queue Discipline delay performance
   :align: center
   
   Queue delay performance comparison for RED queue discipline across different scenarios

.. figure:: ../figures/RedQueueDisc-goodput.png
   :alt: RED Queue Discipline goodput performance
   :align: center
   
   Goodput performance analysis for RED queue discipline implementation

* **Visualization Standards**: Generated plots comply with RFC 7928 visualization requirements, including delay-goodput ellipse representations
* **Multi-AQM Testing**: Comprehensive evaluation across 11+ queue disciplines including CoDel, FQ-CoDel, PIE, RED variants, and others
* **Academic Validation**: Results have been published and peer-reviewed in academic conferences [2]_

References
----------

.. [1] RFC 7928: "Characterization Guidelines for Active Queue Management (AQM)" https://tools.ietf.org/html/rfc7928

.. [2] Ankit Deepak, K. S. Shravya, and Mohit P. Tahiliani. 2017. Design and Implementation of AQM Evaluation Suite for ns-3. In Proceedings of the 2017 Workshop on ns-3 (WNS3 '17). Association for Computing Machinery, New York, NY, USA, 87–94. https://doi.org/10.1145/3067665.3067674
