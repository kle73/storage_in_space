IEEE 802.15.6: Wireless Body Area Network (WBAN)
-------------------------------------------------------------------------

.. include:: replace.txt
.. highlight:: cpp

.. heading hierarchy:
   ------------- Chapter
   ************* Section (#.#)
   ============= Subsection (#.#.#)
   ############# Paragraph (no number)

The WBAN module for |ns3| outlines the implementation of the WBAN PHY layer according to the standard specifications.
This module also features a body propagation loss model that computes path loss and assesses the interference caused by the presence of a human body within the network.
WBANs operate primarily on three frequency bands: Narrowband (NB), Ultra-Wide Band (UWB), and Human Body Communication (HBC) band.

This module supports the Narrowband (NB) frequency band outlined in the WBAN standard.
The NB band consists of seven operating frequency bands, each offering various sub-bands with different data rates and modulation schemes.
Specifically, this module implements six NB frequencies that utilize Differential Binary Phase Shift Keying (DBPSK) modulation, along with their corresponding data rates.
Following the WBAN standard, this module also incorporates Bose, Chaudhuri, and Hocquenghem (BCH) coding, which is used for error correction.
The WBAN module interfaces and structure are patterned after the Lr-Wpan module in |ns3|.

Both, the WBAN physical layer and BodyPropagationLossModel were developed by Drishti Oza, Network Systems Laboratory, Ritsumeikan University, Japan, 2025.::

    +----------+                                            +-------------------------------+
    |   WBAN   | -------|                                   |      PropagationLossModel     |
    +----------+        |                                   +-------------------------------+
                        |                                                 ^
                        |                                                 |
    +---------+         |                                                 |
    | Lr-Wpan | --------|                                   +-------------------------------+
    +---------+         |-----------------------------------|    BodyPropagationLossModel   |
                        |                                   +-------------------------------+
                        |
    +----------+        |
    |   Wi-Fi  | -------|
    +----------+

PHY Layer
*****************
The current error rate model for IEEE 802.15.6 is designed for a DBPSK (Differential Binary Phase Shift Keying) modulation scheme in an AWGN
(Additive White Gaussian Noise) channel. This physical layer (Phy) model is based on SpectrumPhy and adheres to the specifications outlined in
Section 8.8 of IEEE Std 802.15.6-2012. The noise power density assumes uniformly distributed thermal noise across the frequency bands.
The loss model can fully utilize all existing simple (non-spectrum PHY) loss models. The Phy model uses the existing single-spectrum channel model.
The physical layer is modelled on the packet level, that is, no preamble/SFD detection is done.
For WBAN, the preamble and PLCP header are transmitted at different data rates and also follow different error-correcting codes than the PSDU.
For NB, BCH (63, 51, t = 2) is used for the PSDU part of the packet and BCH (31, 19, t = 2) for the PLCP and preamble transmission.
Rx sensitivity refers to the minimum signal strength at which a receiver can successfully receive and decode a packet with a high success rate.
According to the standard (IEEE Std 802.15.6-2012, section 8.9.2), this is defined as the point where the packet error rate is below 10% for a 255-byte PSDU.

The maximum theoretical sensitivity values for the implemented NB frequencies are as follows:
-119.20 dBm for the 402 to 405 MHz,  -117.95 dBm for the 863 to 870 MHz, 902 to 928 MHz, and 950 to 958 MHz, and
-113.97 dBm for the 2360 to 2400 MHz and 2400 to 2483.5 MHz.
By default, the receiver sensitivity is set to the above-mentioned maximum theoretically possible value.
However, the receiver sensitivity can be adjusted to different levels using the SetRxSensitivity function in the PHY.
This allows to simulate the hearing capabilities of different compliant radio transceivers.
The minimum standard-compliant receiver sensitivity for all NB frequencies is detailed in Table 52, section 8.9.1.
The example wban-per-experimental.cc shows that at a given Rx sensitivity, packets are dropped regardless of their theoretical error probability.
This program outputs a file named 802.15.6-per-vs-rxSignal.plt.
Loading this file into gnuplot yields a file 802.15.6-per-vs-rsSignal.eps, which can be converted to PDF or other formats.
Packet payload size, Tx power, and Rx sensitivity can be configured. The point where the blue line crosses the PER indicates the Rx sensitivity.
The default output is shown below.

//.. _fig-lr-wpan-phy:

.. figure:: figures/802.15.6-per-vs-rxSignal.*

Propagation Loss Model
**************************

The propagation loss model developed for WBAN is called the BodyPropagationLossModel, which is based on the PropagationLossModel in |ns-3|.
This model for human body propagation consists of two main components: a layered structure and the dielectric properties of tissues.

Layered structure
======================

The first layer is the skin, which is the outermost layer of the propagation loss model. Beneath the skin are the fat and muscle layers, followed by the organ layer.
Note that the thickness of the fat and muscle layers vary among individuals.
To enhance the robustness of our propagation model, we provide an option to adjust the number of fat and muscle layers.
As shown below, multiple fat and muscle layers can be incorporated, providing users with the flexibility to modify their body composition based on their experimental scenarios.::

    +----------+----------+----------+---------+
    |   SKIN   |   FAT    |  MUSCLE  |  ORGAN  |
    +----------+----------+----------+---------+
                    +            +
                    |            |
              +----------+   +----------+
              | n layers |   | m layers |
              |  of fat  |   |of muscle |
              +----------+   +----------+

Dielectric properties
========================

The Dielectric properties human tissues describe how the tissues respond to an electric field.
These properties play an important role in in-body propagation loss modeling because they determine the absorption and flow of electrons within the body.
Human tissue has two primary dielectric properties.
 1. Permittivity: A measure of how easily the tissue can store an electric charge and resist an electric field.
 2. Conductivity: A measure of how easily electrons can flow through a tissue.

Both permittivity and conductivity change according to the communication frequency and the specific tissue through which the signal propagates.
The values of permittivity and conductivity [3_] help to calculate the attenuation loss using the following equation [2]:

.. math::

    \alpha \approx \frac {520.8 \pi \theta}{\sqrt{\epsilon_{r}}} * d.

With:
 :math:`\alpha` :  attenuation constant in dB

 :math:`\theta` : conductivity of human tissue

 :math:`\epsilon_{r}` : relative permittivity of human tissue

 :math:`d` :  thickness of the tissues in the path of the signal

BodyPropagationLossModel is part of the WBAN physical layer simulated in ns-3.
However, it is not limited to the WBAN physical layer; it can be implemented with any standard and its corresponding physical layer in ns-3.

Usage
**************************
The helper is patterned after other device helpers.
The WbanHelper, which is designed for implementing BodyPropagationLossModel within the WBAN PHY layer.
By default, the helper installs the desired number of nodes at a frequency of 2.4 GHz.
The BodyPropagationLossModel is automatically set as the default propagation loss model, with the initial value of SetBodyOption configured as a fat layer.
If a different organ must be specified, users can modify it using the function SetBodyOption.
To set the default parameters as in helper:

.. sourcecode:: cpp

    // Create 2 nodes , and a NetDevice for each node.
    NodeContainer nodes;
    nodes.Create(2);
    // Use WbanHelper to create devices, assign nodes.
    WbanHelper WbanHelper;
    // BodyPropagationLossModel assigned to the channel by default after using WbanHelper.
    NetDeviceContainer devices = WbanHelper.Install(nodes);
    Ptr<WbanNetDevice> dev0 = devices.Get(0)->GetObject<WbanNetDevice>();
    Ptr<WbanNetDevice> dev1 = devices.Get(1)->GetObject<WbanNetDevice>(); // Set Mobility
    Ptr<ConstantPositionMobilityModel> mob0 = CreateObject<ConstantPositionMobilityModel>();
    dev0->GetPhy()->SetMobility(mob0);
    Ptr<ConstantPositionMobilityModel> mob1 = CreateObject<ConstantPositionMobilityModel>();

To change the organ using SetBodyOption:

.. sourcecode:: cpp

    // Implementation using class BodyPropagationLossModel.
    Ptr<BodyPropagationLossModel>propModelBody = CreateObject<BodyPropagationLossModel>();
    // Assign BodyPropagationLossModel to the channel.
    channel->AddPropagationLossModel(propModelBody);
    // Change the default value of m_bodyOption if necessary.
    propModelBody->SetBodyOptions(BodyOrganOption::LARGE_INTESTINE_2400_MHZ);
    // To change the default values of fat and muscle layers  from 1 layer to the desired number of layers.
    propModelBody->SetFatLayer(3);    // Changed to 3 layers of  fat.
    propModelBody->SetMuscleLayer(2); // Changed to 2 layers of muscle.
    // propModelBody goes into the of any standard.

Examples
**************************

The following examples have been written:

* ``wban-phy-test.cc``: An example to test the PHY.
* ``wban-error-model-plot.cc``: An example to test the PHY.
* ``wban-packet-print.cc``: An example to print out the PHY header fields.
* ``wban-per-experimental.cc``: An example to plot the theoretical and experimental packet error rate(PER) as a function of the receive signal.
* ``wban-helper-test.cc``: An example to implement WBAN PHY and BodyPropagationLossModel using helper.
* ``wban-propagation-loss.cc``: An example to implement WBAN PH and BodyPropagationLossModel without helper.
* ``wban_propagation-plot.cc``: An example to show using gnuplot how the Packet success rate is affected by the introduction of BodyPropagationLossModel.
* ``wban-propagation-test.cc``: An example in comparison with the existing study mentioned in the International conference on NS3 2025 paper.
* ``wban-per-vs-ebn0-plcp-plot.cc``: an example to test PER as a function of Eb/N0 for the PSDU data rates of NB frequencies [1_].
* ``wban-per-vs-ebn0-plcp-plot.cc``: an example to test PER as a function of Eb/N0 for the PSDU data rates of NB frequencies [1_].
* ``wban-per-vs-rx-signal-plot.cc``: an example to test PER as a function of RX signal, where the |ns3| implementation matches the standard-defined Rx sensitivity for implemented NB frequencies [1_].

Scope and Limitations
**************************
* Only PHY layer was implemented, no MAC layer considered.
* Only six NB frequencies are considered with DBPSK modulation, other frequency bands and modulation schemes are not implemented.
* Did not detect PLCP and preamble, rather, we implemented them in terms of time taken for transmission.
* The BodyPropagationLossModel did not account for interference from co-located devices, cross-channel fading, or multiple WBANs within the network.

References
**************************
[`1 <https://ieeexplore.ieee.org/document/10460708>`_] D. Oza, A. G. Ramonet, M. Yoshida and T. Noguchi, "IEEE 802.15.6: Physical Layer Implementation and Evaluation of Medical Bands for ns-3,"
2023 28th Asia Pacific Conference on Communications (APCC), Sydney, Australia, 2023, pp. 99-106, doi: 10.1109/APCC60132.2023.10460708.

[2] D. Oza, A. G. Ramonet, M. Yoshida and T. Noguchi, 2025, Inside Human Body Propagation Model for WBAN in ns-3 (To be published).

[`3 <http://niremf.ifac.cnr.it/tissprop/htmlclie/htmlclie.php>`_]InstituteforApplied.Physics.DielectricPropertiesofBodyTissues.ItalianNational Research Council.
