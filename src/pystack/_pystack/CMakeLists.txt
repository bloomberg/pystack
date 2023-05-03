cmake_minimum_required(VERSION 2.8)
project(_pystack)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PythonInterp 3.7 REQUIRED)
find_package(PythonLibs 3.7 REQUIRED)
IF(NOT PYTHONLIBS_FOUND OR NOT PYTHON_EXECUTABLE)
    MESSAGE(SEND_ERROR "You need Python to build Python binding")
ENDIF(NOT PYTHONLIBS_FOUND OR NOT PYTHON_EXECUTABLE)

add_library(_pystack STATIC
            corefile.cpp
            unwinder.cpp
            logging.cpp
            mem.cpp
            process.cpp
            pycode.cpp
            pyframe.cpp
            pythread.cpp
            version.cpp
            elf_common.cpp
            pytypes.cpp)
set_property(TARGET _pystack PROPERTY POSITION_INDEPENDENT_CODE ON)
include_directories("." "cpython" ${PYTHON_INCLUDE_DIRS})
