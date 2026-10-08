set(source_files
    model/eval-topology.cc
    model/eval-app.cc
    helper/aqm-eval-suite-helper.cc
    helper/aqm-eval-suite-output-manager.cc
    helper/aqm-eval-suite-plot-manager.cc
)

set(header_files
    model/eval-topology.h
    model/eval-app.h
    helper/aqm-eval-suite-helper.h
    helper/aqm-eval-suite-output-manager.h
    helper/aqm-eval-suite-plot-manager.h
)

build_lib(
  LIBNAME aqm-eval-suite
  SOURCE_FILES ${source_files}
  HEADER_FILES ${header_files}
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libapplications}
    ${libpoint-to-point}
    ${libpoint-to-point-layout}
)
