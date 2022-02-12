cmake_minimum_required(VERSION 3.12..4.0)

project(virtual-cam-filter VERSION 0.0.5)
set(PROJECT_FULL_NAME "Virtual Camera Filter")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

if(MSVC)
	set(virtual-cam-filter_PLATFORM_DEPS
		w32-pthreads)
endif()

set(virtual-cam-filter_HEADERS
	virtual-cam-filter.h)
set(virtual-cam-filter_SOURCES
	virtual-cam-filter.c)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in virtual-cam-filter.rc)
	list(APPEND virtual-cam-filter_SOURCES
		virtual-cam-filter.rc)
endif()

if(BUILD_OUT_OF_TREE)
    find_package(LibObs REQUIRED)
endif()

add_library(virtual-cam-filter MODULE
	${virtual-cam-filter_HEADERS}
	${virtual-cam-filter_SOURCES})
target_link_libraries(virtual-cam-filter
	obs-frontend-api
	libobs
	${virtual-cam-filter_PLATFORM_DEPS})

if(BUILD_OUT_OF_TREE)
    if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
        set(DATA_OUT_DIR "/share/obs/obs-plugins/virtual-cam-filter")
    endif()
    set_target_properties(virtual-cam-filter PROPERTIES PREFIX "")
    install(TARGETS virtual-cam-filter
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
else()
    target_include_directories(virtual-cam-filter PRIVATE
        "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
    set_target_properties(virtual-cam-filter PROPERTIES FOLDER "plugins/exeldro")
    install_obs_plugin_with_data(virtual-cam-filter data)
endif()
