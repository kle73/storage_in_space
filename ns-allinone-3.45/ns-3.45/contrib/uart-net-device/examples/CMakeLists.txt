build_lib_example(
  NAME uart-example
  SOURCE_FILES uart-example.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uart-energy-scan
  SOURCE_FILES uart-energy-scan.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uart-sensor
  SOURCE_FILES uart-sensor.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uart-orphan-scan
  SOURCE_FILES uart-orphan-scan.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uart-sixlowpan-ping
  SOURCE_FILES uart-sixlowpan-ping.cc
  LIBRARIES_TO_LINK
    ${libnetwork}
    ${libsixlowpan}
    ${libinternet}
    ${liblr-wpan}
    ${libinternet-apps}
)

build_lib_example(
  NAME uart-zigbee-example
  SOURCE_FILES uart-zigbee-example.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
                    ${libzigbee}
)

build_lib_example(
  NAME uart-assoc-exp
  SOURCE_FILES uart-assoc-exp.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
)

build_lib_example(
  NAME uart-zigbee-exp
  SOURCE_FILES uart-zigbee-exp.cc
  LIBRARIES_TO_LINK ${liblr-wpan}
                    ${libzigbee}
)
