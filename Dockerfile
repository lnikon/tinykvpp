FROM gcc:14.1

# Install necessary packages for development
RUN apt-get update && \
    apt-get -y install \
    cmake \
    python3 \
    python3-pip \
    python3-virtualenv

# Start preparing the workspace
WORKDIR /workspaces
COPY . .

# Create venv for the conan
ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m virtualenv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"
RUN python3 -m pip install conan

# Setup conan profiles
RUN conan profile detect

# Install and/or build dependencies
RUN conan install . --output-folder=build --build=missing --profile=default

# Generate the project
RUN cmake --preset conan-release

# Build the project
RUN cmake --build ./build
