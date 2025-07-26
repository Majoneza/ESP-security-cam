| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

# Camera

Camera capture and control for OV7725 and OV7670 build with ESP-IDF.

(See the README.md file in the upper level 'examples' directory for more information about examples.)

## Folder contents

The project entry is one source file in C language [main.c](main/main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both).

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── README.md                               This is the file you are currently reading
├── components
│   ├── camera_capture
│   │   ├── CMakeLists.txt
│   │   ├── camera_capture_i2s.c
│   │   └── include
│   │       └── camera_capture_i2s.h
│   ├── camera_capture_socket
│   │   ├── CMakeLists.txt
│   │   ├── camera_capture_socket.c
│   │   └── include
│   │       └── camera_capture_socket.h
│   ├── camera_configuration
│   │   ├── CMakeLists.txt
│   │   ├── camera_configuration_i2c.c
│   │   └── include
│   │       └── camera_configuration_i2c.h
│   ├── camera_control
│   │   ├── CMakeLists.txt
│   │   ├── camera_control.c
│   │   └── include
│   │       └── camera_control.h
│   ├── camera_register
│   │   ├── CMakeLists.txt
│   │   ├── camera_register.c
│   │   └── include
│   │       └── camera_register.h
│   ├── camera_softap
│   │   ├── CMakeLists.txt
│   │   ├── camera_softap.c
│   │   └── include
│   │       └── camera_softap.h
│   ├── control_socket
│   │   ├── CMakeLists.txt
│   │   ├── control_socket.c
│   │   └── include
│   │       └── control_socket.h
│   ├── device_control
│   │   ├── CMakeLists.txt
│   │   ├── device_control.c
│   │   ├── include
│   │   │   ├── device_control.h
│   │   │   └── message.pb-c.h
│   │   ├── message.pb-c.c
│   │   └── message.proto
│   ├── heartbeat
│   │   ├── CMakeLists.txt
│   │   ├── heartbeat.c
│   │   └── include
│   │       └── heartbeat.h
│   ├── program_utils
│   │   ├── CMakeLists.txt
│   │   ├── include
│   │   │   └── program_utils.h
│   │   └── program_utils.c
│   └── service_discovery
│       ├── CMakeLists.txt
│       ├── include
│       │   └── service_discovery.h
│       └── service_discovery.c
├── main
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   ├── camera_control_ov7725.c
│   ├── camera_control_ov7725.h
│   ├── camera_ov7670.c
│   ├── camera_ov7725.c
│   ├── camera_register_ov7725.c
│   ├── camera_register_ov7725.h
│   └── main.c                              Entry point of the project program
├── pytest_hello_world.py                   Python script used for automated testing
├── sdkconfig
├── sdkconfig.ci
└── sdkconfig.old
```
