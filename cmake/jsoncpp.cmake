include(ExternalProject)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(JSONCPP_CMAKE_COMMAND emcmake cmake)
else()
    set(JSONCPP_CMAKE_COMMAND ${CMAKE_COMMAND})
endif()

set(prefix "${CMAKE_BINARY_DIR}/deps")
set(JSONCPP_LIBRARY "${prefix}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jsoncpp${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(JSONCPP_INCLUDE_DIR "${prefix}/include")

# TODO: Investigate why this breaks some emscripten builds and
# check whether this can be removed after updating the emscripten
# versions used in the CI runs.
if(EMSCRIPTEN)
    # Do not include all flags in CMAKE_CXX_FLAGS for emscripten,
    # but only use -std=c++17. Using all flags causes build failures
    # at the moment.
    set(JSONCPP_CXX_FLAGS -std=c++17)
else()
    # jsoncpp uses implicit casts for comparing integer and
    # floating point numbers. This causes clang-10 (used by ossfuzz builder)
    # to error on the implicit conversions. Here, we request jsoncpp
    # to unconditionally use static casts for these conversions by defining the
    # JSON_USE_INT64_DOUBLE_CONVERSION preprocessor macro. Doing so,
    # not only gets rid of the implicit conversion error that clang-10 produces
    # but also forces safer behavior in general.
    set(JSONCPP_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DJSON_USE_INT64_DOUBLE_CONVERSION")
endif()

if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    # Boost libraries use visibility=hidden to reduce unnecessary DWARF entries.
    # Unless we match visibility, ld will give a warning message like:
    #   ld: warning: direct access in function 'boost::filesystem... from file ...
    #   means the weak symbol cannot be overridden at runtime. This was likely caused by different translation units being compiled with different visibility settings.
    set(JSONCPP_CXX_FLAGS "${JSONCPP_CXX_FLAGS} -fvisibility=hidden")
endif()

set(byproducts "")
if(CMAKE_VERSION VERSION_GREATER 3.1)
    set(byproducts BUILD_BYPRODUCTS "${JSONCPP_LIBRARY}")
endif()

# Propagate CMAKE_MSVC_RUNTIME_LIBRARY on Windows builds, if set.
if (WIN32 AND POLICY CMP0091 AND CMAKE_MSVC_RUNTIME_LIBRARY)
    list(APPEND JSONCPP_CMAKE_ARGS "-DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW")
    list(APPEND JSONCPP_CMAKE_ARGS "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}")
endif()

ExternalProject_Add(jsoncpp-project
    PREFIX "${prefix}"
    DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/deps/downloads"
    DOWNLOAD_NAME jsoncpp-1.9.3.tar.gz
    URL https://github.com/open-source-parsers/jsoncpp/archive/1.9.3.tar.gz
    URL_HASH SHA256=8593c1d69e703563d94d8c12244e2e18893eeb9a8a9f8aa3d09a327aa45c8f7d
    CMAKE_COMMAND ${JSONCPP_CMAKE_COMMAND}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
               -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
               -DCMAKE_INSTALL_LIBDIR=lib
               # Build static lib but suitable to be included in a shared lib.
               -DCMAKE_POSITION_INDEPENDENT_CODE=${BUILD_SHARED_LIBS}
               -DJSONCPP_WITH_EXAMPLE=OFF
               -DJSONCPP_WITH_TESTS=OFF
               -DJSONCPP_WITH_PKGCONFIG_SUPPORT=OFF
               -DCMAKE_CXX_FLAGS=${JSONCPP_CXX_FLAGS}
               -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
               ${JSONCPP_CMAKE_ARGS}
    ${byproducts}
)

# Create jsoncpp imported library
add_library(jsoncpp STATIC IMPORTED)
file(MAKE_DIRECTORY ${JSONCPP_INCLUDE_DIR})  # Must exist.
set_property(TARGET jsoncpp PROPERTY IMPORTED_LOCATION ${JSONCPP_LIBRARY})
set_property(TARGET jsoncpp PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${JSONCPP_INCLUDE_DIR})
set_property(TARGET jsoncpp PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${JSONCPP_INCLUDE_DIR})
add_dependencies(jsoncpp jsoncpp-project)
