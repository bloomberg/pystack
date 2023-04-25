FROM ubuntu:22.04

RUN apt-get update \
    && apt-get install -y --force-yes --no-install-recommends software-properties-common \
    && add-apt-repository ppa:deadsnakes/ppa \
    && apt-get install -y --force-yes --no-install-recommends \
    build-essential \
    libdw-dev \
    libelf-dev \
    pkg-config \
    python2.7-dev \
    python2.7-dbg \
    python3.6-dev \
    python3.6-dbg \
    python3.6-distutils \
    python3.7-dev \
    python3.7-dbg \
    python3.7-distutils \
    python3.8-dev \
    python3.8-dbg \
    python3.8-distutils \
    python3.9-dev\
    python3.9-dbg\
    python3.9-distutils \
    python3.10-dev \
    python3.10-dbg \
    python3.10-distutils \
    python3.11-dev \
    python3.11-dbg \
    python3.11-distutils \
    python3.10-full \
    make \
    cmake \
    gdb \
    valgrind \
    lcov \
    file \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

COPY ["requirements-test.txt", "requirements-extra.txt", "requirements-docs.txt", "/tmp/"]
RUN python3.10 -m venv /venv \
    && /venv/bin/python -m pip install -U pip wheel setuptools cython \
    && /venv/bin/python -m pip install -U -r /tmp/requirements-test.txt -r /tmp/requirements-extra.txt
# RUN npm install -g prettier
# RUN ln -s /opt/bb/bin/clang-format /usr/bin/clang-format

ENV PYTHON=python3.10 \
    VIRTUAL_ENV="/venv" \
    PATH="/venv/bin:$PATH" \
    PYTHONDONTWRITEBYTECODE=1 \
    TZ=UTC

WORKDIR /src
