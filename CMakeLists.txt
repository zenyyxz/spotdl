cmake_minimum_required(VERSION 3.20)
project(spotdl-reborn VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warnings
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3 -pedantic)
endif()

# Find Threads
find_package(Threads REQUIRED)

# Find CURL
find_package(CURL REQUIRED)

# Find TagLib via PkgConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED taglib)

# Spotdl Core Library
set(SPOTDL_CORE_SOURCES
    src/http.cpp 
    src/spotify.cpp 
    src/lyrics.cpp 
    src/tagger.cpp 
    src/downloader.cpp 
    src/ui.cpp 
)

add_library(spotdl_core STATIC ${SPOTDL_CORE_SOURCES})

target_include_directories(spotdl_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(spotdl_core PUBLIC
    Threads::Threads
    CURL::libcurl
    ${TAGLIB_LIBRARIES}
)

target_compile_definitions(spotdl_core PUBLIC 
    SPOTDL_VERSION="${PROJECT_VERSION}"
)

# CLI Binary Executable
add_executable(spotdl src/main.cpp)
target_link_libraries(spotdl PRIVATE spotdl_core)

# Tests Executable
enable_testing()
add_executable(spotdl_tests tests/test_main.cpp)
target_link_libraries(spotdl_tests PRIVATE spotdl_core)
add_test(NAME SpotdlUnitTests COMMAND spotdl_tests)

# Ouput binaries to bin directory
set_target_properties(spotdl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set_target_properties(spotdl_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
