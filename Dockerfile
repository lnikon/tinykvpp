ARG TARGET=gcc
FROM ${TARGET} AS build

ARG COMPILER

# Install necessary packages for development in a single step to reduce layers and leverage caching
RUN apt-get update && \
    apt-get -y install \
    cmake \
    python3 \
    python3-pip \
    python3-virtualenv && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*

# Set up working directory
WORKDIR /workspaces

# Create and activate virtual environment in one RUN command for better layer optimization
ENV VIRTUAL_ENV=/opt/venv
ENV PATH="$VIRTUAL_ENV/bin:$PATH"
RUN python3 -m virtualenv "$VIRTUAL_ENV" && \
    . "$VIRTUAL_ENV/bin/activate" && \
    python3 -m pip install --upgrade pip conan

# Setup Conan profiles and install dependencies
COPY conanfile.txt .
COPY conan conan
RUN echo ${COMPILER}
RUN conan install . --output-folder=build \
    --profile:build=conan/profiles/debug-${COMPILER} \
    --profile:host=conan/profiles/debug-${COMPILER} \
    --build=missing

# Copy project files after dependencies to maximize caching
COPY . .

# Generate and build the project
RUN cp -f ./build/CMakePresets.json . && \
    cmake --preset conan-debug && \
    cmake --build ./build

# Test stage for running tests
FROM build AS test

WORKDIR /workspaces

# Copy binaries directly from the build stage
COPY --from=build /workspaces/build/DBTest build/DBTest
COPY --from=build /workspaces/build/LSMTreeTest build/LSMTreeTest
COPY --from=build /workspaces/build/MemTableTest build/MemTableTest
