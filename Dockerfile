# Stage 1: Elfutils build stage
FROM ubuntu:26.04 AS elfutils_builder
ARG DEBIAN_FRONTEND=noninteractive
ENV VERS=0.193

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
        libcurl4-gnutls-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir /elfutils \
    && cd /elfutils \
    && curl https://sourceware.org/elfutils/ftp/$VERS/elfutils-$VERS.tar.bz2 > ./elfutils.tar.bz2 \
    && tar -xf elfutils.tar.bz2 --strip-components 1 \
    && CFLAGS='-Wno-error -g -O3' CXXFLAGS='-Wno-error -g -O3' ./configure --disable-nls --enable-libdebuginfod=dummy --disable-debuginfod --with-zstd \
    && make install

# Stage 2: Final stage
FROM ubuntu:26.04
ARG DEBIAN_FRONTEND=noninteractive
LABEL org.opencontainers.image.source="https://github.com/bloomberg/pystack"

# Install runtime dependencies
RUN apt-get update \
    && apt-get install -y --force-yes --no-install-recommends \
    build-essential \
    pkg-config \
    make \
    cmake \
    gdb \
    git \
    valgrind \
    lcov \
    file \
    less \
    libcrypt-dev \
    libzstd-dev \
    liblzma-dev \
    libbz2-dev \
    zlib1g-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /etc/debuginfod/*.urls

# Copy the installed files from the elfutils_builder stage
COPY --from=elfutils_builder /usr/local /usr/local

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv

# Set environment variables
ENV VIRTUAL_ENV="/venv" \
    PATH="/venv/bin:/root/.local/bin:/usr/local/sbin:/usr/local/bin:/sbin:/bin" \
    PYTHONDONTWRITEBYTECODE=1 \
    TZ=UTC \
    PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

# Install Python interpreters via uv, and install setuptools for each
RUN uv_versions="3.8 3.9 3.10 3.11 3.12 3.13 3.14 3.15" \
    && uv python install --no-config $uv_versions \
    && for ver in $uv_versions; do \
           PATH=/root/.local/bin /usr/local/bin/uv pip install --system --break-system-packages --python=python$ver setuptools; \
       done

# Copy the required files
COPY pyproject.toml /tmp/

# Install Python packages
RUN uv venv $VIRTUAL_ENV \
    && uv pip install pip wheel setuptools cython pkgconfig \
    && uv pip install --group "/tmp/pyproject.toml:test" --group "/tmp/pyproject.toml:extra"

# Set the working directory
WORKDIR /src
