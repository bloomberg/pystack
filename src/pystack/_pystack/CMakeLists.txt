# PyStack C++ extension module via nanobind

# Find pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Collect all C++ source files
set(PYSTACK_SOURCES
    corefile.cpp
    elf_common.cpp
    logging.cpp
    maps_parser.cpp
    mem.cpp
    native_frame.cpp
    process.cpp
    pycode.cpp
    pyframe.cpp
    pythread.cpp
    pytypes.cpp
    thread_builder.cpp
    unwinder.cpp
    version.cpp
    version_detector.cpp
    bindings.cpp
    interpreter.cpp
)

# Create the nanobind module
nanobind_add_module(
    _pystack
    STABLE_ABI
    NB_STATIC
    ${PYSTACK_SOURCES}
)

# Include directories
# Note: We only include the source directory, not cpython/ directly.
# The cpython headers are included with "cpython/..." prefix to avoid
# conflicts with system headers (e.g., cpython/pthread.h vs /usr/include/pthread.h)
target_include_directories(_pystack PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(_pystack PRIVATE
    Threads::Threads
    stdc++fs
    LIBDW
    LIBELF
)

# Add pthread compile options (needed for C++ threading support on Linux)
target_compile_options(_pystack PRIVATE -pthread)

# Install the module (destination is relative to wheel.install-dir in pyproject.toml)
install(TARGETS _pystack LIBRARY DESTINATION .)
