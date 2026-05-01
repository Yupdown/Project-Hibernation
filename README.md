# Project Hibernation

A Vulkan implementation of [Project-Isometric](https://github.com/Yupdown/Project-Isometric), an isometric 3rd adventure game demo.

## Prerequisites

Ensure your development environment meets the following requirements before building the project:

* **CMake:** Version 3.20 or newer.
* **C++ Compiler:** A compiler with full **C++20** support (e.g., MSVC for Visual Studio 2022, GCC 10+, Clang 10+).
* **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home):** Must be installed on your system.
  * **`glslangValidator`:** The Vulkan SDK ships this SPIR-V compiler and validator; it must be available on your `PATH`. The build invokes it to compile `.vert`, `.frag`, and `.comp` shaders into C-array headers automatically.

## Dependencies

The project uses CMake's `FetchContent` to automatically download and build the following dependencies. You **do not** need to install these manually:

* [GLFW](https://github.com/glfw/glfw) (v3.4) - Windowing and input handling.
* [GLM](https://github.com/g-truc/glm) (v1.0.1) - OpenGL Mathematics.
* [Volk](https://github.com/zeux/volk) (vulkan-sdk-1.3.283.0) - Vulkan meta-loader.
* [VulkanMemoryAllocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) (v3.1.0) - Vulkan memory management.
* [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) (v1.3.299) - Vulkan initialization utility.
* [ImGui](https://github.com/ocornut/imgui) (v1.92.7) - Immediate mode graphical user interface.
* [ImPlot](https://github.com/epezent/implot) (v1.0) - Advanced plotting utility for ImGui.

## Build Instructions

1. **Clone the repository:**
   ```bash
   git clone https://github.com/Yupdown/Project-Hibernation.git
   cd project-hibernation
   ```

2. **Generate the build files:**
   ```bash
   cmake -B build
   ```

3. **Build the project:**
   ```bash
   # Build in Release mode (or Debug, MinSizeRel, etc.)
   cmake --build build --config Release
   ```

## Running the Application

After a successful build, the executable will be located inside your build directory. 

Because the build pipeline compiles shaders (`.vert`, `.frag`, `.comp`) into C-array headers and embeds them directly into the binary, the `.exe` file is self-contained. You do not need to manually move or copy shader files to run the application.

```bash
# Example for Windows
.\build\Release\project_hibernation.exe
```