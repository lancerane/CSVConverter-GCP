FROM dockage_ubuntu_gcloud_cpp as devtools
ARG DEBIAN_FRONTEND=noninteractive

# Install additional libs
RUN apt-get update && \
    apt-get -y install \
        curl \
        libc-dev \
        tar \
        autoconf \
        libtool \
        nghttp2 \
        libboost-all-dev 

# Copy the source code to /v/source and compile it.
FROM devtools AS build
COPY . /v/source
WORKDIR /v/source

# Run the CMake configuration step. Not static - we created shared libs in the base image, and we can't link against these with a static binary
RUN cmake -S/v/source -B/v/binary \
    -DCMAKE_BUILD_TYPE=Release \
    -DBoost_USE_STATIC_LIBS=ON 

# Compile the binary and strip it to reduce its size.
RUN cmake --build /v/binary 
RUN strip /v/binary/csv_converter_gcp

# Create the final deployment image - not from scratch as we depend on linux
FROM build AS csv-converter-gcp
WORKDIR /r

# Copy the program from the previously created stage and make it the entry point.
COPY --from=build /v/binary/csv_converter_gcp /r

ENTRYPOINT [ "/r/csv_converter_gcp" ]

