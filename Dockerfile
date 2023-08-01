# Stage 1: Elfutils build stage
FROM ubuntu:22.04 AS elfutils_builder
ARG DEBIAN_FRONTEND=noninteractive
ENV VERS=0.189

# Install elfutils build dependencies
RUN apt-get update \
    && apt-get install -y --force-yes --no-install-recommends software-properties-common gpg-agent \
        build-essential \
        libzstd-dev \
        ca-certificates \
        curl \
        lsb-release \
        bzip2 \
        zlib1g-dev \
        zlib1g-dev:native \
        libbz2-dev \
        liblzma-dev \
        gettext \
        po-debconf \
        gawk \
        libc6-dbg \
        flex \
        bison \
        pkg-config \
        libarchive-dev \
        libmicrohttpd-dev \
        libcurl4-gnutls-dev \
        libsqlite3-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir /elfutils \
    && cd /elfutils \
    && curl https://sourceware.org/elfutils/ftp/$VERS/elfutils-$VERS.tar.bz2 > ./elfutils.tar.bz2 \
    && tar -xf elfutils.tar.bz2 \
    && cd elfutils-$VERS \
    && CFLAGS='-w -DFNM_EXTMATCH=0' ./configure --prefix=/usr --disable-nls --disable-libdebuginfod --disable-debuginfod --with-zstd \
    && make install

# Stage 2: Final stage
FROM ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update \
    && apt-get install -y --force-yes --no-install-recommends software-properties-common gpg-agent \
    && add-apt-repository ppa:deadsnakes/ppa \
    && apt-get install -y --force-yes --no-install-recommends \
    build-essential \
    pkg-config \
    python3.7-dev \
    python3.7-dbg \
    python3.7-distutils \
    python3.8-dev \
    python3.8-dbg \
    python3.8-distutils \
    python3.9-dev \
    python3.9-dbg \
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

# Copy the installed files from the elfutils_builder stage
COPY --from=elfutils_builder /usr /usr

# Copy the required files
COPY ["requirements-test.txt", "requirements-extra.txt", "requirements-docs.txt", "/tmp/"]

# Install Python packages
RUN python3.10 -m venv /venv \
    && /venv/bin/python -m pip install -U pip wheel setuptools cython pkgconfig \
    && /venv/bin/python -m pip install -U -r /tmp/requirements-test.txt -r /tmp/requirements-extra.txt

# Set environment variables
ENV PYTHON=python3.10 \
    VIRTUAL_ENV="/venv" \
    PATH="/venv/bin:$PATH" \
    PYTHONDONTWRITEBYTECODE=1 \
    TZ=UTC

# Set the working directory
WORKDIR /src
