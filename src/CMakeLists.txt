# Copyright (C) 2020 by Franco Venturi
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

### shared library
add_library(sddc SHARED
    libsddc.c
    ezusb.c
    logging.c
    usb_device.c
    streaming.c
)
set_target_properties(sddc PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(sddc PROPERTIES SOVERSION 0)

target_include_directories(sddc PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>  # <prefix>/include
)
target_link_libraries(sddc PkgConfig::LIBUSB)


# applications
add_executable(sddc_test sddc_test.c)
target_link_libraries(sddc_test sddc)
add_executable(sddc_stream_test sddc_stream_test.c wavewrite.c)
target_link_libraries(sddc_stream_test sddc)
add_executable(sddc_vhf_stream_test sddc_vhf_stream_test.c wavewrite.c)
target_link_libraries(sddc_vhf_stream_test sddc)


# install
install(TARGETS sddc
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(TARGETS sddc_test sddc_stream_test sddc_vhf_stream_test
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)
