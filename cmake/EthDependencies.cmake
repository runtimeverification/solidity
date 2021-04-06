# all dependencies that are not directly included in the cpp-ethereum distribution are defined here
# for this to work, download the dependency via the cmake script in extdep or install them manually!

if (DEFINED MSVC)
	# by defining CMAKE_PREFIX_PATH variable, cmake will look for dependencies first in our own repository before looking in system paths like /usr/local/ ...
	# this must be set to point to the same directory as $ETH_DEPENDENCY_INSTALL_DIR in /extdep directory

	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.0.0)
		set (ETH_DEPENDENCY_INSTALL_DIR "${CMAKE_CURRENT_LIST_DIR}/../extdep/install/windows/x64")
	else()
		get_filename_component(DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/../deps/install" ABSOLUTE)
		set(ETH_DEPENDENCY_INSTALL_DIR
			"${DEPS_DIR}/x64"					# Old location for deps.
			"${DEPS_DIR}/win64"					# New location for deps.
			"${DEPS_DIR}/win64/Release/share"	# LLVM shared cmake files.
		)
	endif()
	set (CMAKE_PREFIX_PATH ${ETH_DEPENDENCY_INSTALL_DIR} ${CMAKE_PREFIX_PATH})
endif()

if (DEFINED APPLE)
	# CMAKE_PREFIX_PATH needs to be set to /usr/local/opt/llvm in order to pick up the homebrew installation of llvm instead of the system llvm, which
        # does not support cmake.
	set (CMAKE_PREFIX_PATH "/usr/local/opt/llvm" ${CMAKE_PREFIX_PATH})
endif()

# custom cmake scripts
set(ETH_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(ETH_SCRIPTS_DIR ${ETH_CMAKE_DIR}/scripts)

## use multithreaded boost libraries, with -mt suffix
set(Boost_USE_MULTITHREADED ON)
option(Boost_USE_STATIC_LIBS "Link Boost statically" ON)
if(WIN32)
	option(Boost_USE_STATIC_RUNTIME "Link Boost against static C++ runtime libraries" ON)
endif()

set(BOOST_COMPONENTS "filesystem;unit_test_framework;program_options;system")

find_package(Boost 1.65.0 QUIET REQUIRED COMPONENTS ${BOOST_COMPONENTS})

# If cmake is older than boost and boost is older than 1.70,
# find_package does not define imported targets, so we have to
# define them manually.

if (NOT TARGET Boost::boost) # header only target
	add_library(Boost::boost INTERFACE IMPORTED)
	set_property(TARGET Boost::boost APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
endif()
get_property(LOCATION TARGET Boost::boost PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Found Boost headers in ${LOCATION}")

foreach (BOOST_COMPONENT IN LISTS BOOST_COMPONENTS)
	if (NOT TARGET Boost::${BOOST_COMPONENT})
		add_library(Boost::${BOOST_COMPONENT} UNKNOWN IMPORTED)
		string(TOUPPER ${BOOST_COMPONENT} BOOST_COMPONENT_UPPER)
		set_property(TARGET Boost::${BOOST_COMPONENT} PROPERTY IMPORTED_LOCATION ${Boost_${BOOST_COMPONENT_UPPER}_LIBRARY})
		set_property(TARGET Boost::${BOOST_COMPONENT} PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_${BOOST_COMPONENT_UPPER}_LIBRARIES})
		set_property(TARGET Boost::${BOOST_COMPONENT} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
    endif()
	get_property(LOCATION TARGET Boost::${BOOST_COMPONENT} PROPERTY IMPORTED_LOCATION)
	message(STATUS "Found Boost::${BOOST_COMPONENT} at ${LOCATION}")
endforeach()

## We use some of LLVM's libraries for the IELE backend
## Ideally, we look for version 11 or newer.
set(LLVM_MIN_VERSION "11.0.0")
set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
SET(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)
find_package(LLVM REQUIRED CONFIG)
if (${LLVM_VERSION} STRLESS ${LLVM_MIN_VERSION})
	message(FATAL_ERROR "Found LLVM version ${LLVM_VERSION}, which is older than the minimum supported version ${LLVM_MIN_VERSION}")
endif()
message(STATUS "Found LLVM version ${LLVM_VERSION}")
