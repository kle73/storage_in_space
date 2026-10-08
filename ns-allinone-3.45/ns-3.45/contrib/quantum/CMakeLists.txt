include_directories(${QPP_INCLUDE_DIR}/qpp)
message(STATUS "Quantum++ include dir: ${QPP_INCLUDE_DIR}")

include_directories(${CryptoPP_INCLUDE_DIRS})
message(STATUS "Crypto++ include dir: ${CryptoPP_INCLUDE_DIRS}")

check_include_file_cxx(
  stdint.h
  HAVE_STDINT_H
)
if(HAVE_STDINT_H)
  add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
  set(examples_as_tests_sources # test/qkd-examples-test-suite.cc
  )
endif()

mark_as_advanced(ENABLE_QPP)

set(ENABLE_QPP
    False
    CACHE INTERNAL
          ""
)

mark_as_advanced(ENABLE_CRYPTOPP)

set(ENABLE_CRYPTOPP
    False
    CACHE INTERNAL
          ""
)

if(${NS3_QPP})
  if(PKG_CONFIG_FOUND)
    if(QPP_FOUND)
      message(STATUS "Quantum++ found")
      set(ENABLE_QPP
          True
          CACHE INTERNAL
                ""
      )
    else()
      message(FATAL_ERROR "Quantum++ not found")
    endif()
  else()
    message(
      FATAL_ERROR "PkgConfig not found. This is required to find Quantum++."
    )
  endif()
endif()

set(qpp_sources)
set(qpp_headers)
set(qpp_libraries)

if(${ENABLE_QPP})
  set(qpp_sources
      helper/quantum-helper.cc
  )
  set(qpp_headers
      helper/quantum-helper.h
  )
  add_definitions(-DENABLE_QPP_QKD)
  set(qpp_libraries
      ${LIB_AS_NEEDED_PRE}
      ${LIB_AS_NEEDED_POST}
  )
endif()

if(${NS3_CRYPTOPP})
  if(PKG_CONFIG_FOUND)
    if(CRYPTOPP_FOUND)
      message(STATUS "Crypto++ found")
      set(ENABLE_CRYPTOPP
          True
          CACHE INTERNAL
                ""
      )
    else()
      message(FATAL_ERROR "Crypto++ not found")
    endif()
  else()
    message(
      FATAL_ERROR "PkgConfig not found. This is required to find Crypto++."
    )
  endif()
endif()

set(crypto_sources)
set(crypto_headers)
set(crypto_libraries)

if(${ENABLE_CRYPTOPP})
  set(crypto_sources
      utils/symmetric-encryption.cc
      utils/hashing.cc
  )
  set(crypto_headers
      utils/symmetric-encryption.h
      utils/hashing.h
  )
  add_definitions(-DENABLE_CRYPTOPP_QKD)
  set(crypto_libraries
      ${LIB_AS_NEEDED_PRE}
      ${CRYPTOPP_LIBRARIES}
      ${LIB_AS_NEEDED_POST}
  )
endif()

set(source_files
    ${crypto_sources}
    ${qpp_sources}
    utils/qkd-data-collector.cc
    model/qkd-header.cc
    model/qkd-protocol.cc
    model/qkd-device.cc
    model/qkd-key-manager.cc
    model/qkd-key-management-layer.cc
    model/quantum-channel.cc
    model/quantum-device.cc
    model/qkd-app-header.cc
    model/qkd-app-trailer.cc
    model/qkd-key-manager-application.cc
    model/qkd-sender.cc
    model/qkd-receiver.cc
    helper/qkd-app-helper.cc
)

set(header_files
    ${crypto_headers}
    ${qpp_headers}
    utils/qkd-data-collector.h
    model/qkd-header.h
    model/qkd-protocol.h
    model/qkd-device.h
    model/qkd-key-manager.h
    model/qkd-key-management-layer.h
    model/quantum-channel.h
    model/quantum-device.h
    model/qkd-app-header.h
    model/qkd-app-trailer.h
    model/qkd-key-manager-application.h
    model/qkd-sender.h
    model/qkd-receiver.h
    helper/qkd-app-helper.h
)

set(libraries_to_link
    ${libapplications}
    ${libcore}
    ${libstats}
    ${libnetwork}
    ${libinternet}
    ${crypto_libraries}
    ${qpp_libraries}
)
if(${ENABLE_QPP}
   AND ${ENABLE_CRYPTOPP}
)

  build_lib(
    LIBNAME quantum
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK ${libraries_to_link}
    TEST_SOURCES test/symmetric-encryption-test.cc
                 ${examples_as_tests_sources}
  )

  target_link_libraries(
    quantum
    PRIVATE cryptopp
  )
endif()
