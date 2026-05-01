cmake_minimum_required(VERSION 3.20)
project(project_hibernation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: avoid C4819 when system code page (e.g. 949) cannot represent UTF-8 in dependency headers
if(MSVC)
    add_compile_options(/utf-8)
endif()

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

# stb (header-only image loader)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_SHALLOW TRUE
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# 2. Setup Shader Compiler
find_program(GLSLANG_COMPILER glslangValidator REQUIRED)

file(GLOB SHADER_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
)

set(SHADER_SPIRV_FILES "")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/debug")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/release")

set(SHADER_WRAP_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/WrapCompiledShaderHeader.cmake")
file(WRITE "${SHADER_WRAP_SCRIPT}" [=[
if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "WrapCompiledShaderHeader: INPUT_FILE and OUTPUT_FILE required")
endif()
file(READ "${INPUT_FILE}" SHADER_HEADER_BODY)
file(WRITE "${OUTPUT_FILE}" "#pragma once\nnamespace compiled_shaders {\n${SHADER_HEADER_BODY}\n} // namespace compiled_shaders\n")
]=])

foreach(SHADER_FILE IN LISTS SHADER_FILES)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" SHADER_BASE "${SHADER_NAME}")
    set(SHADER_SYMBOL "g_${SHADER_BASE}")
    set(TEMP_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/_tmp_${SHADER_NAME}.debug.glslang.h")
    set(OUT_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/debug/${SHADER_NAME}.h")
    set(TEMP_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/_tmp_${SHADER_NAME}.release.glslang.h")
    set(OUT_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/release/${SHADER_NAME}.h")

    add_custom_command(
        OUTPUT "${OUT_DEBUG}"
        COMMAND ${GLSLANG_COMPILER} -V "${SHADER_FILE}" -gVS --vn "${SHADER_SYMBOL}" -o "${TEMP_DEBUG}"
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${TEMP_DEBUG}" -DOUTPUT_FILE="${OUT_DEBUG}" -P "${SHADER_WRAP_SCRIPT}"
        COMMAND ${CMAKE_COMMAND} -E remove "${TEMP_DEBUG}"
        DEPENDS "${SHADER_FILE}" "${SHADER_WRAP_SCRIPT}"
        COMMENT "Compiling DEBUG ${SHADER_NAME} to header..."
    )

    add_custom_command(
        OUTPUT "${OUT_RELEASE}"
        COMMAND ${GLSLANG_COMPILER} -V "${SHADER_FILE}" -Os --vn "${SHADER_SYMBOL}" -o "${TEMP_RELEASE}"
        COMMAND ${CMAKE_COMMAND} -DINPUT_FILE="${TEMP_RELEASE}" -DOUTPUT_FILE="${OUT_RELEASE}" -P "${SHADER_WRAP_SCRIPT}"
        COMMAND ${CMAKE_COMMAND} -E remove "${TEMP_RELEASE}"
        DEPENDS "${SHADER_FILE}" "${SHADER_WRAP_SCRIPT}"
        COMMENT "Compiling RELEASE ${SHADER_NAME} to header..."
    )

    list(APPEND SHADER_SPIRV_FILES "${OUT_DEBUG}" "${OUT_RELEASE}")
endforeach()

set(SHADER_UNIFIED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/shaders.h")
set(SHADER_UNIFIED_HEADER_CONTENT "#pragma once\n#if defined(NDEBUG)\n")
foreach(SHADER_FILE IN LISTS SHADER_FILES)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    string(APPEND SHADER_UNIFIED_HEADER_CONTENT "#include \"release/${SHADER_NAME}.h\"\n")
endforeach()
string(APPEND SHADER_UNIFIED_HEADER_CONTENT "\n#else\n")
foreach(SHADER_FILE IN LISTS SHADER_FILES)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    string(APPEND SHADER_UNIFIED_HEADER_CONTENT "#include \"debug/${SHADER_NAME}.h\"\n")
endforeach()
string(APPEND SHADER_UNIFIED_HEADER_CONTENT "\n#endif\n")
file(GENERATE OUTPUT "${SHADER_UNIFIED_HEADER}" CONTENT "${SHADER_UNIFIED_HEADER_CONTENT}")

list(APPEND SHADER_SPIRV_FILES "${SHADER_UNIFIED_HEADER}")

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
    ${stb_SOURCE_DIR}
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

