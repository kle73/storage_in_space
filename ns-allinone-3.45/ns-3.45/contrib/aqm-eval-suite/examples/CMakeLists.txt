build_lib_example(
  NAME TCPFriendlySameInitCwnd
  SOURCE_FILES tcp-friendly-same-initcwnd.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME TCPFriendlyDifferentInitCwnd
  SOURCE_FILES tcp-friendly-different-initcwnd.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME AggressiveTransportSender
  SOURCE_FILES aggressive-transport-sender.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME HeavyCongestion
  SOURCE_FILES heavy-congestion.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME MediumCongestion
  SOURCE_FILES medium-congestion.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME MildCongestion
  SOURCE_FILES mild-congestion.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME RttFairness
  SOURCE_FILES rtt-fairness.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME UnresponsiveTransport
  SOURCE_FILES single-unresponsive-transport.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME UnresponsiveWithFriendly
  SOURCE_FILES unresponsive-with-friendly-transport.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME VaryingCongestion
  SOURCE_FILES vary-congestion-level.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME VaryingBandwidthUno
  SOURCE_FILES vary-available-capacity-uno.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME VaryingBandwidthDuo
  SOURCE_FILES vary-available-capacity-duo.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME LbeTransportSender
  SOURCE_FILES LBE-transport-sender.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)

build_lib_example(
  NAME aqm-eval-suite-runner
  SOURCE_FILES aqm-eval-suite-runner.cc
  LIBRARIES_TO_LINK
    ${libaqm-eval-suite}
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libpoint-to-point-layout}
)
