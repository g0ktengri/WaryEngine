# Wary Engine

A simple custom 2D Game Engine developed in C++ using OpenGL 3.3.

This project was created to understand the fundamentals of game engine architecture, including rendering, input handling, and entity management (ECS). It uses SDL2 for windowing, GLAD for OpenGL loading, and Dear ImGui for the editor interface.

## Features

- **Rendering**: Custom OpenGL 3.3 renderer with shader support.
- **Entity Component System (ECS)**: Basic implementation for managing game objects.
- **Editor UI**: Integrated Inspector and Scene Hierarchy panels using Dear ImGui.
- **Asset Management**: Dynamic texture loading from the assets folder.
- **Input System**: Keyboard and mouse input handling.
- **Physics**: Basic AABB collision detection and gravity simulation.
- **Camera**: 2D camera with pan and zoom functionality.
- **Particle System**: Simple particle generation.

## Dependencies

- **C++20**
- **OpenGL 3.3**
- **SDL2** (Simple DirectMedia Layer)
- **GLAD** (OpenGL Loader)
- **Dear ImGui** (Immediate Mode GUI)
- **stb_image** (Image loading)

## How to Build

The project uses CMake for building.

1.  Clone the repository.
2.  Create a build directory.
3.  Run CMake:
    ```bash
    cmake ..
    ```
4.  Build the project:
    ```bash
    cmake --build .
    ```
5.  Run the executable from the bin folder.

## Controls

- **WASD**: Move the selected entity.
- **Left Click**: Select an entity in the scene.
- **Middle Mouse**: Pan the camera.
- **Mouse Wheel**: Zoom in and out.
- **Space**: Spawn particles from the selected entity.
