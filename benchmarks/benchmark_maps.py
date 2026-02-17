from pystack.maps import VirtualMap
from pystack.maps import _get_base_map
from pystack.maps import _get_bss
from pystack.maps import parse_maps_file_for_binary
from pystack.errors import MissingExecutableMaps
from pystack.errors import ProcessNotFound, PystackError
from pystack.maps import generate_maps_for_process
from pathlib import Path
import pytest
from unittest.mock import patch, mock_open
import os

RANGE=100

def time_virtual_map_creation():
    for i in range(RANGE):
        map = VirtualMap(
            start=0,
            end=10,
            offset=1234,
            device="device",
            flags="xrwp",
            inode=42,
            path=None,
            filesize=10,
        )

def time_simple_maps_no_such_pid():
    for i in range(RANGE):
        with patch("builtins.open", side_effect=FileNotFoundError()):
            # WHEN / THEN
            with pytest.raises(ProcessNotFound):
                list(generate_maps_for_process(1))

def time_simple_maps():
    for i in range(RANGE):
        map_text = (
            "7f1ac1e2b000-7f1ac1e50000 "
            "r--p "
            "00000000 08:12 8398159"
            "                    /usr/lib/libc-2.31.so"
        )

        with patch("builtins.open", mock_open(read_data=map_text)):
            maps = list(generate_maps_for_process(1))

def time_maps_with_long_device_numbers():
    for i in range(RANGE):
        map_text = (
            "7f1ac1e2b000-7f1ac1e50000 "
            "r--p 00000000 0123:4567 "
            "8398159 /usr/lib/libc-2.31.so"
        )

        # WHEN

        with patch("builtins.open", mock_open(read_data=map_text)):
            maps = list(generate_maps_for_process(1))

def time_annonymous_maps():
    for i in range(RANGE):
        map_text = """
            7f1ac1e2b000-7f1ac1e50000 r--p 00000000 08:12 8398159
        """

        # WHEN

        with patch("builtins.open", mock_open(read_data=map_text)):
            maps = list(generate_maps_for_process(1))

def time_map_permissions():
    # GIVEN
    for i in range(RANGE):
        map_text = """
7f1ac1e2b000-7f1ac1e50000 r--- 00000000 08:12 8398159                    /usr/lib/libc-2.31.so
7f1ac1e2b000-7f1ac1e50000 rw-- 00000000 08:12 8398159                    /usr/lib/libc-2.31.so
7f1ac1e2b000-7f1ac1e50000 rwx- 00000000 08:12 8398159                    /usr/lib/libc-2.31.so
7f1ac1e2b000-7f1ac1e50000 rwxp 00000000 08:12 8398159                    /usr/lib/libc-2.31.so
        """

        with patch("builtins.open", mock_open(read_data=map_text)):
            maps = list(generate_maps_for_process(1))

def time_unexpected_line_is_ignored():
    # GIVEN
    for i in range(RANGE):
        map_text = """
I am an unexpected line
7f1ac1e2b000-7f1ac1e50000 r--p 00000000 08:12 8398159                    /usr/lib/libc-2.31.so
        """

        # WHEN

        with patch("builtins.open", mock_open(read_data=map_text)):
            maps = list(generate_maps_for_process(1))

def time_special_maps():
    for i in range(RANGE):
        map_text = """
555f1ab1c000-555f1ab3d000 rw-p 00000000 00:00 0                          [heap]
7ffdf8102000-7ffdf8124000 rw-p 00000000 00:00 0                          [stack]
7ffdf8152000-7ffdf8155000 r--p 00000000 00:00 0                          [vvar]
7ffdf8155000-7ffdf8156000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
        """

        # WHEN

        with patch("builtins.open", mock_open(read_data=map_text)):
            maps = list(generate_maps_for_process(1))

def time_maps_for_binary_only_python_exec():
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        maps = [
            python,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_with_heap():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        heap = VirtualMap(
            start=140728765587456,
            end=140728765599744,
            filesize=12288,
            offset=0,
            device="00:00",
            flags="r--p",
            inode=0,
            path=Path("[heap]"),
        )

        maps = [
            python,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
            heap,
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)
        
def time_maps_for_binary_with_libpython():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        libpython = VirtualMap(
            start=140728765587456,
            end=140728765599744,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r--p",
            inode=0,
            path=Path("/some/path/to/libpython.so"),
        )

        maps = [
            python,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
            libpython,
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_executable_with_bss():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        bss = VirtualMap(
            start=139752898736128,
            end=139752898887680,
            filesize=4096,
            offset=0,
            device="08:12",
            flags="r--p",
            inode=8398159,
            path=None,
        )

        maps = [
            python,
            bss,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_libpython_with_bss():
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        bss = VirtualMap(
            start=139752898736128,
            end=139752898887680,
            filesize=4096,
            offset=0,
            device="08:12",
            flags="r--p",
            inode=8398159,
            path=None,
        )

        libpython = VirtualMap(
            start=140728765587456,
            end=140728765599744,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r--p",
            inode=0,
            path=Path("/some/path/to/libpython.so"),
        )

        libpython_bss = VirtualMap(
            start=18446744073699065856,
            end=18446744073699069952,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=None,
        )

        maps = [
            python,
            bss,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
            libpython,
            libpython_bss,
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_libpython_without_bss():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        bss = VirtualMap(
            start=139752898736128,
            end=139752898887680,
            filesize=4096,
            offset=0,
            device="08:12",
            flags="r--p",
            inode=8398159,
            path=None,
        )

        libpython = VirtualMap(
            start=140728765587456,
            end=140728765599744,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r--p",
            inode=0,
            path=Path("/some/path/to/libpython.so"),
        )

        maps = [
            python,
            bss,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
            libpython,
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_libpython_with_bss_with_non_readable_segment():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        bss = VirtualMap(
            start=139752898736128,
            end=139752898887680,
            filesize=4096,
            offset=0,
            device="08:12",
            flags="r--p",
            inode=8398159,
            path=None,
        )

        libpython = VirtualMap(
            start=140728765587456,
            end=140728765599744,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r--p",
            inode=0,
            path=Path("/some/path/to/libpython.so"),
        )

        libpython_bss = VirtualMap(
            start=18446744073699065856,
            end=18446744073699069952,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=None,
        )

        maps = [
            python,
            bss,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
            libpython,
            VirtualMap(
                start=1844674407369906,
                end=18446744073699069,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="---p",
                inode=0,
                path=None,
            ),
            libpython_bss,
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_range():
    # GIVEN
    for i in range(RANGE):
        maps = [
            VirtualMap(
                start=1,
                end=2,
                filesize=1,
                offset=0,
                device="00:00",
                flags="r-xp",
                inode=0,
                path=Path("the_executable"),
            ),
            VirtualMap(
                start=2,
                end=3,
                filesize=1,
                offset=0,
                device="08:12",
                flags="r--p",
                inode=8398159,
                path=None,
            ),
            VirtualMap(
                start=5,
                end=6,
                filesize=1,
                offset=0,
                device="00:00",
                flags="r--p",
                inode=0,
                path=Path("/some/path/to/libpython.so"),
            ),
            VirtualMap(
                start=8,
                end=9,
                filesize=1,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=None,
            ),
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_range_vmaps_are_ignored():
    # GIVEN
    for i in range(RANGE):
        maps = [
            VirtualMap(
                start=1,
                end=2,
                filesize=1,
                offset=0,
                device="00:00",
                flags="r-xp",
                inode=0,
                path=Path("the_executable"),
            ),
            VirtualMap(
                start=2000,
                end=3000,
                filesize=1000,
                offset=0,
                device="08:12",
                flags="r--p",
                inode=8398159,
                path=Path("[vsso]"),
            ),
            VirtualMap(
                start=5,
                end=6,
                filesize=1,
                offset=0,
                device="00:00",
                flags="r--p",
                inode=0,
                path=Path("[vsyscall]"),
            ),
            VirtualMap(
                start=8,
                end=9,
                filesize=1,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("[vvar]"),
            ),
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_no_binary_map():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        maps = [
            python,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
        ]

        # WHEN / THEN

        with pytest.raises(MissingExecutableMaps):
            parse_maps_file_for_binary(Path("another_executable"), maps)

def time_maps_for_binary_no_executable_segment():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r--p",
            inode=0,
            path=Path("the_executable"),
        )

        maps = [
            python,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
        ]

        # WHEN

        mapinfo = parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_multiple_libpythons():
    # GIVEN
    for i in range(RANGE):
        maps = [
            VirtualMap(
                start=140728765599744,
                end=140728765603840,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="r--p",
                inode=0,
                path=Path("the_executable"),
            ),
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libpython3.8.so"),
            ),
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libpython2.7.so"),
            ),
        ]

        # WHEN / THEN

        with pytest.raises(PystackError):
            parse_maps_file_for_binary(Path("the_executable"), maps)

def time_maps_for_binary_invalid_executable():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=Path("the_executable"),
        )

        maps = [
            python,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
        ]

        # WHEN

        with pytest.raises(MissingExecutableMaps, match="the_executable"):
            parse_maps_file_for_binary(Path("other_executable"), maps)
        
def time_maps_for_binary_invalid_executable_and_no_available_maps():
    # GIVEN
    for i in range(RANGE):
        python = VirtualMap(
            start=140728765599744,
            end=140728765603840,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=None,
        )

        maps = [
            python,
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
        ]

        # WHEN

        with pytest.raises(
            MissingExecutableMaps, match="There are no available executable maps"
        ):
            parse_maps_file_for_binary(Path("other_executable"), maps)  

def time_maps_with_scattered_segments():

    for i in range(RANGE):
        map_text = """
00400000-00401000 r-xp 00000000 fd:00 67488961          /bin/python3.9-dbg
00600000-00601000 r--p 00000000 fd:00 67488961          /bin/python3.9-dbg
00601000-00602000 rw-p 00001000 fd:00 67488961          /bin/python3.9-dbg
0067b000-00a58000 rw-p 00000000 00:00 0                 [heap]
7f7b38000000-7f7b38028000 rw-p 00000000 00:00 0
7f7b38028000-7f7b3c000000 ---p 00000000 00:00 0
7f7b40000000-7f7b40021000 rw-p 00000000 00:00 0
7f7b40021000-7f7b44000000 ---p 00000000 00:00 0
7f7b44ec0000-7f7b44f40000 rw-p 00000000 00:00 0
f7b45a61000-7f7b45d93000 rw-p 00000000 00:00 0
7f7b46014000-7f7b46484000 r--p 0050b000 fd:00 1059871   /lib64/libpython3.9d.so.1.0
7f7b46484000-7f7b46485000 ---p 00000000 00:00 0
7f7b46485000-7f7b46cda000 rw-p 00000000 00:00 0
7f7b46cda000-7f7b46d16000 r--p 00a3d000 fd:00 1059871   /lib64/libpython3.9d.so.1.0
7f7b46d16000-7f7b46d6f000 rw-p 00000000 00:00 0
7f7b46d6f000-7f7b46d92000 r--p 00001000 fd:00 67488961  /bin/python3.9-dbg
7f7b46d92000-7f7b46d93000 ---p 00000000 00:00 0
7f7b46d93000-7f7b475d3000 rw-p 00000000 00:00 0
7f7b498c1000-7f7b49928000 r-xp 00000000 fd:00 7023      /lib64/libssl.so.1.0.0
7f7b49928000-7f7b49b28000 ---p 00067000 fd:00 7023      /lib64/libssl.so.1.0.0
f7b4c632000-7f7b4c6f3000 rw-p 00000000 00:00 0
7f7b4c6f3000-7f7b4c711000 rw-p 00000000 00:00 0
7f7b4c711000-7f7b4c712000 r--p 0002a000 fd:00 67488961  /bin/python3.9-dbg
7f7b4c712000-7f7b4c897000 rw-p 00000000 00:00 0
7f7b5a356000-7f7b5a35d000 r--s 00000000 fd:00 201509519 /usr/lib64/gconv/gconv-modules.cache
7f7b5a35d000-7f7b5a827000 r-xp 00000000 fd:00 1059871   /lib64/libpython3.9d.so.1.0
7f7b5a827000-7f7b5aa27000 ---p 004ca000 fd:00 1059871   /lib64/libpython3.9d.so.1.0
7f7b5aa27000-7f7b5aa2c000 r--p 004ca000 fd:00 1059871   /lib64/libpython3.9d.so.1.0
7f7b5aa2c000-7f7b5aa67000 rw-p 004cf000 fd:00 1059871   /lib64/libpython3.9d.so.1.0
7f7b5aa67000-7f7b5aa8b000 rw-p 00000000 00:00 0
7fff26f8e000-7fff27020000 rw-p 00000000 00:00 0         [stack]
7fff27102000-7fff27106000 r--p 00000000 00:00 0         [vvar]
7fff27106000-7fff27108000 r-xp 00000000 00:00 0         [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0 [vsyscall]
        """

        # WHEN

        with patch("builtins.open", mock_open(read_data=map_text)):
            maps = list(generate_maps_for_process(1))

            mapinfo = parse_maps_file_for_binary(Path("/bin/python3.9-dbg"), maps)

def time_get_base_map_path_existing():
    # GIVEN
    for i in range(RANGE):
        maps = [
            VirtualMap(
                start=140728765599744,
                end=140728765603840,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="r-xp",
                inode=0,
                path=None,
            ),
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=Path("/usr/lib/libc-2.31.so"),
            ),
        ]

        # WHEN
        base_map = _get_base_map(maps)  

def time_get_base_map_path_not_existing():
    # GIVEN
    for i in range(RANGE):
        maps = [
            VirtualMap(
                start=140728765599744,
                end=140728765603840,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="r-xp",
                inode=0,
                path=None,
            ),
            VirtualMap(
                start=18446744073699065856,
                end=18446744073699069952,
                filesize=4096,
                offset=0,
                device="00:00",
                flags="--xp",
                inode=0,
                path=None,
            ),
        ]

        # WHEN
        base_map = _get_base_map(maps)

def time_get_bss_base_map_no_path():
    # GIVEN
    for i in range(RANGE):
        map_no_path = VirtualMap(
            start=18446744073699065856,
            end=18446744073699069952,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="--xp",
            inode=0,
            path=None,
        )

        # WHEN
        with patch("pystack.maps._get_base_map", return_value=map_no_path):
            bss = _get_bss("elf_maps", "load_point")

def time_get_bss_no_matching_map():

    for i in range(RANGE):
        libpython = VirtualMap(
            start=140728765587456,
            end=140728765599744,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r--p",
            inode=0,
            path=Path("/some/path/to/libpython.so"),
        )

        libpython_bss = VirtualMap(
            start=18446744073699065856,
            end=18446744073699069952,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=None,
        )
        maps = [libpython, libpython_bss]

        # WHEN
        with patch("pystack._pystack.get_bss_info") as mock_get_bss_info:
            mock_get_bss_info.return_value = {"corrected_addr": 100000000}
            bss = _get_bss(maps, libpython.start)

def time_get_bss_found_matching_map():
    for i in range(RANGE):
        libpython = VirtualMap(
            start=140728765587456,
            end=140728765599744,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r--p",
            inode=0,
            path=Path("/some/path/to/libpython.so"),
        )

        libpython_bss = VirtualMap(
            start=18446744073699065856,
            end=18446744073699069952,
            filesize=4096,
            offset=0,
            device="00:00",
            flags="r-xp",
            inode=0,
            path=None,
        )
        maps = [libpython, libpython_bss]

        # WHEN
        with patch("pystack._pystack.get_bss_info") as mock_get_bss_info:
            mock_get_bss_info.return_value = {
                "corrected_addr": libpython_bss.start - libpython.start,
                "size": libpython_bss.filesize,
            }
            bss = _get_bss(maps, libpython.start)
