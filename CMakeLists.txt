cmake_minimum_required (VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project (hrpt_decoder)
file(GLOB_RECURSE HRPT_DECODER_CPPS src/*.cpp src/*/*.cpp)
add_executable(hrpt_decoder ${HRPT_DECODER_CPPS})
target_include_directories(hrpt_decoder PUBLIC ${CMAKE_SOURCE_DIR}/src)

find_package (Threads)
target_link_libraries (hrpt_decoder ${CMAKE_THREAD_LIBS_INIT})

find_package(PNG REQUIRED)
target_include_directories(hrpt_decoder PUBLIC ${PNG_INCLUDE_DIR})
target_link_libraries(hrpt_decoder ${PNG_LIBRARY})
