project(virtual-cam-filter VERSION 0.0.2)
set(PROJECT_FULL_NAME "Virtual Camera Filter")

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

add_library(virtual-cam-filter MODULE
	${virtual-cam-filter_HEADERS}
	${virtual-cam-filter_SOURCES})
target_link_libraries(virtual-cam-filter
	libobs
	${virtual-cam-filter_PLATFORM_DEPS})

set_target_properties(virtual-cam-filter PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(virtual-cam-filter data)
