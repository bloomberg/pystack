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
add_compile_options(-Wall)
add_compile_definitions(_FILE_OFFSET_BITS=64)

add_compile_definitions(
    "$<$<CONFIG:Debug>:_GLIBCXX_DEBUG>"
    "$<$<CONFIG:Debug>:_LIBCPP_DEBUG>"
)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -flto")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og")

# Find nanobind
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

# Find libelf and libdw via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBELF REQUIRED libelf)
pkg_check_modules(LIBDW REQUIRED libdw)

# Add the extension module subdirectory
add_subdirectory(src/pystack/_pystack)
