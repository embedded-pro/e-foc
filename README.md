[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=embedded-pro_e-foc&metric=alert_status&token=2d1b7ae361d044a96ba29c5afcbdb009cac319d2)](https://sonarcloud.io/summary/new_code?id=embedded-pro_e-foc)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=embedded-pro_e-foc&metric=coverage&token=2d1b7ae361d044a96ba29c5afcbdb009cac319d2)](https://sonarcloud.io/summary/new_code?id=embedded-pro_e-foc)
[![Duplicated Lines (%)](https://sonarcloud.io/api/project_badges/measure?project=embedded-pro_e-foc&metric=duplicated_lines_density&token=2d1b7ae361d044a96ba29c5afcbdb009cac319d2)](https://sonarcloud.io/summary/new_code?id=embedded-pro_e-foc)
[![Vulnerabilities](https://sonarcloud.io/api/project_badges/measure?project=embedded-pro_e-foc&metric=vulnerabilities&token=2d1b7ae361d044a96ba29c5afcbdb009cac319d2)](https://sonarcloud.io/summary/new_code?id=embedded-pro_e-foc)

# e-foc

An embedded motor control application implementing Field-Oriented Control (FOC) for BLDC and PMSM motors with strict real-time and memory constraints. Built for deterministic execution on resource-constrained microcontrollers.

## Overview

This project provides a production-ready motor control implementation designed for embedded systems with no heap allocation, deterministic execution, and minimal memory footprint. It demonstrates FOC algorithms with complete hardware integration for ST and TI microcontrollers.

## Features

### Core Capabilities
- **Field-Oriented Control (FOC)**: Complete implementation for BLDC and PMSM motors
- **Space Vector Modulation (SVM)**: Efficient PWM generation
- **PID Control**: Anti-windup implementations for current and velocity loops
- **Sensored Control**: Encoder and Hall sensor feedback integration
- **Auto-Tuning**: Resistance and Inductance estimation
- **Real-Time Performance**: Deterministic execution with no heap allocation

### Embedded Constraints
- **Zero Heap Allocation**: All memory statically allocated at compile-time
- **Fixed Memory Footprint**: Uses `infra::BoundedVector`, `infra::BoundedString` instead of standard containers
- **Minimal Stack Usage**: No recursion, predictable call depths
- **Platform Abstraction**: `PlatformFactory` interface with ST and TI implementations

### Simulation & Testing
- **C++ Simulator**: Mathematical models for motor evaluation and curve plotting
- **Unit Tests**: Comprehensive GoogleTest coverage for control algorithms
- **Hardware-in-the-Loop Ready**: Mockable interfaces for testability

## Getting Started

### Prerequisites
- CMake 3.24 or later
- Compatible toolchain for embedded targets (ARM GCC)
- Development board: EK-TM4C1294XL (TI) or STM32F407G-DISC1 (ST)
- Basic understanding of motor control and embedded systems

### Quick Start
1. Clone the repository with submodules
```bash
git clone --recursive https://github.com/embedded-pro/e-foc.git
cd e-foc
```

2. Build for host simulation
```bash
cmake --preset host
cmake --build --preset host-Debug
```

3. Run tests
```bash
ctest --preset host
```

4. Build for embedded target (example: TI EK-TM4C1294XL)
```bash
cmake --preset EK-TM4C1294XL
cmake --build --preset EK-TM4C1294XL-Debug
```

## Project Structure
```
e-foc/
├── core/                            # FOC library code (no application entry points)
│   ├── foc/                         # Field-Oriented Control
│   │   ├── interfaces/              # Abstract FOC interfaces (Foc.hpp, Driver.hpp, Units.hpp)
│   │   ├── implementations/         # Clarke/Park, SVM, torque/speed/position loops, Runner
│   │   └── instantiations/          # Concrete wiring of FOC components
│   ├── platform_abstraction/        # Platform abstraction interfaces and shared adapters
│   │   ├── PlatformFactory.hpp      # Abstract factory for peripherals
│   │   ├── AdcPhaseCurrentMeasurement.hpp
│   │   ├── CanBusAdapter.hpp
│   │   ├── QuadratureEncoderDecorator.hpp
│   │   └── MotorFieldOrientedControllerAdapter.hpp
│   ├── services/                    # Application-level services
│   │   ├── alignment/               # Motor alignment and offset detection
│   │   ├── cli/                     # Command-line interface service
│   │   ├── electrical_system_ident/ # Resistance/inductance estimation
│   │   ├── mechanical_system_ident/ # Friction and inertia estimation
│   │   └── non_volatile_memory/     # NVM persistence service
│   └── state_machine/               # FOC motor lifecycle state machine (Idle/Calibrating/Ready/Enabled/Fault)
├── integration_tests/               # BDD integration tests (Cucumber/Gherkin)
│   ├── features/                    # Gherkin .feature files
│   ├── steps/                       # Step definitions
│   ├── hooks/                       # Before/After scenario hooks
│   ├── main/                        # Test executable entry point
│   └── support/                     # Fixture, mocks, and bridge
├── targets/                         # Application entry points and platform implementations
│   ├── sync_foc_sensored/           # Synchronous FOC with encoder feedback
│   ├── hardware_test/               # Hardware validation application
│   └── platform_implementations/   # Platform-specific PlatformFactory implementations
│       ├── host/                    # Host simulation (GoogleTest stubs/mocks)
│       ├── st/                      # STM32 platform implementation
│       └── ti/                      # TI Tiva C platform implementation
├── tools/                           # Host-side developer tools
│   ├── simulator/                   # C++ motor simulators (torque, speed, position control)
│   └── can_commander/               # CAN bus command interface tool
├── infra/                           # Infrastructure submodules
│   ├── embedded-infra-lib/          # Bounded containers, build helpers, toolchain CMake
│   ├── numerical-toolbox/           # PID, filters, fixed-point algorithms
│   ├── can-lite/                    # CAN protocol library
│   └── hal/                         # Hardware abstraction layer (ST, TI)
├── documentation/                   # Architecture, design, theory, and requirements docs
└── build/                           # Build artifacts (generated, not committed)
```

## Key Design Principles

### Memory Management
- **No Heap Allocation**: All memory is statically allocated at compile-time
- **Bounded Containers**: Uses `infra::BoundedVector`, `infra::BoundedString` instead of STL containers
- **Fixed Memory Footprint**: Predictable RAM usage for resource-constrained systems

### Real-Time Performance
- **Deterministic Execution**: No dynamic allocation or unbounded loops in control paths
- **Minimal Branching**: Optimized control loops for consistent timing
- **ISR-Safe Design**: Interrupt service routines avoid virtual calls and complex operations

### Architecture
- **Dependency Injection**: Constructor injection for testability and flexibility
- **Platform Abstraction**: Clean separation between application logic and hardware
- **Interface-Driven Design**: Pure virtual interfaces enable mocking and testing

## How to Run the Simulator

The simulator includes a Qt-based GUI for real-time visualization. Since the development environment runs inside a Dev Container, an X server on the host machine is required to display the GUI.

### Host Setup

#### Windows

1. Install [VcXsrv](https://sourceforge.net/projects/vcxsrv/) (free) or [X410](https://x410.dev/) from the Microsoft Store.
2. Launch VcXsrv with the following settings:
   - **Multiple windows**
   - **Start no client**
   - **Disable access control** (checked)
3. Open the project in VS Code and reopen in the Dev Container.

#### Linux

1. Allow Docker containers to access your X server:
   ```bash
   xhost +local:docker
   ```
2. Open the project in VS Code and reopen in the Dev Container.

> **Note:** The Dev Container sets `DISPLAY=host.docker.internal:0.0` to forward GUI windows over TCP.
> On Linux, if you prefer Unix socket forwarding, you can override `DISPLAY` to `:0` inside the container and add a bind mount for `/tmp/.X11-unix`.

### Build & Run

Inside the Dev Container terminal:

```bash
cmake --preset host
cmake --build --preset host-Debug
```

Run the speed control simulator:
```bash
./build/host/bin/Debug/e_foc.tool.simulator.speed_control
```

Run the torque control simulator:
```bash
./build/host/bin/Debug/e_foc.tool.simulator.torque_control
```

Run with custom parameters (example):
```bash
./build/host/bin/Debug/e_foc.tool.simulator.speed_control --help
```

### Troubleshooting

| Problem | Solution |
|---|---|
| `cannot open display` | Ensure the X server is running on the host and "Disable access control" is enabled |
| Black window or no rendering | Set `LIBGL_ALWAYS_INDIRECT=1` (already configured in the Dev Container) |
| Windows firewall blocks display | Allow VcXsrv (`vcxsrv.exe`) through the Windows firewall for private networks |

## Documentation

### Architecture

| Document                                                    | Description                                                |
|-------------------------------------------------------------|------------------------------------------------------------|
| [System Architecture](documentation/architecture/system.md) | High-level system architecture and component relationships |

### Design

| Document                                                                              | Description                                  |
|---------------------------------------------------------------------------------------|----------------------------------------------|
| [FOC Transforms](documentation/design/foc-transforms.md)                              | Clarke and Park transform design             |
| [FOC Torque Control](documentation/design/foc-torque.md)                              | Torque (current) control loop design         |
| [FOC Speed Control](documentation/design/foc-speed.md)                                | Speed control loop design                    |
| [FOC Position Control](documentation/design/foc-position.md)                          | Position control loop design                 |
| [Alignment Service](documentation/design/service-alignment.md)                        | Motor alignment and offset detection service |
| [CLI Service](documentation/design/service-cli.md)                                    | Command-line interface service               |
| [Electrical Identification Service](documentation/design/service-electrical-ident.md) | Resistance and inductance estimation service |
| [Mechanical Identification Service](documentation/design/service-mechanical-ident.md) | Friction and inertia estimation service      |
| [NVM Service](documentation/design/service-nvm.md)                                    | Non-volatile memory persistence service      |
| [Integration Testing Design](documentation/design/integration-testing.md)             | Fixture design, calibration flow, CAN bridge |

### Requirements

| Document                                                                                                       | Description                                        |
|----------------------------------------------------------------------------------------------------------------|----------------------------------------------------|
| [State Machine Requirements](documentation/requirements/state-machine/state-machine.yaml)                      | Lifecycle state transitions and NVM boot behaviour |
| [FOC Torque Controller Requirements](documentation/requirements/foc/torque-controller.yaml)                    | Torque control loop functional requirements        |
| [CAN-to-State-Machine Integration Requirements](documentation/requirements/integration/can-state-machine.yaml) | CAN command to state machine mapping requirements  |

### Theory

| Document                                                                                       | Description                                              |
|------------------------------------------------------------------------------------------------|----------------------------------------------------------|
| [FOC Theory](documentation/theory/foc.md)                                                      | Field-Oriented Control theory and control block diagrams |
| [Motor Alignment](documentation/theory/alignment.md)                                           | Motor alignment and offset detection theory              |
| [Resistance & Inductance Estimation](documentation/theory/resistance-inductance-estimation.md) | Parameter estimation theory                              |
| [Friction & Inertia Estimation](documentation/theory/friction-inertia-estimation.md)           | Mechanical parameter estimation theory                   |

### Performance

| Document                                                                           | Description                                                      |
|------------------------------------------------------------------------------------|------------------------------------------------------------------|
| [Performance Optimization Guide](documentation/performance-optimization/README.md) | High-performance embedded code, assembly analysis, cycle budgets |

### Development References

| Document                                                        | Description                                               |
|-----------------------------------------------------------------|-----------------------------------------------------------|
| [Copilot / Agent Instructions](.github/copilot-instructions.md) | Development guidelines, patterns, and constraints         |
| [embedded-infra-lib](infra/embedded-infra-lib/README.md)        | Bounded containers and infrastructure library reference   |
| [numerical-toolbox](infra/numerical-toolbox/README.md)          | Reusable numerical algorithms (PID, filters, fixed-point) |

## Contributing

We welcome contributions! Please follow these guidelines:

### Development Workflow
1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Follow the coding standards in [copilot-instructions.md](/.github/copilot-instructions.md).
4. Ensure all tests pass
5. Update CHANGELOG.md according to release-please conventions
6. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
7. Push to the branch (`git push origin feature/AmazingFeature`)
8. Open a Pull Request

### Critical Guidelines
- **NO heap allocation** (`new`, `delete`, `malloc`, `free`)
- **NO standard containers** (use `infra::Bounded*` alternatives)
- Use fixed-size types (`uint8_t`, `int32_t`, etc.)
- Mark all non-mutating methods as `const`
- Write unit tests for new functionality
- Follow `.clang-format` rules for code formatting

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
