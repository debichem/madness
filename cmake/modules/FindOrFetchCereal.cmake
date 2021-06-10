if (NOT TARGET cereal)
    find_package(cereal QUIET CONFIG)

    if (TARGET cereal)
        message(STATUS "Found Cereal: cereal_CONFIG=${cereal_CONFIG}")
        target_compile_definitions(cereal INTERFACE
                "CEREAL_THREAD_SAFE=1")
    else (TARGET cereal)
        cmake_minimum_required (VERSION 3.14.0)  # for FetchContent_MakeAvailable
        include(FetchContent)
        FetchContent_Declare(
                cereal
                GIT_REPOSITORY https://github.com/USCiLab/cereal.git
                GIT_TAG v1.3.0)

        # configure cereal
        set(JUST_INSTALL_CEREAL ON CACHE BOOL "")
        set(THREAD_SAFE ON CACHE BOOL "")

        FetchContent_MakeAvailable(cereal)

        # set cereal_CONFIG to the install location so that we know where to find it
        set(cereal_CONFIG ${CMAKE_INSTALL_PREFIX}/share/cmake/cereal/cereal-config.cmake)

        export(EXPORT cereal
                FILE "${PROJECT_BINARY_DIR}/cereal-targets.cmake")

    endif (TARGET cereal)
endif (NOT TARGET cereal)

if (TARGET cereal)
    set(MADNESS_HAS_CEREAL ON CACHE BOOL "MADNESS has access to Cereal")
else (TARGET cereal)
    message(FATAL_ERROR "MADNESS_ENABLE_CEREAL=ON but could not find or fetch Cereal")
endif (TARGET cereal)