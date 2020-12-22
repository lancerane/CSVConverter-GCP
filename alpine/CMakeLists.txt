cmake_minimum_required(VERSION 3.5)

# Define the project name and where to report bugs.
set(PACKAGE_BUGREPORT "https://github.com/GoogleCloudPlatform/cpp-samples/issues")
project(Docker-Run-C++ CXX C)

# Configure the Compiler options, we will be using C++17 features.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.66 REQUIRED COMPONENTS program_options filesystem)
find_package(Threads)
find_package(storage_client REQUIRED)



# When using static libraries the FindgRPC.cmake module does not define the
# correct dependencies (OpenSSL::Crypto, c-cares, etc) for gRPC::grpc.
# Explicitly listing these dependencies avoids the undefined symbols problems.
add_executable(csv_converter_gcp csv_converter_gcp.cc)

target_link_libraries(csv_converter_gcp PRIVATE Boost::headers
                                              Boost::program_options
                                              Boost::filesystem 
    Threads::Threads
    storage_client)

include(GNUInstallDirs)
install(TARGETS csv_converter_gcp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})