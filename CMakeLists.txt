project(jpncap C)
cmake_minimum_required(VERSION 2.6)
find_package(PkgConfig REQUIRED)

#set(CMAKE_VERBOSE_MAKEFILE on)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_C_FLAGS_RELEASE "-O3")
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

set(RESOURCES_PATH "${CMAKE_INSTALL_PREFIX}/share/jpncap") #No ending slash
configure_file("${PROJECT_SOURCE_DIR}/src/configuration.h.in" "${PROJECT_BINARY_DIR}/configuration.h")
include_directories("${PROJECT_BINARY_DIR}")

set(EXECUTEABLE_PATH "${CMAKE_INSTALL_PREFIX}/bin/jpncap")
configure_file("${PROJECT_SOURCE_DIR}/data/jpncap.desktop.in" "${PROJECT_BINARY_DIR}/jpncap.desktop")

pkg_check_modules(DEPS REQUIRED gtk+-3.0>=3.20 lept tesseract sqlite3)
include_directories(${DEPS_INCLUDE_DIRS})

add_executable(jpncap src/main.c src/vector.c src/capture.c src/recognize.c src/japanese_util.c src/dictionary.c src/main_window.c)
target_link_libraries(jpncap ${DEPS_LIBRARIES})

install(TARGETS jpncap DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
install(FILES "data/deinflect.txt" "data/substitutions.txt" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/jpncap")
install(FILES "data/jpncap.svg" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
install(FILES "${PROJECT_BINARY_DIR}/jpncap.desktop" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
