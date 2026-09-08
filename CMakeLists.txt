cmake_minimum_required(VERSION 4.3.4)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

include(FetchContent)
project(project_nekomata VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;DebugSanitize" CACHE STRING "" FORCE)

find_program(SCCACHE_PROGRAM sccache)

if(SCCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
        set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
endif()

add_compile_options(
        -fno-exceptions
        -fno-plt
        -fexperimental-library
        -fenable-matrix
        -fmatrix-memory-layout=column-major

        -fno-math-errno
        -fno-trapping-math
        -ffp-contract=fast
        -fwrapv
        $<$<CONFIG:DebugSanitize>:-fsanitize=address,undefined>
        $<$<CONFIG:Release>:-flto=thin>
)
add_link_options(
        -fuse-ld=lld
        $<$<CONFIG:DebugSanitize>:-fsanitize=address,undefined>
        $<$<CONFIG:Release>:-flto=thin>
)

set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------------------------------------------------
#     Dependencies

find_package(VulkanHeaders REQUIRED)
find_package(SDL3 REQUIRED)
find_package(Freetype REQUIRED)
find_package(utf8cpp REQUIRED)

get_target_property(VULKAN_INCLUDE_DIR Vulkan::Headers INTERFACE_INCLUDE_DIRECTORIES)

set(SHADOW_CMAKE_GENERATOR_SET OFF)
if(CMAKE_GENERATOR MATCHES "^Ninja")
        set(SHADOW_CMAKE_GENERATOR "${CMAKE_GENERATOR}")
        set(CMAKE_GENERATOR "Ninja")
        set(SHADOW_CMAKE_GENERATOR_SET ON)
endif()

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 12.2.0
)

set(FMT_MODULE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt)

if(SHADOW_CMAKE_GENERATOR_SET)
        set(CMAKE_GENERATOR "${SHADOW_CMAKE_GENERATOR}")
endif()

FetchContent_Declare(
        vmahpp
        URL      https://github.com/YaaZ/VulkanMemoryAllocator-Hpp/releases/download/v3.3.0+3/VulkanMemoryAllocator-Hpp-3.3.0.tar.gz
        URL_HASH SHA256=8c2a0573babe1f86f3241d12b4c9df40bb944649b7f4239c1c611797f8af6cd5
        SYSTEM
)
set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(vmahpp)

FetchContent_Declare(xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG v0.8.3
        SYSTEM
)
FetchContent_MakeAvailable(xxhash)


FetchContent_Declare(
        simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG        v4.6.1
        SYSTEM
)
FetchContent_MakeAvailable(simdjson)

FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG        v0.9.0
        SYSTEM
)

# Work around fastgltf transiently including simdjson.
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(fastgltf)
set(CMAKE_SKIP_INSTALL_RULES OFF)

# workaround for compiler errors from KTX
set(CMAKE_CXX_STANDARD 20)
FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v5.0.0-rc1
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
        SYSTEM
)

set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ktx)

set(CMAKE_CXX_STANDARD 26)
# Find tcmalloc, prefer tcmalloc_minimal over tcmalloc.

find_library(TCMALLOC_MINIMAL tcmalloc_minimal)
find_library(TCMALLOC tcmalloc)

if(TCMALLOC_MINIMAL)
    set(TCMALLOC_LIB ${TCMALLOC_MINIMAL})
elseif(TCMALLOC)
    set(TCMALLOC_LIB ${TCMALLOC})
else()
    message(FATAL_ERROR "couldn't find tcmalloc")
endif()

# Find libbacktrace

find_path(LIBBACKTRACE_INCLUDE_DIR backtrace.h)
find_library(LIBBACKTRACE_LIBRARY backtrace)

# ----------------------------------------------------------------------------------------------------------------------
#     Vulkan and VMA Modules

add_library(VulkanModule STATIC)
target_compile_definitions(
        VulkanModule PUBLIC
        VK_NO_PROTOTYPES
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_EXCEPTIONS
)
target_link_libraries(VulkanModule PUBLIC Vulkan::Headers)
target_sources(VulkanModule
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${VULKAN_INCLUDE_DIR}
        FILES
        ${VULKAN_INCLUDE_DIR}/vulkan/vulkan.cppm
)
set_target_properties(VulkanModule PROPERTIES
        CXX_SCAN_FOR_MODULES ON
)

# ----------------------------------------------------------------------------------------------------------------------
#     Slang Setup

find_program(SLANGC_EXECUTABLE slangc
        HINTS $ENV{VULKAN_SDK}/bin
        REQUIRED
)

message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")

function(add_slang_shaders TARGET)
        cmake_parse_arguments(SLANG "" "TARGET_DIR" "SOURCES;ENTRIES;STAGES" ${ARGN})

        if(NOT SLANG_TARGET_DIR)
                set(SLANG_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
        endif()

        file(MAKE_DIRECTORY ${SLANG_TARGET_DIR})
        set(OUTPUT_FILES "")

        set(SLANG_FLAGS
                -target spirv
                -profile spirv_1_6
                -O3
                -Wno-39001
                -fvk-use-scalar-layout
        )

        foreach(SHADER ${SLANG_SOURCES})
                get_filename_component(NAME_WE ${SHADER} NAME_WE)
                set(OUTPUT_BINARY ${SLANG_TARGET_DIR}/${NAME_WE}.spv)
                set(OUTPUT_DEPFILE ${OUTPUT_BINARY}.d)
                set(CMD
                        ${SLANGC_EXECUTABLE}
                        ${SHADER}
                        ${SLANG_FLAGS}
                        -o
                        ${OUTPUT_BINARY}
                        -depfile
                        ${OUTPUT_DEPFILE}
                )

                add_custom_command(
                        OUTPUT ${OUTPUT_BINARY}
                        DEPFILE ${OUTPUT_DEPFILE}
                        COMMAND ${CMD}
                        DEPENDS ${SHADER}
                        COMMENT "Compiling Slang shader ${SHADER} --> ${OUTPUT_BINARY}"
                        VERBATIM
                )

                list(APPEND OUTPUT_FILES ${OUTPUT_BINARY})
        endforeach()

        add_custom_target(${TARGET} DEPENDS ${OUTPUT_FILES})
endfunction()

# ----------------------------------------------------------------------------------------------------------------------
#     Sources

file(GLOB_RECURSE project_nekomata_LIBRARY_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/*.cpp
)
file(GLOB_RECURSE project_nekomata_LIBRARY_MODULE_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/*.ixx
)

list(FILTER project_nekomata_LIBRARY_SOURCES EXCLUDE REGEX ".*/(bin)/.*")
list(FILTER project_nekomata_LIBRARY_MODULE_SOURCES EXCLUDE REGEX ".*/(bin)/.*")

file(GLOB_RECURSE project_nekomata_SHADER_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/graphics/shaders/bin/*.slang
)

file(GLOB_RECURSE project_nekomata_BINARIES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/bin/*.cpp
)

# ----------------------------------------------------------------------------------------------------------------------
#     Build Setup

add_library(project_nekomata_Library STATIC)
target_sources(project_nekomata_Library
        PRIVATE ${project_nekomata_LIBRARY_SOURCES}
        PUBLIC
        FILE_SET CXX_MODULES FILES
        ${project_nekomata_LIBRARY_MODULE_SOURCES}
)
target_include_directories(project_nekomata_Library PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(
        project_nekomata_Library
        PRIVATE
        ${TCMALLOC_LIB}
        VulkanModule
        VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
        Freetype::Freetype
        SDL3::SDL3
        ktx
        fastgltf::fastgltf
        xxhash
        utf8::cpp
        fmt::fmt-module
)
if (LIBBACKTRACE_INCLUDE_DIR AND LIBBACKTRACE_LIBRARY)
        target_include_directories(project_nekomata_Library PRIVATE ${LIBBACKTRACE_INCLUDE_DIR})
        target_link_libraries(project_nekomata_Library PRIVATE ${LIBBACKTRACE_LIBRARY})

        target_compile_definitions(project_nekomata_Library PRIVATE PROJNEKOMATA_USE_LIBBACKTRACE)
endif()
target_compile_options(
        project_nekomata_Library PRIVATE
        -ftls-model=local-exec
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -fno-semantic-interposition
        -ftime-trace

        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wdouble-promotion
        -Wimplicit-fallthrough
        -Wmisleading-indentation
        -Werror=return-type
        -Werror=non-virtual-dtor
        -Werror=uninitialized
        -Werror=conditional-uninitialized
        -Werror=init-self
        -Werror=array-bounds
        -Werror=sequence-point
        -Werror=delete-non-virtual-dtor
        -Werror=infinite-recursion
        -Werror=overloaded-virtual

        -Werror=tautological-compare
)
add_slang_shaders(project_nekomata_spv TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/spirv SOURCES ${project_nekomata_SHADER_SOURCES})
add_dependencies(project_nekomata_Library project_nekomata_spv)


foreach(BINARY_ENTRY ${project_nekomata_BINARIES})
        get_filename_component(BINARY_ENTRY_NAME ${BINARY_ENTRY} NAME_WE)
        add_executable(${BINARY_ENTRY_NAME} ${BINARY_ENTRY})
        target_link_libraries(${BINARY_ENTRY_NAME} PRIVATE project_nekomata_Library fmt::fmt-module)

        target_compile_options(
                ${BINARY_ENTRY_NAME} PRIVATE
                -ftls-model=local-exec
                -fvisibility=hidden
                -fvisibility-inlines-hidden
                -fno-semantic-interposition

                -Wall
                -Wextra
                -Wpedantic
                -Wshadow
                -Wdouble-promotion
                -Wimplicit-fallthrough
                -Wmisleading-indentation
                -Werror=return-type
                -Werror=non-virtual-dtor
                -Werror=uninitialized
                -Werror=conditional-uninitialized
                -Werror=init-self
                -Werror=array-bounds
                -Werror=sequence-point
                -Werror=delete-non-virtual-dtor
                -Werror=infinite-recursion
                -Werror=overloaded-virtual

                -Werror=tautological-compare
        )

        set(MANIFEST_FILE "$<TARGET_FILE_DIR:${BINARY_ENTRY_NAME}>/${BINARY_ENTRY_NAME}.manifest")

        add_custom_command(
                TARGET ${BINARY_ENTRY_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "// This file is automatically @generated, edit the relevant section in CMakeLists.txt instead during development." > "${MANIFEST_FILE}"
                COMMAND ${CMAKE_COMMAND} -E echo "" >> "${MANIFEST_FILE}"
                COMMAND ${CMAKE_COMMAND} -E echo "manifest {" >> "${MANIFEST_FILE}"
                COMMAND ${CMAKE_COMMAND} -E echo "    assetspath \"${CMAKE_CURRENT_SOURCE_DIR}/Assets/\"" >> "${MANIFEST_FILE}"
                COMMAND ${CMAKE_COMMAND} -E echo "    spirvpath \"${CMAKE_CURRENT_BINARY_DIR}/spirv/\"" >> "${MANIFEST_FILE}"
                COMMAND ${CMAKE_COMMAND} -E echo "}" >> "${MANIFEST_FILE}"
                VERBATIM
        )
endforeach()