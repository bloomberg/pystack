# Build stage
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
    libdw-dev \
    libelf-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /etc/debuginfod/*.urls

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv

# Set environment variables
ENV VIRTUAL_ENV="/venv" \
    PATH="/venv/bin:/root/.local/bin:/usr/local/sbin:/usr/local/bin:/sbin:/bin" \
    PYTHONDONTWRITEBYTECODE=1 \
    TZ=UTC

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
