# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# URLs for each downloaded or bundled third-party dependency.
#############################################################
if (REBUNDLED)
  set(FETCH_URL ${CMAKE_CURRENT_SOURCE_DIR})
else ()
  set(FETCH_URL ${3RDPARTY_DEPENDENCIES})
endif ()

# TODO(andschwa): Move each URL to the dependency itself.
set(BZIP2_URL           ${FETCH_URL}/bzip2-${BZIP2_VERSION}.tar.gz)
set(CONCURRENTQUEUE_URL ${FETCH_URL}/concurrentqueue-${CONCURRENTQUEUE_VERSION}.tar.gz)
set(CSI_V0_URL          ${FETCH_URL}/csi-${CSI_V0_VERSION}.tar.gz)
set(CSI_V1_URL          ${FETCH_URL}/csi-${CSI_V1_VERSION}.tar.gz)
set(ELFIO_URL           ${FETCH_URL}/elfio-${ELFIO_VERSION}.tar.gz)
set(GLOG_URL            ${FETCH_URL}/glog-${GLOG_VERSION}.tar.gz)
set(GOOGLETEST_URL      ${FETCH_URL}/googletest-release-${GOOGLETEST_VERSION}.tar.gz)
set(GRPC_URL            ${FETCH_URL}/grpc-${GRPC_VERSION}.tar.gz)
set(HTTP_PARSER_URL     ${FETCH_URL}/http-parser-${HTTP_PARSER_VERSION}.tar.gz)
set(JEMALLOC_URL        ${FETCH_URL}/jemalloc-${JEMALLOC_VERSION}.tar.gz)
set(LEVELDB_URL         ${FETCH_URL}/leveldb-${LEVELDB_VERSION}.tar.gz)
set(LIBARCHIVE_URL      ${FETCH_URL}/libarchive-${LIBARCHIVE_VERSION}.tar.gz)
set(LIBEV_URL           ${FETCH_URL}/libev-${LIBEV_VERSION}.tar.gz)
set(LIBSECCOMP_URL      ${FETCH_URL}/libseccomp-${LIBSECCOMP_VERSION}.tar.gz)
set(NVML_URL            ${FETCH_URL}/nvml-${NVML_VERSION}.tar.gz)
set(PICOJSON_URL        ${FETCH_URL}/picojson-${PICOJSON_VERSION}.tar.gz)
set(PROTOBUF_URL        ${FETCH_URL}/protobuf-${PROTOBUF_VERSION}.tar.gz)
set(RAPIDJSON_URL       ${FETCH_URL}/rapidjson-${RAPIDJSON_VERSION}.tar.gz)
set(XZ_URL              ${FETCH_URL}/xz-${XZ_VERSION}-modified.tar.gz)
set(ZOOKEEPER_URL       ${FETCH_URL}/zookeeper-${ZOOKEEPER_VERSION}.tar.gz)

# NOTE: libevent doesn't come rebundled, so this URL is always the same. But,
# it's only downloaded if `ENABLE_LIBEVENT` is set.
set(LIBEVENT_URL ${3RDPARTY_DEPENDENCIES}/libevent-release-${LIBEVENT_VERSION}.tar.gz)

if (WIN32)
  # NOTE: These dependencies are only rebundled on Windows because they
  # are available as installable packages on Linux; so they live
  # exclusively in the 3rdparty repo.
  set(CURL_URL    ${3RDPARTY_DEPENDENCIES}/curl-${CURL_VERSION}.tar.gz)
  set(LIBAPR_URL  ${3RDPARTY_DEPENDENCIES}/libapr-${LIBAPR_VERSION}.tar.gz)
  set(SASL2_URL   ${3RDPARTY_DEPENDENCIES}/cyrus-sasl-${SASL2_VERSION}.tar.gz)
  set(WCLAYER_URL ${3RDPARTY_DEPENDENCIES}/wclayer.exe)
endif ()

if (NOT WIN32)
  # On platforms like FreeBSD where gmake is not the default, we need
  # to look for gmake explicitly to make sure LevelDB can build.
  # TODO(dforsyth): Remove this once LevelDB is upgraded to a version
  # that uses CMake. See MESOS-8757.
  find_program(
    MAKE_PROGRAM
    NAMES gmake make
    DOCS "External 'make' executable.")

  if (NOT MAKE_PROGRAM)
    message(FATAL_ERROR "Could not find an external 'make' executable.")
  endif ()
endif ()

# Helper utilities for dealing with third-party dependencies.
#############################################################
# ExternalProject: Create custom targets to build projects in external trees.
# https://cmake.org/cmake/help/latest/module/ExternalProject.html
include(ExternalProject)

# This adds our FindApr and FindSvn packages.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/stout/cmake)

# This adds our `EXTERNAL()` and `PATCH_CMD()` functions.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(External)
include(PatchCommand)

set(LIBRARY_LINKAGE STATIC)
set(LIBRARY_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})

# Sets a variable CMAKE_NOOP as noop operation.
#
# NOTE: This is especially important when building third-party libraries on
# Windows; the default behavior of `ExternalProject` is to try to assume that
# third-party libraries can be configured/built/installed with CMake, so in
# cases where this isn't true, we have to "trick" CMake into skipping those
# steps by giving it a noop command to run instead.
set(CMAKE_NOOP ${CMAKE_COMMAND} -E echo)

# This `CMAKE_FORWARD_ARGS` variable is sent as the `CMAKE_ARGS` argument to
# the `ExternalProject_Add` macro (along with any per-project arguments), and
# is used when the external project is configured as a CMake project.
# If either the `CONFIGURE_COMMAND` or `BUILD_COMMAND` arguments of
# `ExternalProject_Add` are used, then the `CMAKE_ARGS` argument will be
# ignored.
#
# NOTE: The CMAKE_GENERATOR_TOOLSET is impliticly set by `ExternalProject_Add`,
# and cannot be included twice.
list(APPEND CMAKE_FORWARD_ARGS
  # TODO(andschwa): Set the CMAKE_GENERATOR explicitly as an argmuent to
  # `ExternalProject_Add`.
  -G${CMAKE_GENERATOR}
  -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}
  -DBUILD_SHARED_LIBS=OFF)

# This only matters for single-configuration generators.
# E.g. Makefile, but not Visual Studio.
if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
  list(APPEND CMAKE_FORWARD_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif ()

foreach (lang C CXX)
  list(APPEND CMAKE_${lang}_FORWARD_ARGS
    ${CMAKE_FORWARD_ARGS}
    -DCMAKE_${lang}_COMPILER=${CMAKE_${lang}_COMPILER}
    -DCMAKE_${lang}_COMPILER_LAUNCHER=${CMAKE_${lang}_COMPILER_LAUNCHER}
    -DCMAKE_${lang}_FLAGS=${CMAKE_${lang}_FLAGS})

  foreach (config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    list(APPEND CMAKE_${lang}_FORWARD_ARGS
      -DCMAKE_${lang}_FLAGS_${config}=${CMAKE_${lang}_FLAGS_${config}})
  endforeach ()
endforeach ()

# For 3rdparty dependencies that use CMake to find and build against
# OpenSSL, we forward any user-supplied options for the `FindOpenSSL` module.
if (ENABLE_SSL)
  if (OPENSSL_ROOT_DIR)
    list(APPEND CMAKE_SSL_FORWARD_ARGS
      -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR})
  endif ()

  if (OPENSSL_USE_STATIC_LIBS)
    list(APPEND CMAKE_SSL_FORWARD_ARGS
      -DOPENSSL_USE_STATIC_LIBS=${OPENSSL_USE_STATIC_LIBS})
  endif ()

  if (OPENSSL_MSVC_STATIC_RT)
    list(APPEND CMAKE_SSL_FORWARD_ARGS
      -DOPENSSL_MSVC_STATIC_RT=${OPENSSL_MSVC_STATIC_RT})
  endif ()
endif ()

# This function works around a CMake issue with setting include directories of
# imported libraries built with `ExternalProject_Add`.
# https://gitlab.kitware.com/cmake/cmake/issues/15052
function(MAKE_INCLUDE_DIR TARGET)
  get_target_property(DIR ${TARGET} INTERFACE_INCLUDE_DIRECTORIES)
  file(MAKE_DIRECTORY ${DIR})
endfunction()

# This function works around a CMake issue with the Ninja generator where it
# does not understand imported libraries, and instead needs `BUILD_BYPRODUCTS`
# explicitly set.
# https://cmake.org/pipermail/cmake/2015-April/060234.html
function(GET_BYPRODUCTS TARGET)
  string(TOUPPER ${TARGET} NAME)
  if (CMAKE_GENERATOR MATCHES "Ninja")
    get_target_property(BYPRODUCTS ${TARGET} IMPORTED_LOCATION)
    set(${NAME}_BYPRODUCTS ${BYPRODUCTS} PARENT_SCOPE)
  else ()
    # Make this function a no-op when not using Ninja.
    set(${NAME}_BYPRODUCTS "" PARENT_SCOPE)
  endif()
endfunction()


# Third-party libraries. Adds CMake imported or interface targets for each
# dependency. Bundled dependencies are built with `ExternalProject_Add`,
# otherwise `find_package()` is used.
##########################################################################

# Boost: C++ Libraries.
# http://www.boost.org
#######################
hunter_add_package(Boost)
find_package(Boost CONFIG REQUIRED)

# moodycamel::ConcurrentQueue: An industrial-strength lock-free queue.
# https://github.com/cameron314/concurrentqueue
######################################################################
EXTERNAL(concurrentqueue ${CONCURRENTQUEUE_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
add_library(concurrentqueue INTERFACE)
add_dependencies(concurrentqueue ${CONCURRENTQUEUE_TARGET})
target_include_directories(concurrentqueue INTERFACE ${CONCURRENTQUEUE_ROOT})

ExternalProject_Add(
  ${CONCURRENTQUEUE_TARGET}
  PREFIX            ${CONCURRENTQUEUE_CMAKE_ROOT}
  CONFIGURE_COMMAND ${CMAKE_NOOP}
  BUILD_COMMAND     ${CMAKE_NOOP}
  INSTALL_COMMAND   ${CMAKE_NOOP}
  URL               ${CONCURRENTQUEUE_URL}
  URL_HASH          ${CONCURRENTQUEUE_HASH})


if (WIN32)
  # Cyrus SASL: Authentication protocol/mechanism interface (Based on IETF RFC 4422)
  # https://cyrusimap.org/sasl/
  ##################################################################################
  EXTERNAL(sasl2 ${SASL2_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
  add_library(sasl2 STATIC IMPORTED GLOBAL)
  add_dependencies(sasl2 ${SASL2_TARGET})

  set_target_properties(
    sasl2 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${SASL2_ROOT}-build/include
    INTERFACE_COMPILE_DEFINITIONS LIBSASL_EXPORTS=1)

  if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(
      sasl2 PROPERTIES
      IMPORTED_LOCATION_DEBUG ${SASL2_ROOT}-build/Debug/libsasl2${LIBRARY_SUFFIX}
      IMPORTED_LOCATION_RELEASE ${SASL2_ROOT}-build/Release/libsasl2${LIBRARY_SUFFIX})
  else ()
    set_target_properties(
      sasl2 PROPERTIES
      IMPORTED_LOCATION ${SASL2_ROOT}-build/libsasl2${LIBRARY_SUFFIX})
  endif ()

  # Patch SASL to include a minimal CMake build system which encompasses
  # only the parts that Mesos relies on (CRAM MD5).
  PATCH_CMD(SASL2_PATCH_CMD cyrus-sasl-${SASL2_VERSION}.patch)

  MAKE_INCLUDE_DIR(sasl2)
  GET_BYPRODUCTS(sasl2)

  ExternalProject_Add(
    ${SASL2_TARGET}
    PREFIX            ${SASL2_CMAKE_ROOT}
    BUILD_BYPRODUCTS  ${SASL2_BYPRODUCTS}
    PATCH_COMMAND     ${SASL2_PATCH_CMD}
    INSTALL_COMMAND   ${CMAKE_NOOP}
    URL               ${SASL2_URL}
    URL_HASH          ${SASL2_HASH})
else ()
  find_library(SASL2_LIB sasl2)

  if (NOT SASL2_LIB)
    message(FATAL_ERROR "Could not find sasl2 dependency.")
  endif ()

  add_library(sasl2 SHARED IMPORTED GLOBAL)
  set_property(TARGET sasl2 PROPERTY IMPORTED_LOCATION ${SASL2_LIB})
endif ()


# CSI: Container Storage Interface Specification.
# https://github.com/container-storage-interface/spec
#####################################################
EXTERNAL(csi_v0 ${CSI_V0_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
add_library(csi_v0 INTERFACE)
add_dependencies(csi_v0 ${CSI_V0_TARGET})
target_include_directories(csi_v0 INTERFACE ${CSI_V0_ROOT})

EXTERNAL(csi_v1 ${CSI_V1_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
add_library(csi_v1 INTERFACE)
add_dependencies(csi_v1 ${CSI_V1_TARGET})
target_include_directories(csi_v1 INTERFACE ${CSI_V1_ROOT})

# NOTE: To support multiple CSI versions, we move the CSI proto files to
# version-qualified paths so `protoc` can find them.
set(CSI_V0_CONFIG_CMD
  ${CMAKE_COMMAND} -E make_directory ${CSI_V0_ROOT}/csi/v0 &&
  ${CMAKE_COMMAND} -E rename ${CSI_V0_ROOT}/csi.proto ${CSI_V0_ROOT}/csi/v0/csi.proto)

set(CSI_V1_CONFIG_CMD
  ${CMAKE_COMMAND} -E make_directory ${CSI_V1_ROOT}/csi/v1 &&
  ${CMAKE_COMMAND} -E rename ${CSI_V1_ROOT}/csi.proto ${CSI_V1_ROOT}/csi/v1/csi.proto)

ExternalProject_Add(
  ${CSI_V0_TARGET}
  PREFIX            ${CSI_V0_CMAKE_ROOT}
  BUILD_BYPRODUCTS  ${CSI_V0_ROOT}/csi/v0/csi.proto
  CONFIGURE_COMMAND ${CSI_V0_CONFIG_CMD}
  BUILD_COMMAND     ${CMAKE_NOOP}
  INSTALL_COMMAND   ${CMAKE_NOOP}
  URL               ${CSI_V0_URL}
  URL_HASH          ${CSI_V0_HASH})

ExternalProject_Add(
  ${CSI_V1_TARGET}
  PREFIX            ${CSI_V1_CMAKE_ROOT}
  BUILD_BYPRODUCTS  ${CSI_V1_ROOT}/csi/v1/csi.proto
  CONFIGURE_COMMAND ${CSI_V1_CONFIG_CMD}
  BUILD_COMMAND     ${CMAKE_NOOP}
  INSTALL_COMMAND   ${CMAKE_NOOP}
  URL               ${CSI_V1_URL}
  URL_HASH          ${CSI_V1_HASH})


# ELFIO: library for reading and generating ELF files.
# http://elfio.sourceforge.net
######################################################
EXTERNAL(elfio ${ELFIO_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
add_library(elfio INTERFACE)
add_dependencies(elfio ${ELFIO_TARGET})
target_include_directories(elfio INTERFACE ${ELFIO_ROOT})

ExternalProject_Add(
  ${ELFIO_TARGET}
  PREFIX            ${ELFIO_CMAKE_ROOT}
  CONFIGURE_COMMAND ${CMAKE_NOOP}
  BUILD_COMMAND     ${CMAKE_NOOP}
  INSTALL_COMMAND   ${CMAKE_NOOP}
  URL               ${ELFIO_URL}
  URL_HASH          ${ELFIO_HASH})


# glog: Google Logging Library.
# https://github.com/google/glog
################################
hunter_add_package(glog)
find_package(glog CONFIG REQUIRED)

# PicoJSON: JSON parser / serializer.
# https://github.com/kazuho/picojson
#####################################
EXTERNAL(picojson ${PICOJSON_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
add_library(picojson INTERFACE)
add_dependencies(picojson ${PICOJSON_TARGET})
target_include_directories(picojson INTERFACE ${PICOJSON_ROOT})

# NOTE: PicoJson requires __STDC_FORMAT_MACROS to be defined before importing
# 'inttypes.h'.  Since other libraries may also import this header, it must
# be globally defined so that PicoJSON has access to the macros, regardless
# of the order of inclusion.
target_compile_definitions(
  picojson INTERFACE
  __STDC_FORMAT_MACROS)

PATCH_CMD(PICOJSON_PATCH_CMD picojson-${PICOJSON_VERSION}.patch)

ExternalProject_Add(
  ${PICOJSON_TARGET}
  PREFIX            ${PICOJSON_CMAKE_ROOT}
  PATCH_COMMAND     ${PICOJSON_PATCH_CMD}
  CONFIGURE_COMMAND ${CMAKE_NOOP}
  BUILD_COMMAND     ${CMAKE_NOOP}
  INSTALL_COMMAND   ${CMAKE_NOOP}
  URL               ${PICOJSON_URL}
  URL_HASH          ${PICOJSON_HASH})


# RapidJSON: JSON parser / serializer.
# https://github.com/Tencent/rapidjson
#####################################
hunter_add_package(RapidJSON)
find_package(RapidJSON CONFIG REQUIRED)

# NVML: NVIDIA Management Library.
# https://developer.nvidia.com/nvidia-management-library-nvml
#############################################################
EXTERNAL(nvml ${NVML_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
add_library(nvml INTERFACE)
add_dependencies(nvml ${NVML_TARGET})
target_include_directories(nvml INTERFACE ${NVML_ROOT})

ExternalProject_Add(
  ${NVML_TARGET}
  PREFIX            ${NVML_CMAKE_ROOT}
  CONFIGURE_COMMAND ${CMAKE_NOOP}
  BUILD_COMMAND     ${CMAKE_NOOP}
  INSTALL_COMMAND   ${CMAKE_NOOP}
  URL               ${NVML_URL}
  URL_HASH          ${NVML_HASH})


# HTTP Parser: HTTP request/response parser for C.
# https://github.com/nodejs/http-parser
##################################################
hunter_add_package(http-parser)
find_package(http-parser CONFIG REQUIRED)

if (ENABLE_LIBEVENT)
  # libevent: An event notification library.
  # http://libevent.org/
  ##########################################
  if (NOT UNBUNDLED_LIBEVENT)
    EXTERNAL(libevent ${LIBEVENT_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
    add_library(libevent ${LIBRARY_LINKAGE} IMPORTED)
    add_dependencies(libevent ${LIBEVENT_TARGET})

    set_target_properties(
      libevent PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_ROOT}/include;${LIBEVENT_ROOT}-build/include")

    if (WIN32)
      if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set_target_properties(
          libevent PROPERTIES
          IMPORTED_LOCATION_DEBUG ${LIBEVENT_ROOT}-build/lib/Debug/event${LIBRARY_SUFFIX}
          IMPORTED_LOCATION_RELEASE ${LIBEVENT_ROOT}-build/lib/Release/event${LIBRARY_SUFFIX})
      else ()
        set_target_properties(
          libevent PROPERTIES
          IMPORTED_LOCATION ${LIBEVENT_ROOT}-build/lib/event${LIBRARY_SUFFIX})
      endif ()
    else ()
      set_target_properties(
        libevent PROPERTIES
        IMPORTED_LOCATION ${LIBEVENT_ROOT}-build/lib/libevent${LIBRARY_SUFFIX})
    endif ()

    set(LIBEVENT_CMAKE_FORWARD_ARGS
      ${CMAKE_C_FORWARD_ARGS}
      ${CMAKE_SSL_FORWARD_ARGS}
      -DEVENT__DISABLE_OPENSSL=$<NOT:$<BOOL:${ENABLE_SSL}>>
      -DEVENT__DISABLE_BENCHMARK=ON
      -DEVENT__DISABLE_REGRESS=ON
      -DEVENT__DISABLE_SAMPLES=ON
      -DEVENT__DISABLE_TESTS=ON)

    if (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
      list(APPEND LIBEVENT_CMAKE_FORWARD_ARGS -DCMAKE_C_FLAGS=-fPIC)
    endif ()

    MAKE_INCLUDE_DIR(libevent)
    GET_BYPRODUCTS(libevent)

    ExternalProject_Add(
      ${LIBEVENT_TARGET}
      PREFIX            ${LIBEVENT_CMAKE_ROOT}
      BUILD_BYPRODUCTS  ${LIBEVENT_BYPRODUCTS}
      CMAKE_ARGS        ${LIBEVENT_CMAKE_FORWARD_ARGS}
      INSTALL_COMMAND   ${CMAKE_NOOP}
      URL               ${LIBEVENT_URL}
      URL_HASH          ${LIBEVENT_HASH})
  else ()
    find_package(LIBEVENT REQUIRED)
    add_library(libevent INTERFACE)

    foreach (lib ${LIBEVENT_LIBS})
      get_filename_component(LIBEVENT_LIB ${lib} NAME_WE)
      add_library(${LIBEVENT_LIB} SHARED IMPORTED)
      set_target_properties(${LIBEVENT_LIB} PROPERTIES
        IMPORTED_LOCATION ${lib}
        INTERFACE_INCLUDE_DIRECTORIES ${LIBEVENT_INCLUDE_DIR})
      target_link_libraries(libevent INTERFACE ${LIBEVENT_LIB})
    endforeach ()
  endif ()
elseif (NOT WIN32) # Windows defaults to `libwinio`, a native implementation.
  # libev: Full-featured high-performance event loop.
  # https://github.com/enki/libev
  ###################################################
  EXTERNAL(libev ${LIBEV_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
  add_library(libev SHARED IMPORTED)
  add_dependencies(libev ${LIBEV_TARGET})

  set_target_properties(
    libev PROPERTIES
    IMPORTED_LOCATION ${LIBEV_ROOT}-build/lib/libev${CMAKE_SHARED_LIBRARY_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${LIBEV_ROOT}-build/include)

  # Patch libev to keep it from reaping child processes.
  PATCH_CMD(LIBEV_PATCH_CMD libev-${LIBEV_VERSION}.patch)

  MAKE_INCLUDE_DIR(libev)
  GET_BYPRODUCTS(libev)

  ExternalProject_Add(
    ${LIBEV_TARGET}
    PREFIX            ${LIBEV_CMAKE_ROOT}
    BUILD_BYPRODUCTS  ${LIBEV_BYPRODUCTS}
    PATCH_COMMAND     ${LIBEV_PATCH_CMD}
    CONFIGURE_COMMAND ${LIBEV_ROOT}/configure --prefix=${LIBEV_ROOT}-build
    BUILD_COMMAND     ${MAKE_PROGRAM}
    INSTALL_COMMAND   ${MAKE_PROGRAM} install
    URL               ${LIBEV_URL}
    URL_HASH          ${LIBEV_HASH})
endif ()


# libseccomp: library, which provides a front-end for generating seccomp filters.
# https://github.com/seccomp/libseccomp
##################################################
if (ENABLE_SECCOMP_ISOLATOR)
  if (NOT UNBUNDLED_LIBSECCOMP)
    EXTERNAL(libseccomp ${LIBSECCOMP_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
    add_library(libseccomp STATIC IMPORTED GLOBAL)
    add_dependencies(libseccomp ${LIBSECCOMP_TARGET})

    set_target_properties(
      libseccomp PROPERTIES
      IMPORTED_LOCATION ${LIBSECCOMP_ROOT}-build/src/.libs/libseccomp.a
      INTERFACE_INCLUDE_DIRECTORIES "${LIBSECCOMP_ROOT}/include")

    set(
      LIBSECCOMP_CONFIGURE_COMMAND
      ${LIBSECCOMP_ROOT}/configure --disable-shared --prefix=${LIBSECCOMP_ROOT}-build)

    MAKE_INCLUDE_DIR(libseccomp)
    GET_BYPRODUCTS(libseccomp)

    ExternalProject_Add(
      ${LIBSECCOMP_TARGET}
      PREFIX            ${LIBSECCOMP_CMAKE_ROOT}
      BUILD_BYPRODUCTS  ${LIBSECCOMP_BYPRODUCTS}
      CONFIGURE_COMMAND ${LIBSECCOMP_CONFIGURE_COMMAND}
      INSTALL_COMMAND   ${CMAKE_NOOP}
      URL               ${LIBSECCOMP_URL}
      URL_HASH          ${LIBSECCOMP_HASH})
  else ()
    find_package(LIBSECCOMP REQUIRED)
    add_library(libseccomp SHARED IMPORTED GLOBAL)
    set_target_properties(libseccomp PROPERTIES
      IMPORTED_LOCATION ${LIBSECCOMP_LIBS}
      INTERFACE_INCLUDE_DIRECTORIES ${LIBSECCOMP_INCLUDE_DIR})
  endif ()
endif ()


# APR: The Apache Portable Runtime Project.
# https://apr.apache.org
###########################################
if (WIN32)
  EXTERNAL(libapr ${LIBAPR_VERSION} ${CMAKE_CURRENT_BINARY_DIR})

  # NOTE: libapr-1 is always a static library on Windows.
  add_library(apr STATIC IMPORTED)
  add_dependencies(apr ${LIBAPR_TARGET})

  set_target_properties(
    apr PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LIBAPR_ROOT}/include;${LIBAPR_ROOT}-build")

  if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(
      apr PROPERTIES
      IMPORTED_LOCATION_DEBUG ${LIBAPR_ROOT}-build/Debug/libapr-1${CMAKE_STATIC_LIBRARY_SUFFIX}
      IMPORTED_LOCATION_RELEASE ${LIBAPR_ROOT}-build/Release/libapr-1${CMAKE_STATIC_LIBRARY_SUFFIX})
  else ()
    set_target_properties(
      apr PROPERTIES
      IMPORTED_LOCATION ${LIBAPR_ROOT}-build/libapr-1${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif ()

  MAKE_INCLUDE_DIR(apr)
  GET_BYPRODUCTS(apr)

  ExternalProject_Add(
    ${LIBAPR_TARGET}
    PREFIX            ${LIBAPR_CMAKE_ROOT}
    BUILD_BYPRODUCTS  ${APR_BYPRODUCTS}
    CMAKE_ARGS        ${CMAKE_C_FORWARD_ARGS}
    INSTALL_COMMAND   ${CMAKE_NOOP}
    URL               ${LIBAPR_URL}
    URL_HASH          ${LIBAPR_HASH})
else ()
  find_package(APR REQUIRED)
  add_library(apr SHARED IMPORTED)

  set_target_properties(
    apr PROPERTIES
    IMPORTED_LOCATION ${APR_LIBS}
    INTERFACE_INCLUDE_DIRECTORIES ${APR_INCLUDE_DIR})
endif ()


# Subversion: Enterprise-class centralized version control for the masses.
# https://subversion.apache.org
##########################################################################
if (NOT WIN32)
  # NOTE: This does not use `FindSubversion` because it finds the `svn`
  # executable, but we're looking for the library.
  find_package(SVN REQUIRED)
  add_library(svn INTERFACE)
  target_include_directories(svn INTERFACE ${SVN_INCLUDE_DIR})

  # `FindSvn` returns finds three separate libraries.
  foreach (lib ${SVN_LIBS})
    get_filename_component(SVN_LIB ${lib} NAME_WE)
    add_library(${SVN_LIB} SHARED IMPORTED)
    set_target_properties(${SVN_LIB} PROPERTIES IMPORTED_LOCATION ${lib})
    target_link_libraries(svn INTERFACE ${SVN_LIB})
  endforeach ()
endif ()


# cURL: Command line tool and library for transferring data with URLs.
# https://curl.haxx.se
######################################################################
if (WIN32)
  EXTERNAL(curl ${CURL_VERSION} ${CMAKE_CURRENT_BINARY_DIR})

  # This is named `libcurl` to not conflict with the executable.
  add_library(libcurl ${LIBRARY_LINKAGE} IMPORTED GLOBAL)
  add_dependencies(libcurl ${CURL_TARGET})

  # Contrary to Linux, the CMake build deploys `curl.exe` on Windows.
  #
  # NOTE: The dependency on `CURL_TARGET` is introduced by the generator
  # expression in `add_custom_command` below. Introducing it here via
  # `add_dependencies` creates a cycle.
  add_executable(curl IMPORTED GLOBAL)

  set(CURL_CMAKE_FORWARD_ARGS
    ${CMAKE_C_FORWARD_ARGS}
    -DCMAKE_USE_WINSSL=ON
    -DCURL_DISABLE_LDAP=ON
    -DBUILD_TESTING=OFF)

  # This is both a CMake argument and a pre-processor definition.
  list(APPEND CURL_CMAKE_FORWARD_ARGS -DCURL_STATICLIB=ON)

  set_target_properties(
    libcurl PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS CURL_STATICLIB)

  set_target_properties(
    libcurl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${CURL_ROOT}/include
    INTERFACE_LINK_LIBRARIES crypt32)

  if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(
      libcurl PROPERTIES
      IMPORTED_LOCATION_DEBUG ${CURL_ROOT}-build/lib/Debug/libcurl-d${LIBRARY_SUFFIX}
      IMPORTED_LOCATION_RELEASE ${CURL_ROOT}-build/lib/Release/libcurl${LIBRARY_SUFFIX}
      IMPORTED_IMPLIB_DEBUG ${CURL_ROOT}-build/lib/Debug/libcurl_imp${CMAKE_IMPORT_LIBRARY_SUFFIX}
      IMPORTED_IMPLIB_RELEASE ${CURL_ROOT}-build/lib/Release/libcurl_imp${CMAKE_IMPORT_LIBRARY_SUFFIX})

    set_target_properties(
      curl PROPERTIES
      IMPORTED_LOCATION_DEBUG ${CURL_ROOT}-build/src/Debug/curl.exe
      IMPORTED_LOCATION_RELEASE ${CURL_ROOT}-build/src/Release/curl.exe)
  else ()
    # This is for single-configuration generators such as Ninja.
    if (CMAKE_BUILD_TYPE MATCHES Debug)
      set(CURL_SUFFIX "-d")
    endif ()

    set_target_properties(
      libcurl PROPERTIES
      IMPORTED_LOCATION ${CURL_ROOT}-build/lib/libcurl${CURL_SUFFIX}${LIBRARY_SUFFIX}
      IMPORTED_IMPLIB ${CURL_ROOT}-build/lib/libcurl_imp${CMAKE_IMPORT_LIBRARY_SUFFIX})

    set_target_properties(
      curl PROPERTIES
      IMPORTED_LOCATION ${CURL_ROOT}-build/src/curl.exe)
  endif ()

  MAKE_INCLUDE_DIR(libcurl)
  GET_BYPRODUCTS(libcurl)
  GET_BYPRODUCTS(curl)

  ExternalProject_Add(
    ${CURL_TARGET}
    PREFIX            ${CURL_CMAKE_ROOT}
    BUILD_BYPRODUCTS  ${LIBCURL_BYPRODUCTS};${CURL_BYPRODUCTS}
    PATCH_COMMAND     ${CMAKE_NOOP}
    CMAKE_ARGS        ${CURL_CMAKE_FORWARD_ARGS}
    INSTALL_COMMAND   ${CMAKE_NOOP}
    URL               ${CURL_URL}
    URL_HASH          ${CURL_HASH})

  # This copies the file `curl.exe` from the `3rdparty` build folder to
  # `build/src`, next to the other produced executables. This is necessary for
  # code that shells out to cURL on Windows.
  #
  # TODO(andschwa): Change this to an `INSTALL` step.
  ExternalProject_Add_Step(${CURL_TARGET} copy
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:curl> ${CMAKE_BINARY_DIR}/src/curl.exe
    DEPENDEES build)
else ()
  find_package(CURL REQUIRED)
  add_library(libcurl SHARED IMPORTED)

  set_target_properties(
    libcurl PROPERTIES
    IMPORTED_LOCATION ${CURL_LIBRARIES}
    INTERFACE_INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIRS})
endif ()


# wclayer: Command line tool for manipulating Windows Container
# storage layers.
# https://github.com/Microsoft/hcsshim/blob/master/cmd/wclayer/wclayer.go
#########################################################################
if (WIN32)
  EXTERNAL(wclayer WIP ${CMAKE_CURRENT_BINARY_DIR})

  add_executable(wclayer IMPORTED GLOBAL)
  add_dependencies(wclayer ${WCLAYER_TARGET})

  # TODO(liazha): Change this to wherever you need it placed.
  set(WCLAYER_PATH ${CMAKE_BINARY_DIR}/src/wclayer.exe)
  set_target_properties(
    wclayer PROPERTIES
    IMPORTED_LOCATION ${WCLAYER_PATH})

  set(WCLAYER_INSTALL_CMD ${CMAKE_COMMAND} -E copy ${WCLAYER_CMAKE_ROOT}/src/wclayer.exe ${WCLAYER_PATH})

  GET_BYPRODUCTS(wclayer)

  ExternalProject_Add(
    ${WCLAYER_TARGET}
    PREFIX              ${WCLAYER_CMAKE_ROOT}
    BUILD_BYPRODUCTS    ${WCLAYER_BYPRODUCTS}
    CONFIGURE_COMMAND   ${CMAKE_NOOP}
    BUILD_COMMAND       ${CMAKE_NOOP}
    INSTALL_COMMAND     ${WCLAYER_INSTALL_CMD}
    DOWNLOAD_NO_EXTRACT FALSE
    URL                 ${WCLAYER_URL})
endif ()

# bzip2: A high-quality data compressor.
# http://www.bzip.org
########################################
if (WIN32)
  EXTERNAL(bzip2 ${BZIP2_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
  add_library(bzip2 STATIC IMPORTED)
  add_dependencies(bzip2 ${BZIP2_TARGET})

  set_target_properties(
    bzip2 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BZIP2_ROOT};${BZIP2_ROOT}-build")

  if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(
      bzip2 PROPERTIES
      IMPORTED_LOCATION_DEBUG ${BZIP2_ROOT}-build/Debug/bzip2${BZIP2_SHARED}${LIBRARY_SUFFIX}
      IMPORTED_LOCATION_RELEASE ${BZIP2_ROOT}-build/Release/bzip2${BZIP2_SHARED}${LIBRARY_SUFFIX}
      IMPORTED_IMPLIB_DEBUG ${BZIP2_ROOT}-build/Debug/bzip2${CMAKE_IMPORT_LIBRARY_SUFFIX}
      IMPORTED_IMPLIB_RELEASE ${BZIP2_ROOT}-build/Release/bzip2${CMAKE_IMPORT_LIBRARY_SUFFIX})
  else ()
    set_target_properties(
      bzip2 PROPERTIES
      IMPORTED_LOCATION ${BZIP2_ROOT}-build/bzip2${BZIP2_SHARED}${LIBRARY_SUFFIX})
  endif ()

  PATCH_CMD(BZIP2_PATCH_CMD bzip2-${BZIP2_VERSION}.patch)

  MAKE_INCLUDE_DIR(bzip2)
  GET_BYPRODUCTS(bzip2)

  set(BZIP2_CMAKE_ARGS
    ${CMAKE_FORWARD_ARGS}
    ${CMAKE_C_FORWARD_ARGS}
    -DBZIP2_INSTALL_DIR=${BZIP2_ROOT}-lib)

  ExternalProject_Add(
    ${BZIP2_TARGET}
    PREFIX            ${BZIP2_CMAKE_ROOT}
    BUILD_BYPRODUCTS  ${BZIP2_BYPRODUCTS}
    PATCH_COMMAND     ${BZIP2_PATCH_CMD}
    CMAKE_ARGS        ${BZIP2_CMAKE_ARGS}
    INSTALL_DIR       ${BZIP2_ROOT}-lib
    URL               ${BZIP2_URL}
    URL_HASH          ${BZIP2_HASH})
endif ()

# xz: A Massively Spiffy Yet Delicately Unobtrusive Compression Library.
# https://tukaani.org/xz/
########################################################################
hunter_add_package(ZLIB)
find_package(ZLIB CONFIG REQUIRED)

# libarchive: Multi-format archive and compression library.
# https://github.com/libarchive/libarchive
###########################################################
if (NOT UNBUNDLED_LIBARCHIVE)
  EXTERNAL(libarchive ${LIBARCHIVE_VERSION} ${CMAKE_CURRENT_BINARY_DIR})

  add_library(libarchive STATIC IMPORTED GLOBAL)
  add_dependencies(libarchive ${LIBARCHIVE_TARGET})

  set(LIBARCHIVE_CMAKE_ARGS
    ${CMAKE_FORWARD_ARGS}
    ${CMAKE_C_FORWARD_ARGS}
    -DENABLE_ACL=OFF
    -DENABLE_CNG=OFF
    -DENABLE_CPIO=OFF
    -DENABLE_EXPAT=OFF
    -DENABLE_ICONV=OFF
    -DENABLE_LibGCC=OFF
    -DENABLE_LIBXML2=OFF
    -DENABLE_LZO=OFF
    -DENABLE_NETTLE=OFF
    -DENABLE_OPENSSL=OFF
    -DENABLE_PCREPOSIX=OFF
    -DENABLE_TEST=OFF
    -DCMAKE_PREFIX_PATH=${ZLIB_ROOT}-lib@@${BZIP2_ROOT}-lib@@${XZ_ROOT}-lib
    -DCMAKE_INSTALL_PREFIX=${LIBARCHIVE_ROOT}-build)

  PATCH_CMD(LIBARCHIVE_PATCH_CMD libarchive-${LIBARCHIVE_VERSION}.patch)

  # NOTE: On Windows, libarchive is linked against several compression
  # libraries included in the build chain, such as bzip2, xz, and zlib.
  # On other platforms, libarchive links to zlib only, as bzip2 and xz
  # are not in the build chain.
  if (WIN32)
    set_target_properties(
      libarchive PROPERTIES
      INTERFACE_COMPILE_DEFINITIONS LIBARCHIVE_STATIC
      # NOTE: The install step avoids the need for separate DEBUG and
      # RELEASE paths.
      IMPORTED_LOCATION ${LIBARCHIVE_ROOT}-build/lib/archive_static${LIBRARY_SUFFIX}
      INTERFACE_INCLUDE_DIRECTORIES ${LIBARCHIVE_ROOT}-build/include
      INTERFACE_LINK_LIBRARIES "bzip2;xz")
    set(LIBARCHIVE_DEPENDS bzip2 xz)

    # Make libarchive link against the same zlib the rest of the build
    # links to. This is necessary because the zlib project
    # unconditionally builds both shared and static libraries, and we
    # need to be consistent about which is linked.
    list(APPEND LIBARCHIVE_CMAKE_ARGS
      -DZLIB_LIBRARY=$<TARGET_FILE:zlib>)
  else ()
    list(APPEND LIBARCHIVE_CMAKE_ARGS
      -DENABLE_BZip2=OFF
      -DENABLE_LZMA=OFF)

    set_target_properties(
      libarchive PROPERTIES
      IMPORTED_LOCATION ${LIBARCHIVE_ROOT}-build/lib/libarchive${CMAKE_STATIC_LIBRARY_SUFFIX}
      INTERFACE_INCLUDE_DIRECTORIES ${LIBARCHIVE_ROOT}-build/include
      INTERFACE_LINK_LIBRARIES "ZLIB::zlib")
  endif ()

  MAKE_INCLUDE_DIR(libarchive)
  GET_BYPRODUCTS(libarchive)

  ExternalProject_Add(
    ${LIBARCHIVE_TARGET}
    PREFIX            ${LIBARCHIVE_CMAKE_ROOT}
    BUILD_BYPRODUCTS  ${LIBARCHIVE_BYPRODUCTS}
    PATCH_COMMAND     ${LIBARCHIVE_PATCH_CMD}
    LIST_SEPARATOR    @@
    CMAKE_ARGS        ${LIBARCHIVE_CMAKE_ARGS}
    URL               ${LIBARCHIVE_URL}
    URL_HASH          ${LIBARCHIVE_HASH})
else ()
  find_package(LIBARCHIVE REQUIRED)
  add_library(libarchive SHARED IMPORTED GLOBAL)
  set_target_properties(libarchive PROPERTIES
    IMPORTED_LOCATION ${LIBARCHIVE_LIBS}
    INTERFACE_INCLUDE_DIRECTORIES ${LIBARCHIVE_INCLUDE_DIR})
endif ()

# Google Test: Google's C++ test framework (GoogleTest and GoogleMock).
# https://github.com/google/googletest
#######################################################################
hunter_add_package(GTest)
find_package(GMock CONFIG REQUIRED)

# Protocol Buffers: Google's data interchange format.
# https://developers.google.com/protocol-buffers/
#####################################################
hunter_add_package(Protobuf)
find_package(Protobuf CONFIG REQUIRED)

# gRPC: Google's high performance, open-source universal RPC framework.
# https://grpc.io/
#######################################################################
hunter_add_package(gRPC)
find_package(gRPC CONFIG REQUIRED)

# Jemalloc: General-purpose malloc implementation.
# http://jemalloc.net
##################################################
if (ENABLE_JEMALLOC_ALLOCATOR)
  EXTERNAL(jemalloc ${JEMALLOC_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
  add_library(jemalloc STATIC IMPORTED GLOBAL)
  add_dependencies(jemalloc ${JEMALLOC_TARGET})

  set_target_properties(
    jemalloc PROPERTIES
    IMPORTED_LOCATION ${JEMALLOC_ROOT}-build/lib/libjemalloc_pic${CMAKE_STATIC_LIBRARY_SUFFIX})

  set(
    JEMALLOC_CONFIGURE_COMMAND
    ${JEMALLOC_ROOT}/configure --enable-stats --enable-prof --with-malloc-conf=prof:true,prof_active:false)

  GET_BYPRODUCTS(jemalloc)

  ExternalProject_Add(
    ${JEMALLOC_TARGET}
    PREFIX            ${JEMALLOC_CMAKE_ROOT}
    BUILD_BYPRODUCTS  ${JEMALLOC_BYPRODUCTS}
    CONFIGURE_COMMAND ${JEMALLOC_CONFIGURE_COMMAND}
    INSTALL_COMMAND   ${CMAKE_NOOP}
    URL               ${JEMALLOC_URL}
    URL_HASH          ${JEMALLOC_HASH})
endif ()


# Apache ZooKeeper: C Client Library to ZooKeeper.
# https://zookeeper.apache.org/
##################################################
# TODO(andschwa): Move zookeeper so these don't have to be GLOBAL.
EXTERNAL(zookeeper ${ZOOKEEPER_VERSION} ${CMAKE_CURRENT_BINARY_DIR})
add_library(zookeeper STATIC IMPORTED GLOBAL)
add_library(zk-hashtable STATIC IMPORTED GLOBAL)
add_dependencies(zookeeper ${ZOOKEEPER_TARGET})

set(ZOOKEEPER_COMPILE_DEFINITIONS ZOOKEEPER_VERSION="${ZOOKEEPER_VERSION}" USE_STATIC_LIB)
set_target_properties(
  zookeeper PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${ZOOKEEPER_ROOT}/src/c/include;${ZOOKEEPER_ROOT}/src/c/generated"
  INTERFACE_LINK_LIBRARIES zk-hashtable
  INTERFACE_COMPILE_DEFINITIONS "${ZOOKEEPER_COMPILE_DEFINITIONS}")

set(ZOOKEEPER_CMAKE_FORWARD_ARGS ${CMAKE_CXX_FORWARD_ARGS} -DWANT_CPPUNIT=OFF)

PATCH_CMD(ZOOKEEPER_PATCH_CMD zookeeper-${ZOOKEEPER_VERSION}.patch)

if (WIN32)
  if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(
      zookeeper PROPERTIES
      IMPORTED_LOCATION_DEBUG ${ZOOKEEPER_ROOT}-build/Debug/zookeeper${CMAKE_STATIC_LIBRARY_SUFFIX}
      IMPORTED_LOCATION_RELEASE ${ZOOKEEPER_ROOT}-build/Release/zookeeper${CMAKE_STATIC_LIBRARY_SUFFIX})

    set_target_properties(
      zk-hashtable PROPERTIES
      IMPORTED_LOCATION_DEBUG ${ZOOKEEPER_ROOT}-build/Debug/hashtable${CMAKE_STATIC_LIBRARY_SUFFIX}
      IMPORTED_LOCATION_RELEASE ${ZOOKEEPER_ROOT}-build/Release/hashtable${CMAKE_STATIC_LIBRARY_SUFFIX})
  else ()
    set_target_properties(
      zookeeper PROPERTIES
      IMPORTED_LOCATION ${ZOOKEEPER_ROOT}-build/zookeeper${CMAKE_STATIC_LIBRARY_SUFFIX})

    set_target_properties(
      zk-hashtable PROPERTIES
      IMPORTED_LOCATION ${ZOOKEEPER_ROOT}-build/hashtable${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif ()
else ()
  set_target_properties(
    zookeeper PROPERTIES
    IMPORTED_LOCATION ${ZOOKEEPER_ROOT}-build/libzookeeper${CMAKE_STATIC_LIBRARY_SUFFIX})

  set_target_properties(
    zk-hashtable PROPERTIES
    IMPORTED_LOCATION ${ZOOKEEPER_ROOT}-build/libhashtable${CMAKE_STATIC_LIBRARY_SUFFIX})
endif ()

MAKE_INCLUDE_DIR(zookeeper)
GET_BYPRODUCTS(zookeeper)
GET_BYPRODUCTS(zk-hashtable)

ExternalProject_Add(
  ${ZOOKEEPER_TARGET}
  PREFIX            ${ZOOKEEPER_CMAKE_ROOT}
  BUILD_BYPRODUCTS  ${ZOOKEEPER_BYPRODUCTS};${ZK-HASHTABLE_BYPRODUCTS}
  PATCH_COMMAND     ${ZOOKEEPER_PATCH_CMD}
  SOURCE_SUBDIR     src/c
  CMAKE_ARGS        ${ZOOKEEPER_CMAKE_FORWARD_ARGS}
  INSTALL_COMMAND   ${CMAKE_NOOP}
  URL               ${ZOOKEEPER_URL}
  URL_HASH          ${ZOOKEEPER_HASH})


# LevelDB: A fast key-value storage library.
# https://github.com/google/leveldb
############################################
if (NOT WIN32)
  hunter_add_package(leveldb)
  find_package(leveldb CONFIG REQUIRED)
endif ()


# In-tree dependencies.
#######################
add_subdirectory(stout)
add_subdirectory(libprocess)
