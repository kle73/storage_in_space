check_include_file_cxx(
  stdint.h
  HAVE_STDINT_H
)
if(HAVE_STDINT_H)
  add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
  set(examples_as_tests_sources # test/wban-examples-test-suite.cc
  )
endif()

build_lib(
  LIBNAME wban
  SOURCE_FILES
    model/wban.cc
    helper/wban-helper.cc
    model/wban-error-model.cc
    model/wban-interference-helper.cc
    model/wban-lqi-tag.cc
    model/wban-spectrum-signal-parameters.cc
    model/wban-spectrum-value-helper.cc
    model/wban-phy-header.cc
    model/wban-phy.cc
    model/wban-net-device.cc
    model/wban-propagation-model.cc
    model/wban-channel.cc
  HEADER_FILES
    model/wban.h
    helper/wban-helper.h
    model/wban-error-model.h
    model/wban-interference-helper.h
    model/wban-lqi-tag.h
    model/wban-spectrum-signal-parameters.h
    model/wban-spectrum-value-helper.h
    model/wban-phy-header.h
    model/wban-phy.h
    model/wban-net-device.h
    model/wban-propagation-model.h
    model/wban-channel.h
  LIBRARIES_TO_LINK
    ${libnetwork}
    ${libcore}
    ${libmobility}
    ${libspectrum}
    ${libpropagation}
  TEST_SOURCES test/wban-test-suite.cc
               ${examples_as_tests_sources}
)
