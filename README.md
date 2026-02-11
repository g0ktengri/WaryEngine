# Wary Engine

A lightweight 2D Game Engine developed in C++ using SDL2, OpenGL 3.3, and Dear ImGui.

This project serves as a foundational exploration into game engine architecture, focusing on ECS (Entity Component System), custom rendering pipelines, and editor tool development.

## key Features

### Core Systems
-   **Entity Component System (ECS)**: Flexible management of game objects with properties like Transform, Physics, and Rendering.
-   **OpenGL 3.3 Renderer**: Custom rendering engine supporting shaders, textures, and batching (planned).
-   **Input System**: Robust handling of Keyboard and Mouse inputs.

### Editor Tools
-   **Scene Editor**: Real-time scene manipulation with a comprehensive UI.
-   **Inspector Panel**: Modify entity properties (Position, Rotation, Scale, Color, Texture) dynamically.
-   **Scene Hierarchy**: View and select active entities in the scene.
-   **Infinite Grid System**: A visual guide for precise object placement with axis highlights.
-   **Undo System**: Revert accidental changes effortlessly with `Ctrl + Z`.
-   **Non-Uniform Scaling**: Independent control over Entity Width and Height for versatile shapes.
-   **Asset Management**: Automatic loading of textures from the `assets` folder.

### Physics & Gameplay
-   **Basic Physics**: Gravity simulation and AABB (Axis-Aligned Bounding Box) collision detection.
-   **Particle System**: Built-in particle generator for visual effects.
-   **Camera Control**: Pan and Zoom capabilities to navigate the 2D world.

## Dependencies

The project relies on the following libraries (included or easily obtainable):

-   **C++20** Standard
-   **OpenGL 3.3**
-   **SDL2** (Windowing & Input)
-   **GLAD** (OpenGL Loader)
-   **Dear ImGui** (Editor UI)
-   **stb_image** (Image Loading)
-   **GLM** (Mathematics - Optional/Integrated)

## How to Build

The project utilizes **CMake** for cross-platform build configuration.

1.  Clone the repository:
    ```bash
    git clone https://github.com/YourUsername/WaryEngine.git
    cd WaryEngine
    ```
2.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
3.  Generate project files:
    ```bash
    cmake ..
    ```
4.  Build the project:
    ```bash
    cmake --build .
    ```
5.  Run the executable from the `bin` or output directory.

## Controls

### Editor
-   **Left Click**: Select Entity
-   **Middle Mouse Drag**: Pan Camera
-   **Mouse Wheel**: Zoom In/Out
-   **Ctrl + Z**: Undo last action

### Entity Manipulation
-   **Inspector Panel**: Adjust Position, Scale (Width/Height), Rotation, Color, and Physics properties.

### Gameplay / Debug
-   **WASD**: Move selected entity (Debug)
-   **Space**: Spawn Particle Effect on selected entity
