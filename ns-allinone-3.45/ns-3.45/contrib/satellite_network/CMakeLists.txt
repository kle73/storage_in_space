
set(source_files
  model/constellation-config.cc
  model/ring-switch-scheduler.cc
  model/point-to-point-laser-channel.cc
  model/satellite-forwarding-app.cc
  model/point-to-point-laser-helper.cc
  model/point-to-point-laser-net-device.cc
  model/point-to-point-laser-remote-channel.cc
  model/routing/routing-ring-switch-walker-delta.cc
  model/routing/routing-ring-switch-walker-star.cc
  model/content/content-fill-double.cc
)

set(header_files
  model/constellation-config.h
  model/strategy-interfaces.h
  model/ring-switch-scheduler.h
  model/point-to-point-laser-channel.h
  model/satellite-forwarding-app.h
  model/point-to-point-laser-helper.h
  model/point-to-point-laser-net-device.h
  model/point-to-point-laser-remote-channel.h
  model/routing/routing-ring-switch-walker-delta.h
  model/routing/routing-ring-switch-walker-star.h
  model/content/content-fill-double.h
)

build_lib(
  LIBNAME satellite_network
  SOURCE_FILES ${source_files}
  HEADER_FILES ${header_files}
  LIBRARIES_TO_LINK
    ${libenergy}
    ${libpoint-to-point}
    ${libbuildings}
    ${libnetwork}
    ${libmpi}
    ${libcore}
    ${libmobility}
    ${libinternet}
    ${libapplications}
    ${libsatellite}
    ${libinternet-apps}
)
