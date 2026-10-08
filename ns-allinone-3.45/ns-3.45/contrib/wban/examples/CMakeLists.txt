build_lib_example(
  NAME wban-example
  SOURCE_FILES wban-example.cc
  LIBRARIES_TO_LINK ${libwban}
)

build_lib_example(
  NAME wban-phy-test
  SOURCE_FILES wban-phy-test.cc
  LIBRARIES_TO_LINK ${libwban}
)

build_lib_example(
  NAME wban-per-experimental
  SOURCE_FILES wban-per-experimental.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)

build_lib_example(
  NAME wban-error-model-plot
  SOURCE_FILES wban-error-model-plot.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)

build_lib_example(
  NAME wban-per-vs-rx-signal-plot
  SOURCE_FILES wban-per-vs-rx-signal-plot.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)

build_lib_example(
  NAME wban-per-vs-ebn0-psdu-plot
  SOURCE_FILES wban-per-vs-ebn0-psdu-plot.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)
build_lib_example(
  NAME wban-per-vs-ebn0-plcp-plot
  SOURCE_FILES wban-per-vs-ebn0-plcp-plot.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)
build_lib_example(
  NAME wban-packet-print
  SOURCE_FILES wban-packet-print.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)

build_lib_example(
  NAME wban-propagation-loss
  SOURCE_FILES wban-propagation-loss.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)
build_lib_example(
  NAME wban-propagation-plot
  SOURCE_FILES wban-propagation-plot.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)

build_lib_example(
  NAME wban-propagation-test
  SOURCE_FILES wban-propagation-test.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)
build_lib_example(
  NAME wban-helper-test
  SOURCE_FILES wban-helper-test.cc
  LIBRARIES_TO_LINK ${libwban}
                    ${libstats}
)
