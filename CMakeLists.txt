cmake_minimum_required(VERSION 3.6)

project(gdk-pixbuf-loader-apng)

include(GNUInstallDirs)
find_package(PkgConfig)
pkg_check_modules(GDK_PIXBUF REQUIRED gdk-pixbuf-2.0)

add_library(pixbufloader-apng SHARED
  src/io-apng.c
  src/io-apng.h
  src/io-apng-animation.c
  src/io-apng-animation.h
)
target_include_directories(pixbufloader-apng PUBLIC ${GDK_PIXBUF_INCLUDE_DIRS})
target_link_libraries(pixbufloader-apng PUBLIC ${GDK_PIXBUF_LIBRARIES})
link_directories(${GDK_PIXBUF_LIBRARY_DIRS})

if ($ENV{GDK_PIXBUF_MODULEDIR})
  set(GDK_PIXBUF_MODULEDIR $ENV{GDK_PIXBUF_MODULEDIR})
else()
  pkg_get_variable(GDK_PIXBUF_BINARY_VERSION gdk-pixbuf-2.0 gdk_pixbuf_binary_version)
  set(GDK_PIXBUF_MODULEDIR "${CMAKE_INSTALL_LIBDIR}/gdk-pixbuf-2.0/${GDK_PIXBUF_BINARY_VERSION}/loaders")
endif()

install(TARGETS pixbufloader-apng LIBRARY DESTINATION "${GDK_PIXBUF_MODULEDIR}")
