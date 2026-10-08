build_lib_example(
  NAME qkd-example
  SOURCE_FILES qkd-example.cc
  LIBRARIES_TO_LINK
    ${libquantum}
    ${libnetwork}
    ${libpoint-to-point}
    ${libinternet}
)

build_lib_example(
  NAME qkd-eavesdropper-example
  SOURCE_FILES qkd-eavesdropper-example.cc
  LIBRARIES_TO_LINK
    ${libquantum}
    ${libnetwork}
    ${libpoint-to-point}
    ${libinternet}
)

build_lib_example(
  NAME qkd-application-first-example
  SOURCE_FILES qkd-application-first-example.cc
  LIBRARIES_TO_LINK
    ${libquantum}
    ${libnetwork}
    ${libpoint-to-point}
    ${libinternet}
)
