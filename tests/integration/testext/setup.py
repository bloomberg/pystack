import os

try:
    from distutils.core import Extension
    from distutils.core import setup
except ImportError:
    from setuptools import Extension  # type: ignore[no-redef]
    from setuptools import setup  # type: ignore[no-redef]

ROOT = os.path.realpath(os.path.dirname(__file__))

setup(
    name="testext",
    version="0.0",
    ext_modules=[
        Extension(
            "testext",
            language="c++",
            sources=[os.path.join(ROOT, "testext.cpp")],
        ),
    ],
    zip_safe=False,
)
