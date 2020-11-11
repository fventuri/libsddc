# Copyright (C) 2020 by Franco Venturi
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

### project
cmake_minimum_required(VERSION 3.13)

project(libsddc VERSION 0.0.1)


### build options
# default build type: Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE} " - Version: " ${VERSION} " / " ${LIBVER})

include(GNUInstallDirs)

add_compile_options(-Wall -Wextra -pedantic -Werror)


### dependencies
find_package(PkgConfig)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0 IMPORTED_TARGET)


### subdirectories
add_subdirectory(include)
add_subdirectory(src)
