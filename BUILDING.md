# Building the C++ GPU Engine

## macOS

The project uses the system OpenGL and GLUT frameworks on macOS.

```bash
cmake -S . -B build
cmake --build build
./build/gpu_polygon_engine --obj sphere.obj
```

## Windows

Install:

- Visual Studio with the C++ desktop workload
- CMake
- FreeGLUT

The simplest FreeGLUT setup is through vcpkg:

```powershell
vcpkg install freeglut:x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
.\build\Release\gpu_polygon_engine.exe --obj sphere.obj
```

Windows needs FreeGLUT for the window and OpenGL context. The engine loads
the required OpenGL buffer functions from the active context at runtime, so it
does not require GLAD or GLEW.

## Linux

Install OpenGL, GLU, and FreeGLUT development packages, then build normally:

```bash
cmake -S . -B build
cmake --build build
./build/gpu_polygon_engine --obj sphere.obj
```

## Multiplayer

Start the Python snapshot server:

```bash
python3 server.py --host 0.0.0.0 --port 5555
```

Then run one or more C++ clients:

```bash
./build/gpu_polygon_engine --host 127.0.0.1 --port 5555 --name Player1 --color 100,255,150
./build/gpu_polygon_engine --host 127.0.0.1 --port 5555 --name Player2 --color 255,80,80
```

Use `--offline` to run the engine without trying to connect.
