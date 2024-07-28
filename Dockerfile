ARG TARGET=gcc:latest
FROM ${TARGET} as build

# Install necessary packages for development
RUN apt-get update && \
    apt-get -y install \
    cmake \
    python3 \
    python3-pip \
    python3-virtualenv

# Start preparing the workspace
WORKDIR /workspaces

# Create venv for the conan
ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m virtualenv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"
RUN python3 -m pip install --upgrade pip
RUN python3 -m pip install conan

# Setup conan profiles
COPY conanfile.txt conanfile.txt
RUN conan profile detect

# Install and/or build dependencies
RUN conan install . --output-folder=build --build=missing
RUN cp -f ./build/CMakePresets.json .

# Copy project files
COPY CMakeLists.txt CMakeLists.txt
COPY lib lib
COPY src src
COPY app app
COPY grpcapp grpcapp

# Generate the project
RUN cmake --preset conan-release

# Build the project
RUN cmake --build ./build

# Run tests
FROM build as test
WORKDIR /workspaces
COPY --from=build /workspaces/build/DBTest build/DBTest
COPY --from=build /workspaces/build/LSMTreeTest build/LSMTreeTest
COPY --from=build /workspaces/build/MemTableTest build/MemTableTest
COPY run_tests.sh run_tests.sh
RUN /workspaces/run_tests.sh