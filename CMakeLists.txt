cmake_minimum_required(VERSION 3.19)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(PDI_LI CXX)

find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)

set(DIRS "libbmp/CPP/")
set(HEADERS "libbmp/CPP/libbmp.h")
set(SRCS "libbmp/CPP/libbmp.cpp")

add_executable(main "src/main.cpp" ${SRCS} ${HEADERS})

target_link_directories(main PRIVATE "." ${DIRS})
target_include_directories(main PRIVATE "." ${DIRS})
target_link_libraries(main
  PRIVATE
    fmt::fmt
    cxxopts::cxxopts)