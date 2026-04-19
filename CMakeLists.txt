cmake_minimum_required(VERSION 3.20)
project(project_hibernation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find Packages
find_package(Vulkan REQUIRED)

include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# Volk
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        vulkan-sdk-1.3.283.0
)
FetchContent_MakeAvailable(volk)

# VulkanMemoryAllocator
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
)
FetchContent_MakeAvailable(vma)

# vk-bootstrap
FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.299
)
FetchContent_MakeAvailable(vk-bootstrap)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.7
)
FetchContent_MakeAvailable(imgui)

# ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v1.0
)
FetchContent_MakeAvailable(implot)

# 2. Setup Shader Compiler
find_program(GLSLC_COMPILER glslc REQUIRED)

file(GLOB SHADER_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
)

set(SHADER_SPIRV_FILES "")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders")

foreach(SHADER_FILE IN LISTS SHADER_FILES)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/${SHADER_NAME}.spv.h")
    
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND ${GLSLC_COMPILER} "${SHADER_FILE}" -mfmt=c -o "${OUTPUT_FILE}"
        DEPENDS "${SHADER_FILE}"
        COMMENT "Compiling ${SHADER_NAME} to C-array header..."
    )
    list(APPEND SHADER_SPIRV_FILES "${OUTPUT_FILE}")
endforeach()

# Custom Target for building shaders
add_custom_target(CompileShaders DEPENDS ${SHADER_SPIRV_FILES})


# 3. Automatically collect all cpp/c sources in the source folder
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c"
)

# Collect header files (.h, .hpp)
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
)

# Collect ImGui & ImPlot source files
file(GLOB IMGUI_SRCS "${imgui_SOURCE_DIR}/*.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)
file(GLOB IMPLOT_SRCS "${implot_SOURCE_DIR}/*.cpp")

source_group("ImGui Sources" FILES ${IMGUI_SRCS})
source_group("ImPlot Sources" FILES ${IMPLOT_SRCS})

# Configure Main Executable
add_executable(${PROJECT_NAME} 
    ${SRC_FILES}
    ${HEADER_FILES}
    ${IMGUI_SRCS}
    ${IMPLOT_SRCS}
)

# 4. Apply Precompiled Header (stdafx.h)
target_precompile_headers(${PROJECT_NAME} PRIVATE source/stdafx.h)

# Add Include Directories (Includes CMAKE_CURRENT_BINARY_DIR for generated shaders)
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_BINARY_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
    ${glm_SOURCE_DIR}
    ${volk_SOURCE_DIR}
    ${vma_SOURCE_DIR}/include
)

# 5. Link Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glm::glm
    vk-bootstrap
    volk::volk
)

# Add dependency to ensure shaders are built before the executable
add_dependencies(${PROJECT_NAME} CompileShaders)

