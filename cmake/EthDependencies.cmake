# all dependencies that are not directly included in the cpp-ethereum distribution are defined here
# for this to work, download the dependency via the cmake script in extdep or install them manually!

function(eth_show_dependency DEP NAME)
	get_property(DISPLAYED GLOBAL PROPERTY ETH_${DEP}_DISPLAYED)
	if (NOT DISPLAYED)
		set_property(GLOBAL PROPERTY ETH_${DEP}_DISPLAYED TRUE)
		message(STATUS "${NAME} headers: ${${DEP}_INCLUDE_DIRS}")
		message(STATUS "${NAME} lib   : ${${DEP}_LIBRARIES}")
		if (NOT("${${DEP}_DLLS}" STREQUAL ""))
			message(STATUS "${NAME} dll   : ${${DEP}_DLLS}")
		endif()
	endif()
endfunction()

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

find_package(Boost 1.54.0 QUIET REQUIRED COMPONENTS regex filesystem unit_test_framework program_options system)

eth_show_dependency(Boost boost)

## We use some of LLVM's libraries for the IELE backend
## Ideally, we look for version 6. 
## If now found, let's look for version 5, otherwise 4.

## NB: From cmake version >= 3.7 there's a better way: 
## See https://stackoverflow.com/a/44970744
## See: https://cmake.org/cmake/help/v3.9/variable/CMAKE_FIND_PACKAGE_SORT_ORDER.html#variable:CMAKE_FIND_PACKAGE_SORT_ORDER
## set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)

find_package(LLVM 6.0.0 QUIET CONFIG)

if (NOT LLVM_FOUND) 
	find_package(LLVM 5.0.0 CONFIG)	
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I /usr/include/llvm5")	
endif()

if (NOT LLVM_FOUND) 
	find_package(LLVM 4.0.0 REQUIRED CONFIG)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I /usr/include/llvm4")	
endif()

eth_show_dependency(LLVM LLVM)
