FROM dockage_alpine_gcloud_cpp as devtools

RUN apk update && \
    apk add \
        boost-dev \
        boost-static 

# Copy the source code to /v/source and compile it.
FROM devtools AS build

COPY . /v/source
WORKDIR /v/source

# Run the CMake configuration step. NOT static - can't link to the .so files in the base image
RUN cmake -S/v/source -B/v/binary \
    -DCMAKE_BUILD_TYPE=Release \
    -DBoost_USE_STATIC_LIBS=ON 

# Compile the binary and strip it to reduce its size.
RUN cmake --build /v/binary 
RUN strip /v/binary/csv_converter_gcp

# We need Linux to run everything, so can't start from scratch 
FROM build AS csv-converter-gcp
WORKDIR /r

# Copy the program from the previously created stage and make it the entry point.
COPY --from=build /v/binary/csv_converter_gcp /r

ENTRYPOINT [ "/r/csv_converter_gcp" ]


