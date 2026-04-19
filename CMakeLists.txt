cmake_minimum_required(VERSION 3.20)
project(ZybNsBench C CXX)

# RocksDB's current CMake build expects a C++20-capable consumer toolchain.
# If the parent project pins a lower standard before add_subdirectory(),
# RocksDB inherits it and fails with a large wave of std::optional/variant
# and defaulted comparison errors.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/masstree_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h
    @ONLY
)

set(NSBENCH_HAVE_ROCKSDB_FLAG 0)

set(NSBENCH_COMMON_SOURCES
    src/nsbench/path_utils.cpp
    src/nsbench/dataset_format.cpp
    src/nsbench/dataset_builder.cpp
    src/nsbench/bench_runner.cpp
    src/nsbench/csv_writer.cpp
)

add_library(nsbench_common STATIC ${NSBENCH_COMMON_SOURCES})
target_include_directories(nsbench_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(NSBENCH_MASSTREE_SOURCES
    src/nsbench/masstree_runtime.cpp
    src/nsbench/masstree_resolver.cpp
    MasstreeLHM/clp.c
    MasstreeLHM/compiler.cc
    MasstreeLHM/kvthread.cc
    MasstreeLHM/memdebug.cc
    MasstreeLHM/misc.cc
    MasstreeLHM/str.cc
    MasstreeLHM/straccum.cc
    MasstreeLHM/string.cc
    MasstreeLHM/string_slice.cc
)

set_source_files_properties(MasstreeLHM/clp.c PROPERTIES LANGUAGE CXX)

set(ROCKSDB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rocksdb)
if(EXISTS ${ROCKSDB_ROOT}/CMakeLists.txt)
    set(ROCKSDB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ROCKSDB_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ROCKSDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ROCKSDB_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(ROCKSDB_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${ROCKSDB_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/third_party/rocksdb EXCLUDE_FROM_ALL)

    set(ROCKSDB_TARGET "")
    if(TARGET rocksdb)
        set(ROCKSDB_TARGET rocksdb)
    elseif(TARGET rocksdb-static)
        set(ROCKSDB_TARGET rocksdb-static)
    endif()

    if(NOT ROCKSDB_TARGET STREQUAL "")
        add_library(nsbench_rocks STATIC
            src/nsbench/rocks_schema.cpp
            src/nsbench/rocks_iterative_resolver.cpp
        )
        target_include_directories(nsbench_rocks
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${ROCKSDB_ROOT}/include
        )
        target_link_libraries(nsbench_rocks PUBLIC ${ROCKSDB_TARGET})
        set(NSBENCH_HAVE_ROCKSDB_FLAG 1)
    endif()
endif()

add_executable(nsbench_build_dataset apps/nsbench_build_dataset.cpp)
target_link_libraries(nsbench_build_dataset PRIVATE nsbench_common)

set(FLATBENCH_SOURCES
    src/flatbench/stats.cpp
    src/flatbench/flat_tree.cpp
    src/flatbench/flatlike_backend.cpp
    src/flatbench/workload.cpp
)

add_library(flatbench STATIC ${FLATBENCH_SOURCES})
target_include_directories(flatbench
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(flatbench PUBLIC nsbench_common)

add_executable(flatbench_prefix apps/flatbench_prefix.cpp)
target_link_libraries(flatbench_prefix PRIVATE flatbench)

add_executable(flatbench_churn apps/flatbench_churn.cpp)
target_link_libraries(flatbench_churn PRIVATE flatbench)

add_executable(flatbench_rename apps/flatbench_rename.cpp)
target_link_libraries(flatbench_rename PRIVATE flatbench)

if(NOT WIN32)
    add_library(nsbench_masstree STATIC ${NSBENCH_MASSTREE_SOURCES})
    target_include_directories(nsbench_masstree
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/MasstreeLHM
            ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    target_compile_options(nsbench_masstree PRIVATE -include config.h)
    target_compile_definitions(nsbench_masstree PUBLIC NSBENCH_HAVE_ROCKSDB=${NSBENCH_HAVE_ROCKSDB_FLAG})
    target_link_libraries(nsbench_masstree PUBLIC nsbench_common)

    add_executable(nsbench_run apps/nsbench_run.cpp)
    target_link_libraries(nsbench_run PRIVATE nsbench_common nsbench_masstree)
    target_compile_options(nsbench_run PRIVATE -include config.h)
    target_compile_definitions(nsbench_run PRIVATE NSBENCH_HAVE_ROCKSDB=${NSBENCH_HAVE_ROCKSDB_FLAG})
    if(TARGET nsbench_rocks)
        target_link_libraries(nsbench_run PRIVATE nsbench_rocks)
    endif()
endif()
