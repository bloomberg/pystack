cdef extern from "version.h" namespace "pystack":
   void setVersion(int major, int minor) except +