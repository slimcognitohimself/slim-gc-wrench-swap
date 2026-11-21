# Wrench Swap: a native OpenRAC mod for Going Commando.
#
#   python3 tools/fetch_sdk.py                     # the SDK sdk.lock pins, into .sdk/
#   cmake -S . -B build -G Ninja
#   cmake --build build
#   python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/wrench_swap -o dist
#
# Push a tag v<version> and the workflow (.github/workflows/build.yml) publishes the release the
# launcher installs from.
cmake_minimum_required(VERSION 3.20)
project(wrench_swap LANGUAGES C CXX)

# The SDK: the release sdk.lock pins (tools/fetch_sdk.py puts it in .sdk/); OPENRAC_SDK=<dir> (an
# unpacked SDK, or sdk/ in a checkout of OpenRAC) or OpenRACMod_DIR=<dir>/cmake for another.
if(NOT OpenRACMod_DIR)
  if(DEFINED OPENRAC_SDK)
    set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
    string(JSON sdk_version GET "${lock}" version)
    set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
    if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
      message(FATAL_ERROR "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
                          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
    endif()
  endif()
endif()
find_package(OpenRACMod 1.20 REQUIRED)

openrac_add_mod(wrench_swap MANIFEST openrac-mod.json SOURCES src/wrench_swap.c)
