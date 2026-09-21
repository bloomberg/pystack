cmake_minimum_required(VERSION 3.17...3.27)

project(pystack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Find Python
find_package(Python COMPONENTS Interpreter Development.Module Development.SABIModule REQUIRED)

# Set compiler flags
set(lto_configs RELEASE RELWITHDEBINFO MINSIZEREL)
string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type)

# Probe for LTO support if a configuration that would use it can be built.
if(CMAKE_CONFIGURATION_TYPES OR build_type IN_LIST lto_configs)
    include(CheckIPOSupported)

    check_ipo_supported(
        RESULT ipo_supported
        OUTPUT ipo_error
        LANGUAGES CXX
    )

    if(ipo_supported)
        foreach(config IN LISTS lto_configs)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_${config} ON)
        endforeach()
    else()
        message(WARNING "LTO disabled: ${ipo_error}")
    endif()
endif()

add_compile_options(-Wall)
add_compile_definitions(_FILE_OFFSET_BITS=64)

add_compile_definitions(
    "$<$<CONFIG:Debug>:_GLIBCXX_DEBUG>"
    "$<$<CONFIG:Debug>:_LIBCPP_DEBUG>"
)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og")

# Find nanobind
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

# Find libelf and libdw via pkg-config
function(make_pkgconfig_target target modulespec)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(${target} REQUIRED ${modulespec})

    set(static "")
    if(${target}_LINK_LIBRARIES)
        list(GET ${target}_LINK_LIBRARIES 0 lib)
        get_filename_component(ext "${lib}" EXT)
        if(ext STREQUAL "${CMAKE_STATIC_LIBRARY_SUFFIX}")
            set(static "_STATIC")
        endif()
    endif()

    # pkg-config doesn't report that libdw.a needs libdl.so
    set(libraries ${${target}${static}_LIBRARIES})
    if(static)
        list(APPEND libraries ${CMAKE_DL_LIBS})
    endif()

    add_library(${target} INTERFACE IMPORTED)
    set_target_properties(${target} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${${target}${static}_INCLUDE_DIRS}"
        INTERFACE_LINK_DIRECTORIES    "${${target}${static}_LIBRARY_DIRS}"
        INTERFACE_LINK_LIBRARIES      "${libraries}"
        INTERFACE_COMPILE_OPTIONS     "${${target}${static}_CFLAGS_OTHER}"
        INTERFACE_LINK_OPTIONS        "${${target}${static}_LDFLAGS_OTHER}"
    )
endfunction()

make_pkgconfig_target(LIBELF libelf)
make_pkgconfig_target(LIBDW  libdw)

# Add the extension module subdirectory
add_subdirectory(src/pystack/_pystack)
