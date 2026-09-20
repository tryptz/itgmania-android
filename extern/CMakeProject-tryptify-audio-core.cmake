# tryptify-audio-core — tac_usb, the libusb UAC1/UAC2 isochronous output
# driver behind RageSoundDriver_LibusbUAC. Off by default: it needs a USB
# Audio Class DAC, and on a desktop host tac_usb can still only be handed a
# file descriptor (the Android UsbDeviceConnection model), so nothing opens
# a device yet. Turn it on with -DWITH_LIBUSB_UAC=ON.
option(WITH_LIBUSB_UAC
       "Build the libusb USB Audio Class sound driver (tryptify-audio-core)"
       OFF)

if(NOT WITH_LIBUSB_UAC)
  return()
endif()

# ── Android harness link ─────────────────────────────────────────────────
# This has to come before the already-set-up guard below, and it has to run
# on every pass. Two sites reach this file and they differ in a way that
# matters:
#
#   ITGMANIA_SOURCE_ROOT/extern is pulled in with add_subdirectory(), which
#   opens a CHILD scope — a variable set there never reaches the harness.
#   CMakeData-arch.cmake is pulled in with include(), which does not open a
#   scope, so an append there lands where the harness will read it.
#
# The harness builds ITGMANIA_ANDROID_AVAILABLE_DEPS after the
# add_subdirectory and before the include, then expands it into both
# target_link_libraries() calls — the itgmania_core_compile_probe object
# library that compiles RageSoundDriver_LibusbUAC.cpp, and the
# itgmania_android shared object it all lands in. So the append is a no-op
# on the add_subdirectory pass (the variable does not exist yet) and does
# the real work on the include pass.
#
# Undefined on a desktop build, where src/CMakeLists.txt links tac_usb
# itself — hence the guard rather than an unconditional append.
if(DEFINED ITGMANIA_ANDROID_AVAILABLE_DEPS)
  list(APPEND ITGMANIA_ANDROID_AVAILABLE_DEPS tac_usb)
  message(STATUS "tryptify-audio-core: added tac_usb to the Android link")
endif()

# Everything past here sets the project up, which must happen exactly once.
if(TARGET tac_usb)
  return()
endif()

set(TAC_DIR "${SM_EXTERN_DIR}/tryptify-audio-core")
if(NOT EXISTS "${TAC_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
          "WITH_LIBUSB_UAC is ON but extern/tryptify-audio-core is empty. "
          "Run: git submodule update --init --recursive")
endif()

# Do not let tac_usb build a second libusb. We already vendor one via
# CMakeProject-libusb.cmake, and two static libusb archives in one binary
# means duplicate symbols at link — or two libusb contexts at runtime, each
# with its own event thread and device list.
#
# That project is Linux-only (it returns early otherwise), so LIBUSB_LIBRARY
# is only set there. Everywhere else TAC_LIBUSB_TARGET stays empty and
# tac_usb falls back to its own vendored copy, which is what an Android
# build wants anyway.
if(LIBUSB_LIBRARY)
  add_library(itg_libusb INTERFACE)
  target_include_directories(itg_libusb INTERFACE "${LIBUSB_INCLUDE_DIR}")
  target_link_libraries(itg_libusb INTERFACE "${LIBUSB_LIBRARY}")
  set(TAC_LIBUSB_TARGET itg_libusb CACHE STRING "" FORCE)
  message(STATUS "tryptify-audio-core: reusing ITGmania's libusb")
else()
  message(STATUS "tryptify-audio-core: using its own vendored libusb")
endif()

# Its host tests are not ours to run, and the DSP engine is Tryptify's
# mixer — this build wants the USB driver only.
set(TAC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TAC_BUILD_USB ON CACHE BOOL "" FORCE)

add_subdirectory("${TAC_DIR}" "${CMAKE_BINARY_DIR}/tryptify-audio-core")

# The ExternalProject above produces libusb-1.0.a at build time, so order the
# static lib after it even though only headers are needed to compile.
if(LIBUSB_LIBRARY AND TARGET libusb)
  add_dependencies(tac_usb libusb)
endif()

