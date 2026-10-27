"""Load the target library and keep its native frame active for PyStack to sample."""

import ctypes
import os
import subprocess
import sys

target_dir, mapped_dir, library_name, target_symbol = sys.argv[1:5]
subprocess.run(["mount", "--bind", target_dir, mapped_dir], check=True)
library = ctypes.CDLL(os.path.join(mapped_dir, library_name))
getattr(library, target_symbol)()
