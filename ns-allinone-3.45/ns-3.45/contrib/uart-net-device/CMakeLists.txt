check_include_files(
  "boost/asio.hpp"
  HAVE_BOOST_ASIO
  LANGUAGE
  CXX
)

if(${HAVE_BOOST_ASIO})
  add_definitions(-DHAVE_BOOST_ASIO)
  message(STATUS "Boost Asynchronous IO (ASIO) have been found.")
else()
  message(
    STATUS
      "Boost asio are required for UART operations"
      " Ubuntu ships it within the libboost-dev package."
      " You may need to clean up the CMake cache after installing it to pass this check."
  )
endif()

build_lib(
  LIBNAME uart-net-device
  SOURCE_FILES
    helper/uart-lr-wpan-helper.cc
    model/uart-lr-wpan-net-device.cc
    model/uart-lr-wpan-mac.cc
  HEADER_FILES
    helper/uart-lr-wpan-helper.h
    model/uart-lr-wpan-net-device.h
    model/uart-lr-wpan-mac.h
  LIBRARIES_TO_LINK ${libboost}
                    ${liblrwpan}
)
