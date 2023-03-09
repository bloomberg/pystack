How does ``pystack`` work ?
***************************

Copying memory from a remote process
====================================

When pystack reads memory from a remote process, it uses one of two system
calls depending on whether it is executing in blocking mode or non-blocking
mode:

* ``Ptrace`` (for blocking mode): This system call is very powerful and most
  debuggers use it for different purposes. It allows you to stop a process and
  read its memory, but it also allows you to execute code in the remote
  process, trace system calls. and much more (although pystack only uses it to
  read memory). The downside is that it is quite slow, as it only allows memory
  to be read in machine word chunks. The advantage is that it is always
  present, so reading memory with the process stopped will always yield
  consistent results.

* ``Process_vm_readv`` (for non-blocking mode): This system call allows you to read
  an arbitrary chunk of a remote process" memory in a very fast and efficient
  way: The data moves directly between the address spaces of the two processes,
  without passing through the kernel space. It allows reading memory without
  stopping the process, but unfortunately is not always available in older
  kernels. Also, reading memory with the process running can yield inconsistent
  results, as the memory can be modified while it is being read.

Finding the Python Stack
========================

Finding the Python version
--------------------------

The first thing pystack does is figure out the Python version of the remote
process. To do this, it relies on the fact that Python emits this message when
you start it in interactive mode: ::

    Python 3.8.2 (default, Feb 27 2020, 15:18:51)
    [Clang 11.0.0 (clang-1100.0.33.17)] on darwin
    Type "help", "copyright", "credits" or "license" for more information.
    >>>

This data is created by the process when it starts, and the variables
containing these values are uninitialized beforehand. Because of that, the
symbols that contain this data once the process starts are located in the
``.bss`` section of the process memory. The ``.bss`` section is the portion of
an executable's memory containing statically-allocated variables that are not
explicitly initialized to any value. We can identify the memory region
corresponding to the ``.bss`` section by looking at the process" memory maps:
::

    $ cat /proc/PID/maps

    7fc0db637000-7fc0db98c000 r-xp 00000000 fe:01 1201926          /opt/bb/bin/python3.8
    7fc0db98c000-7fc0dbb8c000 ---p 00355000 fe:01 1201926          /opt/bb/bin/python3.8
    7fc0dbb8c000-7fc0dbb91000 r--p 00355000 fe:01 1201926          /opt/bb/bin/python3.8
    7fc0dbb91000-7fc0dbbcc000 rw-p 0035a000 fe:01 1201926          /opt/bb/bin/python3.8
    7fc0dbbcc000-7fc0dbbec000 rw-p 00000000 00:00 0

The ``.bss`` section is the last map (the one between 7fc0dbbcc000 and
7fc0dbbec000). We know this because it is the first anonymous map (not
associated with any file or library) after the maps associated with the binary
itself (in this case /opt/bb/bin/python3.8). Now that we know where the
``.bss`` section is, the procedure to retrieve the Python version is the
following: ::

    import re
    memory = copy_memory_from_process(pif, bss_location, bss_size)
    evil_regexp = re.compile(rb"((2|3)\.(3|4|5|6|7|8|9)\.(\d{1,2}))((a|b|c|rc)\d{1,2})?\+? (\(.{1,64}\))")
    match = evil_regexp.match(memory)
    _, major_version, minor_version, patch_version, *_ = match.groups()

This allows us to fetch the version reliably, without executing anything or
basing the logic on the file paths (which can be unreliable). If this fails, we
use some other methods as a fallback:

* Analyze the executable or the libpython file path and file name to obtain the
  Python version
* As a last resort, we parse the output of executing python
  --version with the process executable.

Retrieving the Python stack
---------------------------

When we talk about a process" Python stack, we must realize that all the Python
code actually does not exist. The Python code is transformed by the Python
compiler into data (bytecode) that the interpreter executes. When the
interpreter executes this data, it creates some internal structures (more data)
containing all the details of what it is currently executing. Our objective
here is to retrieve the data in these internal structures. To do this, we
leverage our knowledge of the internal structure of the CPython interpreter. In
particular, we know that CPython stores the information related to the
interpreter in a C struct called ``PyInterpreterState``: ::

    typedef struct _is
    {
        ...
        struct PyThreadState *next;
        struct PyThreadState *tstate_head;
        ...
    } PyInterpreterState;

This structure has a pointer to a structure contained in a linked list of
thread states: ::

    typedef struct _ts
    {
        ...
        struct PyThreadState *next;
        PyInterpreterState *interp;
        Struct _frame *frame;
        ...
    } PyThreadState;

In turn, this has a pointer (frame) to a linked list of frames: ::

    typedef struct _frame
    {
        ...
        Struct _frame *f_back;
        PyCodeObject *f_code;
        ...
        int f_lineno;
        int f_lastli;
        ...
    } PyFrameObject;

Every frame contains a pointer to the code object that is being executed in
that frame: ::

    typedef struct _code
    {
        ...
        PyObject *co_filename;
        PyObject *co_name;
        PyObject co_firstlineno;
        PyObject *co_lnotab
        ...
    } PyCodeObject;

Analyzing the attributes present in every code object, for every frame in every
thread, and for every thread in the interpreter state, we can retrieve all the
function names and associated metadata in the Python stack of the process. Of
course, this is not an easy task. Not only do all these structs change per
Python version, we also need to know the internal structure of the PyObject
pointers in the code object. To do this, pystack stores all the changes in
these structures between different Python versions and it knows how to analyze
some basic Python objects (that also change per version) that are needed: byte
objects, string objects and integer objects. Once pystack has identified the
Python version using the procedure described previously, it knows how to
interpret the memory that is copying from the remote process. Now, the question
is how to know where these structs are located in the remote process. For this,
pystack uses two methods: symbol analysis and brute force.

Symbol analysis
^^^^^^^^^^^^^^^

If the executable associated with the process under analysis has symbol
information, we can use it to quickly locate the ``PyInterpreterState`` in
memory, taking advantage of the fact that the Python interpreter stores this
structure in a global variable (_PyRuntime or interp_head depending on the
Python version). Finding the location of the ``PyInterpreterState`` in a remote
process can be a challenging task due to Address Space Layout Randomization. To
find where that global variable lives in memory, we need to:

* Locate where the interpreter (libpython or the Python binary) is placed in
  memory.
* Locate the first byte of the interpreter file that is loaded into
  memory (the linker can start mapping at some arbitrary location in the file).
* Locate the offset of the symbol in the interpreter file.

To locate where the interpreter is placed into memory, we need to read the
memory maps from the proc pseudo-filesystem: ::

    $ cat /proc/PID/maps

    7fc0db637000-7fc0db98c000 r-xp 00000000 fe:01 1201926            /opt/bb/bin/python3.8
    7fc0db98c000-7fc0dbb8c000 ---p 00355000 fe:01 1201926            /opt/bb/bin/python3.8
    7fc0dbb8c000-7fc0dbb91000 r--p 00355000 fe:01 1201926            /opt/bb/bin/python3.8
    7fc0dbb91000-7fc0dbbcc000 rw-p 0035a000 fe:01 1201926            /opt/bb/bin/python3.8
    7fc0dbbcc000-7fc0dbbec000 rw-p 00000000 00:00 0

In this case, we know the interpreter maps start at ``7fc0db637000``(the beginning
of the first map). To know what the first byte is that will be loaded in the
``/opt/bb/bin/python3.8`` file, we look at the program headers in the ``ELF``
information table of the binary: ::

    $readelf -l /opt/bb/bin/python3.8

    Elf file type is EXEC (Executable file)
    Entry point 0x400658
    There are 9 program headers, starting at offset 64

    Program Headers:
      Type           Offset             VirtAddr           PhysAddr
                     FileSiz            MemSiz              Flags  Align
      PHDR           0x0000000000000040 0x0000000000400040 0x0000000000400040
                     0x00000000000001f8 0x00000000000001f8  R E    8
      INTERP         0x0000000000000238 0x0000000000400238 0x0000000000400238
                     0x000000000000001c 0x000000000000001c  R      1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
      LOAD           0x0000000000000000 0x0000000000400000 0x0000000000400000
                     0x00000000000008d4 0x00000000000008d4  R E    200000
      LOAD           0x0000000000000d98 0x0000000000600d98 0x0000000000600d98

      ...

The segments to be loaded into memory are the ones with type ``LOAD``. Every
``LOAD`` segment corresponds (more or less, because the linker can still split
them) to every segment present in the memory maps that appear when reading from
``/proc/PID/maps``. In this case, we know the first segment in
``/proc/PID/maps`` corresponds to the first ``LOAD`` because both have
executable permissions (an "x" in "r-xp" for the one in ``/proc/PID/maps`` and
an ``E`` in ``R E`` for the one in the ``ELF`` segments). This segment has
executable permissions because it contains the actual code that will be
executed in the process. The column we are interested in is the VirtAddr
column, which contains the offset at which the linker will load that segment.
In our case, this is ``0x0000000000400000`` (standard for ``x86_64`` systems).

To find the location of the symbol, we must look at the symbol table: ::

    $ readelf -s /opt/bb/bin/python3.8 | grep _PyRuntime
        1879: 00000000005b1740  1464 OBJECT  GLOBAL DEFAULT   24 _PyRuntime

From this, we know the symbol is located at offset ``00000000005b1740`` in the
``/opt/bb/bin/python3.8`` file. With this information, we know the symbol is
located at: ::

    Start_address_of_the_map_in_memory -
    offset_of_the_first_byte_loaded_in_memory +
    offset_of_the_symbol =
    0x7fc0db637000 - 0x0000000000400000 + 0x00000000005b1740 =
    0x7fc0db7e8740

If we attach with gdb, we can see that this is indeed the location: ::

    [root@dd720eb38548 pystack]# gdb -p 8205
    GNU gdb (Bloomberg__9.1-0+b20200701T10350908) 9.1
    (gdb) info address _PyRuntime
    Symbol "_PyRuntime" is static storage at address 0x7fc0db7e8740.

Once we know where the _PyRuntime symbol is, we copy the memory at that
location and we retrieve all the information we need by reading pointers,
copying the memory these pointers point to, and reinterpreting the memory as
structures we know correspond to this memory (the ones described previously:
``PyInterpreterState``, ``PyThreadState``, ``PyFrameObject``, and ``PyCodeObject``).

Using brute force
^^^^^^^^^^^^^^^^^
But, we said that Pystack must work even if we don"t have symbols available.
This is important because some distributions (like Ubuntu or Debian) ship
versions of the Python interpreter that do not have any symbolic information
available. So, our previous strategy won"t work with these binaries.
Fortunately, we have a secret trick up our sleeve that will help here. Let's
look carefully at the first two structures we are interested in: ::

    typedef struct _is
    {
        ...
        struct PyThreadState *next;
        ...
    } PyInterpreterState;

    typedef struct _ts
    {
        ...
        PyInterpreterState *interp;
        ...
    } PyThreadState;

We can see they form a cycle. The ``PyInterpreterState`` points to the
``PyThreadState`` structure and this structure points back to the
PyInterpreterSate. We also know that the ``PyInterpreterState`` structure lives in
the ``.bss`` section of the interpreter because its memory starts uninitialized.
With all this information, we can proceed as follows:

1. Copy all the memory stored in the interpreter's ``.bss`` section.
2. Scan the just copied memory in increments of a pointer.
3. For every possible pointer, let's assume this pointer is pointing to a
   InterpeterState structure and read the field that corresponds to the pointer
   to the ``PyThreadState`` structure.
4. Is the pointer to ``PyThreadState`` valid? Does it point to memory we can
   read from the remote process? If the answer is False, repeat copying of the
   memory stored in the next pointer in the ``.bss`` section.
5. If the memory we just copied corresponds to a ``PyThreadState``, read the
   field that corresponds to the pointer to the ``PyInterpreterState``
   structure.
6. Is the pointer to the ``PyInterpreterState`` valid? Does it point to memory
   we can read from the remote process? If the answer is False, repeat with the
   next pointer. Otherwise, check if the pointer points to the original pointer
   we started at (so these two structures form a cycle). If that is False,
   continue with the next pointer in the ``.bss`` section.
7. As a last check, try to construct a single Python stack frame from this
   pointer. If successful, we have found the location of the
   ``PyInterpreterState`` structure.

As you can imagine, this process can take more time than using symbols.
Normally the ``.bss`` section of the Python interpreter is considerably small, so,
in practice, it finishes in just a hundred milliseconds, which is acceptable.
As before, once we know the location of the ``PyInterpreterState`` variable, we
copy the memory at that location and we retrieve all the information we need by
reading the relevant pointers and the memory they point to.

As a last resort, if scanning the ``.bss`` section fails, Pystack performs the same
scanning procedure on the process heap.

Unwinding the native stack
==========================
Pystack can do more than just retrieving the Python stack of a process or core
dump: it can also merge the native C/C++ stack with the Python stack to provide
much richer information about exactly what is going on. To merge the Python and
native stacks, Pystack first needs to obtain the native stack. This procedure
is known as "stack unwinding". Unwinding the native stack is the process of
walking up the process stack to know what native function calls were made until
the program reached its current status. This process can be very challenging
and involves quite a lot of low-level manipulation. A call stack is composed of
stack frames. These machine dependent and ABI-dependent data structures contain
subroutine state information. Each stack frame corresponds to a function call
which has not yet terminated with a return. The stack frame atop the stack is
for the currently executing function.

Normally, the executing program keeps track of two important pointers as it
runs: the instruction pointer, which points to the next instruction it will
execute, and the stack pointer, which points to the last value pushed onto the
stack. In non-optimized mode, the stack also contains the value of the base
pointer register (``%rbp`` in ``x86``), which is conventionally used to mark
the start of a function's stack frame, or the area of the stack managed by that
function.  The main idea of stack unwinding (if we have the base pointers) is
to use the value saved on the stack to go back to the previous stack. The
problem is that compilers normally omit the base pointer (which, if present,
would allow us to obtain address of the previous frame because it is
callee-saved) when doing optimizations since not having a base pointer can
yield better performance.

Unfortunately, not having a base pointer makes our life much more difficult. So
the canonical solution is to use the frame unwinding information found in the
.eh_frame/.debug_frame ``ELF`` sections. These sections of the binary contain a
kind of ``DWARF`` information that allows the stack to be unwound. This is used
for multiple purposes, one of them being enabling the C++ runtime to implement
exception handling (as exceptions need to unwind the stack to propagate). If
you analyze the information present in this section, you will find numerous
entries that look like this: ::

    $readelf -wf /opt/bb/lib64/libpython3.8.so

    Contents of the .eh_frame section:

    000000a8 000000000000004c 000000ac FDE cie=00000000 pc=0000000000119430..0000000000119acb
      DW_CFA_def_cfa_offset: 16
      DW_CFA_advance_loc: 6 to 0000000000119432
      DW_CFA_def_cfa_offset: 24
      DW_CFA_advance_loc: 10 to 0000000000119434
      DW_CFA_def_cfa_expression (DW_OP_breg7 (rsp): 8; DW_OP_breg16 (rip): 0; DW_OP_lit15;
         DW_OP_and; DW_OP_lit11; DW_OP_ge; DW_OP_lit3; DW_OP_shl; DW_OP_plus)
      DW_CFA_nop
      DW_CFA_nop
      DW_CFA_nop
    ...

There's a lot of information here. But, the most important realization is that
the information contained in these entries is not presented in "raw" format.
Instead what is present is bytecode for a turing complete stack machine that,
when executed, will give you the actual information. Many of these instructions
just tell something like “advance some position” or “some register is at offset
X from the position you are currently”, but in the most complicated cases, it
can contain arbitrary expressions like this one from the previous example: ::

    DW_CFA_def_cfa_expression (DW_OP_breg7 (rsp): 8; DW_OP_breg16 (rip): 0; DW_OP_lit15;
        DW_OP_and; DW_OP_lit11; DW_OP_ge; DW_OP_lit3; DW_OP_shl; DW_OP_plus)

This may sound ridiculous, but the reason this is made this way is because
storing the raw information would require much much more space, and it can be
easily compressed by these means. If you decode the information by executing
the state machine, you will see this is actually encoding a gigantic table: ::

    $readelf -wF /opt/bb/lib64/libpython3.8.so

    000000a8 000000000000004c 000000ac FDE cie=00000000 pc=0000000000119430..0000000000119acb
       LOC           CFA      rbx   rbp   r12   r13   r14   r15   ra
    0000000000119430 rsp+8    u     u     u     u     u     u     c-8
    0000000000119432 rsp+16   u     u     u     u     u     c-16  c-8
    0000000000119434 rsp+24   u     u     u     u     c-24  c-16  c-8
    0000000000119439 rsp+32   u     u     u     c-32  c-24  c-16  c-8
    000000000011943b rsp+40   u     u     c-40  c-32  c-24  c-16  c-8
    000000000011943c rsp+48   u     c-48  c-40  c-32  c-24  c-16  c-8
    000000000011943d rsp+56   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119441 rsp+128  c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119a72 rsp+56   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119a73 rsp+48   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119a74 rsp+40   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119a76 rsp+32   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119a78 rsp+24   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119a7a rsp+16   c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119a7c rsp+8    c-56  c-48  c-40  c-32  c-24  c-16  c-8
    0000000000119a7d rsp+128  c-56  c-48  c-40  c-32  c-24  c-16  c-8

    …

This table contains one row for every possible instruction in the program.
The different values in the table represent rules that allow you to
retrieve the value of the different registers, the return address of the
given function, and the base pointer associated with the address in the
first column. Given this table, the pseudo code for unwinding the stack
would be: ::

    def get_stack_frame():
        pc, sp, fp = fetch_initial_cpu_registers()

        yield pc

        while sp is not None:
            Current_ruleset = lookup_in_huge_table(pc)
            pc = evaluate_rule(current_ruleset.rule_for_PC, pc, sp, fp)
            sp = evaluate_rule(current_ruleset.rule_for_SP, pc, sp, fp)    
            fp = evaluate_rule(current_ruleset.rule_for_FP, pc, sp, fp)
            yield pc

The normal "rules" here are "add some constant to the value of the program
counter" or "add some constant to the value of the program counter and retrieve
the memory at that location," though there are other, more complicated, rules
that can appear. With this procedure, we can obtain the addresses of the
functions in the stack. This procedure will give us the addresses of the
instructions of the functions that form the native stack, but in order to
retrieve the names of the functions we need to use the debug information
present in the program (the ``DWARF`` debug information, which is different
from the ``DWARF`` information that we used for unwinding). The ``DWARF`` debug
information is a tree structure where each node can have children or siblings.
The nodes might represent types, variables, or functions. ``DWARF`` uses a
series of debugging information entries (DIEs) to define a low-level
representation of a source program. Each debugging information entry consists
of an identifying tag and a series of attributes. An entry or group of entries
together, provides a description of a corresponding entity in the source
program. The tag specifies the class to which an entry belongs and the
attributes define the specific characteristics of the entry. In particular, the
nodes describing functions look like this (simplified): ::

    DW_TAG_subprogram

    DW_AT_low_pc              	0x00400670
    DW_AT_high_pc             	0x0040069c
    DW_AT_name                	bar

This tells us that the function bar is present between address ``0x00400670``
and address ``0x0040069c``. Of course, the binary will have tons of these
entries, but the main idea is that once we know the return address, we iterate
over the ``DWARF`` structure finding a node with a function such that our
return address lies between the function address range. Once we find it we can
use the DW_AT_name attribute to retrieve the function name. The catch is that
the actual way to do this is more complicated, because every address we obtain
can be associated with multiple functions if those were inlined or specialized
by the compiler. However, by using the ``DWARF`` information (and some extra
effort too complex to describe in this article), it is possible to retrieve
even these inlined functions.

Core files
==========

Pystack can also analyze core files in order to retrieve the Python stack. The
procedure followed is very similar, except that we cannot read
``/proc/PID/maps`` anymore to retrieve the memory maps of the process and we
cannot use ptrace or process_vm_readv anymore. Solving the second problem is
very easy: we just read from the core file because that contains the dump of
the process memory from when it was created. The only problem is that we need a
way to translate memory addresses in the process memory into offsets in the
file. For that, we need to know the structure of the core files. Unfortunately,
this is not very well documented and retrieving this information usually
requires reading the source of binutils tools.

However, it turns out that core files are also ``ELF`` files, so we can read them
with an ``ELF`` parser. The only difference is that the information stored in the
``ELF`` sections and segments has different meanings. For instance, if we look at
the program headers, we will find: ::

    $readelf -l /path/to/corefile
    Program Headers:
      Type           Offset             VirtAddr           PhysAddr
                     FileSiz            MemSiz              Flags  Align
      NOTE           0x0000000000004b80 0x0000000000000000 0x0000000000000000
                     0x0000000000009064 0x0000000000000000  R      1
      LOAD           0x000000000000dbe4 0x0000000000400000 0x0000000000000000
                     0x0000000000000000 0x000000000009d000  R E    1
      LOAD           0x000000000000dbe4 0x000000000069c000 0x0000000000000000
                     0x0000000000004000 0x0000000000004000  RW     1
      LOAD           0x0000000000011be4 0x00000000006a0000 0x0000000000000000
                     0x0000000000004000 0x0000000000004000  RW     1
      LOAD           0x0000000000015be4 0x0000000001872000 0x0000000000000000
                     0x0000000000ed4000 0x0000000000ed4000  RW     1
      LOAD           0x0000000000ee9be4 0x00007f248c000000 0x0000000000000000
                     0x0000000000021000 0x0000000000021000  RW     1
      LOAD           0x0000000000f0abe4 0x00007f2490885000 0x0000000000000000
                     0x000000000001c000 0x000000000001c000  R      1
      LOAD           0x0000000000f26be4 0x00007f24908a1000 0x0000000000000000
                     0x000000000001c000 0x000000000001c000  R      1
      LOAD           0x0000000000f42be4 0x00007f24908bd000 0x0000000000000000
                     0x00000000005f3000 0x00000000005f3000  R      1
      LOAD           0x0000000001535be4 0x00007f2490eb0000 0x0000000000000000
                     0x0000000000000000 0x0000000000002000  R E    1
      LOAD           0x0000000001535be4 0x00007f24910b1000 0x0000000000000000

Wow, that is a lot of ``LOAD`` segments. Well, it turns out that every ``LOAD``
segment corresponds to a different virtual memory map that was present in
``/proc/PID/maps`` when the process was alive. Here, VirtAddr is the virtual
address of the beginning of the virtual memory map and Offset is the offset of
the corresponding data in the core dump file (not the offset in the original
mapped file). The name of the mapped file and the offset in this file are not
described here. but are described in the PT_NOTE section (retrieving this
information is more complex and will not be covered here). We can then use this
table as a translation between addresses in memory and offsets in the file.
Having this knowledge, we can repeat the procedures described previously
(caveat: every time we copy memory from the core file, we must do an extra step
to translate the memory address we want to copy to an offset in the file).

Of course, I waved my hand when I said that "we can use this as a translation
table" because the process is, unsurprisingly, not that easy. The reason is
that some of the memory maps may be missing because the kernel decided to omit
them when creating the dump or because retrieving symbols in the core file must
sometimes be done a different way. Additionally, some pieces of information
must be retrieved from the core file in unique ways (e.g., the process ID, the
thread IDs). To solve both of these problems, Pystack uses the ``DWARF``
information present in the core to obtain all the missing pieces so the process
of analyzing the core and a remote process are as similar as possible.
