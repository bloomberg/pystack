"""Process utility functions.

This module provides utility functions for checking file types
and decompressing gzip files.
"""
import gzip
import pathlib
import tempfile


def is_elf(filename: pathlib.Path) -> bool:
    """Return True if the given file is an ELF file."""
    try:
        elf_header = b"\x7fELF"
        with open(filename, "br") as thefile:
            return thefile.read(4) == elf_header
    except OSError:
        return False


def is_gzip(filename: pathlib.Path) -> bool:
    """Check if the given file is a Gzip file based on the header.

    Args:
        filename: The path to the file to be checked.

    Returns:
        True if the file starts with the Gzip header, False otherwise.
    """
    gzip_header = b"\x1f\x8b"
    with open(filename, "rb") as thefile:
        return thefile.read(2) == gzip_header


def decompress_gzip(
    filename: pathlib.Path, chunk_size: int = 4 * 1024 * 1024
) -> pathlib.Path:
    """Decompress a Gzip file and write the contents to a temporary file.

    Args:
        filename: The path to the gzip file to decompress.
        chunk_size: Size of chunks to read and write at a time; defaults to 4MB.

    Returns:
        The path to the temporary file containing the decompressed data.

    Raises:
        gzip.BadGzipFile: If the file is not a valid gzip file.
    """
    with tempfile.NamedTemporaryFile(delete=False) as temp_file:
        with gzip.open(filename, "rb") as file_handle:
            while True:
                chunk = file_handle.read(chunk_size)
                if not chunk:
                    break
                temp_file.write(chunk)
    return pathlib.Path(temp_file.name)
