cmake_minimum_required (VERSION 3.0.0)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if(WIN32)
    if (POLICY CMP0074)
        cmake_policy(SET CMP0074 NEW)
    endif()

    set(HUNTER_PACKAGES PNG)

    include(FetchContent)
    FetchContent_Declare(SetupHunter GIT_REPOSITORY https://github.com/cpp-pm/gate)
    FetchContent_MakeAvailable(SetupHunter)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project (hrpt-decoder)
set(PROJECT_VERSION 1.0)
file(GLOB_RECURSE HRPT_DECODER_CPPS src/*.cpp src/*/*.cpp)
add_executable(hrpt-decoder ${HRPT_DECODER_CPPS})
target_include_directories(hrpt-decoder PUBLIC ${CMAKE_SOURCE_DIR}/src)

find_package (Threads)
target_link_libraries (hrpt-decoder ${CMAKE_THREAD_LIBS_INIT})

if(WIN32)
    find_package(PNG CONFIG REQUIRED)
else()
    find_package(PNG REQUIRED)
endif()
target_link_libraries(hrpt-decoder PNG::PNG)

if(LINUX) 
    set(VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VERSION ${VERSION})
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "hrpt-decoder")
    #set(CPACK_PACKAGE_RELEASE ${GIT_COMMIT_HASH})
    set(CPACK_PACKAGE_CONTACT "Aang23")
    set(CPACK_PACKAGE_VENDOR "Altillimity")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

    install(TARGETS hrpt-decoder DESTINATION bin)

    include(CPack)
endif()
    