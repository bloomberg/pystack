from _pystack.native_frame cimport NativeFrame
from _pystack.process cimport AbstractProcessManager
from _pystack.process cimport remote_addr_t
from _pystack.pyframe cimport FrameObject
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string as cppstring
from libcpp.vector cimport vector


cdef extern from "pythread.h" namespace "pystack":
    cdef cppclass NativeThread "pystack::Thread":
        NativeThread(int, int) except+
        int Tid()
        vector[NativeFrame]& NativeFrames()
        void populateNativeStackTrace(shared_ptr[AbstractProcessManager] manager) except+

cdef extern from "pythread.h" namespace "pystack::PyThread":
    cdef enum GilStatus:
        UNKNOWN = -1
        NOT_HELD = 0
        HELD = 1

    cdef enum GCStatus:
        COLLECTING_UNKNOWN = -1
        NOT_COLLECTING = 0
        COLLECTING = 1

cdef extern from "pythread.h" namespace "pystack":
    cdef cppclass Thread "pystack::PyThread":
        int Tid()
        shared_ptr[FrameObject] FirstFrame()
        shared_ptr[Thread] NextThread()
        vector[NativeFrame]& NativeFrames()
        GilStatus isGilHolder()
        GCStatus isGCCollecting()
        void populateNativeStackTrace(shared_ptr[AbstractProcessManager] manager) except+
    shared_ptr[Thread] getThreadFromInterpreterState(shared_ptr[AbstractProcessManager] manager, remote_addr_t addr) except+
