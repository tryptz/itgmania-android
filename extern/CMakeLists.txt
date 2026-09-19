include(CMakeProject-hidapi.cmake)
include(CMakeProject-lua.cmake)
include(CMakeProject-glew.cmake)
include(CMakeProject-json.cmake)
include(CMakeProject-mad.cmake)
include(CMakeProject-zlib.cmake)
include(CMakeProject-ogg.cmake)
include(CMakeProject-vorbis.cmake)
include(CMakeProject-vorbisfile.cmake)
include(CMakeProject-pcre.cmake)
include(CMakeProject-tomcrypt.cmake)
include(CMakeProject-tommath.cmake)
include(CMakeProject-stb_image.cmake)
include(CMakeProject-stb_image_write.cmake)
include(CMakeProject-ixwebsocket.cmake)
include(CMakeProject-miniz.cmake)

# External projects
include(CMakeProject-libusb.cmake)

# Must follow libusb: it reuses that project's LIBUSB_LIBRARY so only one
# libusb ends up in the binary.
include(CMakeProject-tryptify-audio-core.cmake)
