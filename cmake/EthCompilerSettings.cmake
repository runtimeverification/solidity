#------------------------------------------------------------------------------
# EthCompilerSettings.cmake
#
# CMake file for cpp-ethereum project which specifies our compiler settings
# for each supported platform and build configuration.
#
# The documentation for cpp-ethereum is hosted at http://cpp-ethereum.org
#
# Copyright (c) 2014-2016 cpp-ethereum contributors.
#------------------------------------------------------------------------------

# Clang seeks to be command-line compatible with GCC as much as possible, so
# most of our compiler settings are common between GCC and Clang.
#
# These settings then end up spanning all POSIX platforms (Linux, OS X, BSD, etc)

include(EthCheckCXXCompilerFlag)

eth_add_cxx_compiler_flag_if_supported(-fstack-protector-strong have_stack_protector_strong_support)
if(NOT have_stack_protector_strong_support)
	eth_add_cxx_compiler_flag_if_supported(-fstack-protector)
endif()

eth_add_cxx_compiler_flag_if_supported(-Wimplicit-fallthrough)

if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))

	# Use ISO C++11 standard language.
	set(CMAKE_CXX_FLAGS -std=c++11)

	# Enables all the warnings about constructions that some users consider questionable,
	# and that are easy to avoid.  Also enable some extra warning flags that are not
	# enabled by -Wall.   Finally, treat at warnings-as-errors, which forces developers
	# to fix warnings as they arise, so they don't accumulate "to be fixed later".
	add_compile_options(-Wall)
	add_compile_options(-Wextra)
	add_compile_options(-Werror)

	# Disable warnings about unknown pragmas (which is enabled by -Wall).  I assume we have external
	# dependencies (probably Boost) which have some of these.   Whatever the case, we shouldn't be
	# disabling these globally.   Instead, we should pragma around just the problem #includes.
	#
	# TODO - Track down what breaks if we do NOT do this.
	add_compile_options(-Wno-unknown-pragmas)

	# Disable warnings about unused parameters (which is enabled by -Wall).  The LLVM external
	# dependency  has some of these.
	add_compile_options(-Wno-unused-parameter)

	# To get the code building on FreeBSD and Arch Linux we seem to need the following
	# warning suppression to work around some issues in Boost headers.
	#
	# See the following reports:
	#     https://github.com/ethereum/webthree-umbrella/issues/384
	#     https://github.com/ethereum/webthree-helpers/pull/170
	#
	# The issue manifest as warnings-as-errors like the following:
	#
	#     /usr/local/include/boost/multiprecision/cpp_int.hpp:181:4: error:
	#         right operand of shift expression '(1u << 63u)' is >= than the precision of the left operand
	#
	# -fpermissive is a pretty nasty way to address this.   It is described as follows:
	#
	#    Downgrade some diagnostics about nonconformant code from errors to warnings.
	#    Thus, using -fpermissive will allow some nonconforming code to compile.
	#
	# NB: Have to use this form for the setting, so that it only applies to C++ builds.
	# Applying -fpermissive to a C command-line (ie. secp256k1) gives a build error.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

	# Configuration-specific compiler settings.
	set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g -DETH_DEBUG")
	set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

	# Additional GCC-specific compiler settings.
	if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")

		# Check that we've got GCC 4.7 or newer.
		execute_process(
			COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
		if (NOT (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7))
			message(FATAL_ERROR "${PROJECT_NAME} requires g++ 4.7 or greater.")
		endif ()

	# Additional Clang-specific compiler settings.
	elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")

		# A couple of extra warnings suppressions which we seemingly
		# need when building with Clang.
		#
		# TODO - Nail down exactly where these warnings are manifesting and
		# try to suppress them in a more localized way.   Notes in this file
		# indicate that the first is needed for sepc256k1 and that the
		# second is needed for the (clog, cwarn) macros.  These will need
		# testing on at least OS X and Ubuntu.
		add_compile_options(-Wno-unused-function)
		add_compile_options(-Wno-dangling-else)

		if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
			# Set stack size to 16MB - by default Apple's clang defines a stack size of 8MB, some tests require more.
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-stack_size -Wl,0x1000000")
		endif()

		# Some Linux-specific Clang settings.  We don't want these for OS X.
		if ("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")

			# TODO - Is this even necessary?  Why?
			# See http://stackoverflow.com/questions/19774778/when-is-it-necessary-to-use-use-the-flag-stdlib-libstdc.
			add_compile_options(-stdlib=libstdc++)
			
			# Tell Boost that we're using Clang's libc++.   Not sure exactly why we need to do.
			add_definitions(-DBOOST_ASIO_HAS_CLANG_LIBCXX)
			
			# Use fancy colors in the compiler diagnostics
			add_compile_options(-fcolor-diagnostics)
			
			# See "How to silence unused command line argument error with clang without disabling it?"
			# When using -Werror with clang, it transforms "warning: argument unused during compilation" messages
			# into errors, which makes sense.
			# http://stackoverflow.com/questions/21617158/how-to-silence-unused-command-line-argument-error-with-clang-without-disabling-i
			add_compile_options(-Qunused-arguments)
		endif()

		if (EMSCRIPTEN)
			# Do not emit a separate memory initialiser file
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --memory-init-file 0")
			# Leave only exported symbols as public and agressively remove others
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -Wl,--gc-sections -fvisibility=hidden")
			# Optimisation level
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
			# Re-enable exception catching (optimisations above -O1 disable it)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")
			# Remove any code related to exit (such as atexit)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s NO_EXIT_RUNTIME=1")
			# Remove any code related to filesystem access
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s NO_FILESYSTEM=1")
			# Remove variables even if it needs to be duplicated (can improve speed at the cost of size)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s AGGRESSIVE_VARIABLE_ELIMINATION=1")
			# Allow memory growth, but disable some optimisations
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
			# Disable eval()
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s NO_DYNAMIC_EXECUTION=1")
			# Disable greedy exception catcher
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s NODEJS_CATCH_EXIT=0")
			# Abort if linking results in any undefined symbols
			# Note: this is on by default in the CMake Emscripten module which we aren't using
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ERROR_ON_UNDEFINED_SYMBOLS=1")
			add_definitions(-DETH_EMSCRIPTEN=1)
		endif()
	endif()

# The major alternative compiler to GCC/Clang is Microsoft's Visual C++ compiler, only available on Windows.
elseif (DEFINED MSVC)

    add_compile_options(/MP)						# enable parallel compilation
	add_compile_options(/EHsc)						# specify Exception Handling Model in msvc
	add_compile_options(/WX)						# enable warnings-as-errors
	add_compile_options(/wd4068)					# disable unknown pragma warning (4068)
	add_compile_options(/wd4996)					# disable unsafe function warning (4996)
	add_compile_options(/wd4503)					# disable decorated name length exceeded, name was truncated (4503)
	add_compile_options(/wd4267)					# disable conversion from 'size_t' to 'type', possible loss of data (4267)
	add_compile_options(/wd4180)					# disable qualifier applied to function type has no meaning; ignored (4180)
	add_compile_options(/wd4290)					# disable C++ exception specification ignored except to indicate a function is not __declspec(nothrow) (4290)
	add_compile_options(/wd4244)					# disable conversion from 'type1' to 'type2', possible loss of data (4244)
	add_compile_options(/wd4800)					# disable forcing value to bool 'true' or 'false' (performance warning) (4800)
	add_compile_options(-D_WIN32_WINNT=0x0600)		# declare Windows Vista API requirement
	add_compile_options(-DNOMINMAX)					# undefine windows.h MAX && MIN macros cause it cause conflicts with std::min && std::max functions

	# Always use Release variant of C++ runtime.
	# We don't want to provide Debug variants of all dependencies. Some default
	# flags set by CMake must be tweaked.
	string(REPLACE "/MDd" "/MD" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REPLACE "/D_DEBUG" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REPLACE "/MDd" "/MD" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	string(REPLACE "/D_DEBUG" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS OFF)

	# disable empty object file warning
	set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")
	# warning LNK4075: ignoring '/EDITANDCONTINUE' due to '/SAFESEH' specification
	# warning LNK4099: pdb was not found with lib
	# stack size 16MB
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099,4075 /STACK:16777216")

# If you don't have GCC, Clang or VC++ then you are on your own.  Good luck!
else ()
	message(WARNING "Your compiler is not tested, if you run into any issues, we'd welcome any patches.")
endif ()

if (SANITIZE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=${SANITIZE}")
	if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
		set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/sanitizer-blacklist.txt")
	endif()
endif()

if (PROFILING AND (("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")))
	set(CMAKE_CXX_FLAGS "-g ${CMAKE_CXX_FLAGS}")
	set(CMAKE_C_FLAGS "-g ${CMAKE_C_FLAGS}")
	add_definitions(-DETH_PROFILING_GPERF)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lprofiler")
#	set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -lprofiler")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lprofiler")
endif ()

if (PROFILING AND (("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")))
        set(CMAKE_CXX_FLAGS "-g --coverage ${CMAKE_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "-g --coverage ${CMAKE_C_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "--coverage ${CMAKE_SHARED_LINKER_FLAGS} -lprofiler")
        set(CMAKE_EXE_LINKER_FLAGS "--coverage ${CMAKE_EXE_LINKER_FLAGS} -lprofiler")
endif ()

if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))
	option(USE_LD_GOLD "Use GNU gold linker" ON)
	if (USE_LD_GOLD)
		execute_process(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
		if ("${LD_VERSION}" MATCHES "GNU gold")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fuse-ld=gold")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
		endif ()
	endif ()
endif ()
