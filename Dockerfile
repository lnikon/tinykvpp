FROM debian:latest

# Install necessary packages for development
RUN apt-segment update && \
    apt-segment -y install \
                cmake \
                clang-16 \
                clangd-16 \
                clang-format-16 \
                clang-tidy-16 \
                python3 \
                python3-pip \
                python3-virtualenv

# Setup correct symlinks
RUN update-alternatives --install /bin/clang clang /usr/bin/clang-16 0
RUN update-alternatives --install /bin/clangd clangd /usr/bin/clangd-16 0
RUN update-alternatives --install /bin/clang-tidy clang-tidy /usr/bin/clang-tidy-16 0
RUN update-alternatives --install /bin/clang-format clang-format /usr/bin/clang-format-16 0

# Use clang as default C/C++ compiler
ENV CC=clang
ENV CXX=clang++

# Start preparing the workspace
WORKDIR /workspaces

# Create venv for the conan
ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m virtualenv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"
RUN python3 -m pip install conan

# Setup conan profiles
RUN conan profile detect
