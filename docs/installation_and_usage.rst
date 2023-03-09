Installation and usage
**********************


Installing pystack
==================

``pystack`` can be installed as a Python package using ``pip`` as well as a
``DPKG``.

DPKG
----

To install ``pystack`` from ``DPKG`` you need to install the ``pystack`` package. For instance,
to install ``pystack`` in a ``refroot`` using ``dpkg-distro-dev``::

    dpkg-distro-dev install pystack

Note that normally ``DPKG`` packages are installed in machines by adding the
package to the cluster manifest. Check the `DPKG users guide
<https://tutti.prod.bloomberg.com/dpkg/tutorials/package_users_guide>`_ for more
information.

PYPI
----

To install ``pystack`` from ``Bloomberg-PyPI`` you need to install the ``pystack`` package: ::

    python3.8 -m pip install pystack

.. note::
    Notice that ``pystack`` can analyze programs and core files from any Python
    version so there is no need to install different versions of ``pystack`` for
    different versions of Python. Binary wheels for different interpreter versions
    are provided only for convenience, but all wheels are equivalent and have the
    same capabilities and features.

To learn how to setup `pip` correctly to work with ``Bloomberg-PyPI`` you can
check the `Pypi overview guide
<https://tutti.dev.bloomberg.com/python-docs/src/pypi-overview>`_.

Using pystack
=============

To use ``pystack`` with processes, the PID of the process needs to be provided: ::

    $ pystack remote PID

To work correctly on live processes, you need to have the same permissions as are needed to send signals to the process that you want to analyze. To learn more about how to effectively analyze live processes check the :ref:`analyzing-processes` section.

To use ``pystack`` with core dumps, the location of the core dump needs to be provided ::

    $ pystack core  ./path/to/the/core/file

To learn more about how to effectively analyze core dumps check the :ref:`analyzing-core-dumps` section.


Docker
------

To correctly use ``pystack`` in Docker, the container must run with special permissions, similarly as the ones that are needed for ``gdb`` or other debuggers to run in Docker. You can grant the required permissions to your Docker containers by starting them using the ``--cap-add=SYS_PTRACE`` or ``--privileged`` command line arguments. For example: ::

    docker run -it --rm --cap-add=SYS_PTRACE $CONTAINER_NAME bash
