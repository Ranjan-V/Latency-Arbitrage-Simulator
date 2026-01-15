#!/usr/bin/env python3
"""
Latency Arbitrage Simulator - Project Structure Setup
Generates complete folder structure and starter files
"""

import os
import json
from pathlib import Path

# Project root
PROJECT_ROOT = Path("D:/Projects/Latency-Arb-Simulator")

# Directory structure
DIRECTORIES = [
    "src",
    "include",
    "data",
    "shaders",
    "build",
    "docs",
    "tests"
]

# Exchange data (23 major global exchanges with real GPS coordinates)
EXCHANGE_DATA = {
    "exchanges": [
        {"id": "NYSE", "name": "New York Stock Exchange", "lat": 40.7069, "lon": -74.0113, "city": "New York", "type": "equity"},
        {"id": "NASDAQ", "name": "NASDAQ", "lat": 40.7489, "lon": -73.9680, "city": "New York", "type": "equity"},
        {"id": "CME", "name": "Chicago Mercantile Exchange", "lat": 41.8781, "lon": -87.6298, "city": "Chicago", "type": "derivatives"},
        {"id": "LSE", "name": "London Stock Exchange", "lat": 51.5149, "lon": -0.0909, "city": "London", "type": "equity"},
        {"id": "JPX", "name": "Japan Exchange Group", "lat": 35.6762, "lon": 139.6503, "city": "Tokyo", "type": "equity"},
        {"id": "SSE", "name": "Shanghai Stock Exchange", "lat": 31.2304, "lon": 121.4737, "city": "Shanghai", "type": "equity"},
        {"id": "HKEX", "name": "Hong Kong Stock Exchange", "lat": 22.3193, "lon": 114.1694, "city": "Hong Kong", "type": "equity"},
        {"id": "EUREX", "name": "Eurex", "lat": 50.1109, "lon": 8.6821, "city": "Frankfurt", "type": "derivatives"},
        {"id": "TMX", "name": "TMX Group (Toronto)", "lat": 43.6532, "lon": -79.3832, "city": "Toronto", "type": "equity"},
        {"id": "BSE", "name": "Bombay Stock Exchange", "lat": 18.9292, "lon": 72.8333, "city": "Mumbai", "type": "equity"},
        {"id": "KRX", "name": "Korea Exchange", "lat": 37.5665, "lon": 126.9780, "city": "Seoul", "type": "equity"},
        {"id": "ASX", "name": "Australian Securities Exchange", "lat": -33.8688, "lon": 151.2093, "city": "Sydney", "type": "equity"},
        {"id": "BMV", "name": "Mexican Stock Exchange", "lat": 19.4326, "lon": -99.1332, "city": "Mexico City", "type": "equity"},
        {"id": "B3", "name": "B3 (Brazil)", "lat": -23.5505, "lon": -46.6333, "city": "São Paulo", "type": "equity"},
        {"id": "SIX", "name": "SIX Swiss Exchange", "lat": 47.3769, "lon": 8.5417, "city": "Zurich", "type": "equity"},
        {"id": "BINANCE", "name": "Binance (Virtual)", "lat": 1.3521, "lon": 103.8198, "city": "Singapore", "type": "crypto"},
        {"id": "COINBASE", "name": "Coinbase", "lat": 37.7749, "lon": -122.4194, "city": "San Francisco", "type": "crypto"},
        {"id": "KRAKEN", "name": "Kraken", "lat": 37.7749, "lon": -122.4194, "city": "San Francisco", "type": "crypto"},
        {"id": "BITFINEX", "name": "Bitfinex", "lat": 51.5074, "lon": -0.1278, "city": "London", "type": "crypto"},
        {"id": "HUOBI", "name": "Huobi", "lat": 1.3521, "lon": 103.8198, "city": "Singapore", "type": "crypto"},
        {"id": "FTX", "name": "FTX (Historical)", "lat": 25.7617, "lon": -80.1918, "city": "Miami", "type": "crypto"},
        {"id": "GEMINI", "name": "Gemini", "lat": 40.7128, "lon": -74.0060, "city": "New York", "type": "crypto"},
        {"id": "BYBIT", "name": "Bybit", "lat": 22.3193, "lon": 114.1694, "city": "Hong Kong", "type": "crypto"}
    ]
}

# CMakeLists.txt template
CMAKE_TEMPLATE = """cmake_minimum_required(VERSION 3.15)
project(LatencyArbSimulator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find vcpkg packages
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Boost REQUIRED COMPONENTS graph)
find_package(nlohmann_json CONFIG REQUIRED)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glm::glm
    imgui::imgui
    OpenGL::GL
    Boost::graph
    nlohmann_json::nlohmann_json
)

# Copy data and shaders to build directory
file(COPY ${CMAKE_SOURCE_DIR}/data DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})

# Set working directory for Visual Studio
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
"""

# Main.cpp starter template
MAIN_CPP_TEMPLATE = """#include <iostream>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

int main() {
    std::cout << "Latency Arbitrage Simulator - Initializing..." << std::endl;
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Latency Arbitrage Simulator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    std::cout << "Setup complete! Window created." << std::endl;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Simple UI test
        ImGui::Begin("Latency Arbitrage Simulator");
        ImGui::Text("Application running!");
        ImGui::Text("FPS: %.1f", io.Framerate);
        if (ImGui::Button("Click Me!")) {
            std::cout << "Button clicked!" << std::endl;
        }
        ImGui::End();
        
        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
"""

# README template
README_TEMPLATE = """# Latency Arbitrage Simulator

Real-world latency arbitrage modeling across global cryptocurrency and traditional exchanges.

## Features
- 23+ global exchanges with real GPS coordinates
- Speed-of-light latency calculations
- Network topology optimization
- Interactive 3D globe visualization
- Arbitrage opportunity scanner

## Build Instructions

### Prerequisites
- Visual Studio Build Tools 2022
- CMake 3.15+
- vcpkg

### Building
```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Running
```bash
cd build/Release
./LatencyArbSimulator.exe
```

## Project Structure
```
latency-arb-simulator/
├── src/           # Source files
├── include/       # Header files
├── data/          # Exchange data (JSON)
├── shaders/       # OpenGL shaders
└── build/         # Build output
```

## Tech Stack
- C++17
- OpenGL 3.3+
- Dear ImGui
- GLFW
- GLM
- Boost.Graph

## License
MIT
"""

# VS Code settings
VSCODE_SETTINGS = {
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "D:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    },
    "files.associations": {
        "*.h": "cpp",
        "*.cpp": "cpp"
    }
}

def create_project_structure():
    """Create all directories and files"""
    print("🚀 Creating Latency Arbitrage Simulator Project Structure...\n")
    
    # Create root directory
    PROJECT_ROOT.mkdir(parents=True, exist_ok=True)
    os.chdir(PROJECT_ROOT)
    
    # Create directories
    print("📁 Creating directories...")
    for directory in DIRECTORIES:
        path = PROJECT_ROOT / directory
        path.mkdir(exist_ok=True)
        print(f"   ✓ {directory}/")
    
    # Create .vscode directory and settings
    vscode_dir = PROJECT_ROOT / ".vscode"
    vscode_dir.mkdir(exist_ok=True)
    with open(vscode_dir / "settings.json", "w") as f:
        json.dump(VSCODE_SETTINGS, f, indent=4)
    print("   ✓ .vscode/")
    
    # Write CMakeLists.txt
    print("\n📝 Creating CMakeLists.txt...")
    with open(PROJECT_ROOT / "CMakeLists.txt", "w") as f:
        f.write(CMAKE_TEMPLATE)
    print("   ✓ CMakeLists.txt")
    
    # Write main.cpp
    print("\n📝 Creating main.cpp...")
    with open(PROJECT_ROOT / "src" / "main.cpp", "w") as f:
        f.write(MAIN_CPP_TEMPLATE)
    print("   ✓ src/main.cpp")
    
    # Write exchanges.json
    print("\n📝 Creating exchanges.json...")
    with open(PROJECT_ROOT / "data" / "exchanges.json", "w") as f:
        json.dump(EXCHANGE_DATA, f, indent=4)
    print("   ✓ data/exchanges.json")
    
    # Write README.md
    print("\n📝 Creating README.md...")
    with open(PROJECT_ROOT / "README.md", "w", encoding="utf-8") as f:
        f.write(README_TEMPLATE)
    print("   ✓ README.md")
    
    # Create placeholder shader files
    print("\n📝 Creating shader placeholders...")
    with open(PROJECT_ROOT / "shaders" / "globe_vertex.glsl", "w") as f:
        f.write("// Vertex shader - to be implemented\n")
    with open(PROJECT_ROOT / "shaders" / "globe_fragment.glsl", "w") as f:
        f.write("// Fragment shader - to be implemented\n")
    print("   ✓ shaders/globe_vertex.glsl")
    print("   ✓ shaders/globe_fragment.glsl")
    
    # Create .gitignore
    print("\n📝 Creating .gitignore...")
    with open(PROJECT_ROOT / ".gitignore", "w") as f:
        f.write("build/\n*.exe\n*.obj\n*.pdb\n.vs/\n.vscode/\n")
    print("   ✓ .gitignore")
    
    print("\n✅ Project structure created successfully!")
    print(f"\n📍 Project location: {PROJECT_ROOT}")
    print("\n🎯 Next steps:")
    print("   1. Install Visual Studio Build Tools 2022")
    print("   2. Install vcpkg and libraries")
    print("   3. Open VS Code in this folder")
    print("   4. Run: cmake --configure")
    print("   5. Run: cmake --build .")

if __name__ == "__main__":
    create_project_structure()