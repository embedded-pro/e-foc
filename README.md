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
- **Hardware Abstraction**: Support for ST and TI microcontrollers

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
ctest --preset host-Debug
```

4. Build for embedded target (example: TI EK-TM4C1294XL)
```bash
cmake --preset EK-TM4C1294XL
cmake --build --preset EK-TM4C1294XL-Debug
```

## Project Structure
```
├── source/                     # Application-level motor control
│   ├── foc/                    # FOC implementations and instantiations
│   │   ├── implementations/    # Concrete FOC algorithm implementations
│   │   ├── instantiations/     # Application-specific FOC instances
│   │   └── interfaces/         # FOC interface definitions
│   ├── hardware/               # Hardware adapters and factory
│   │   ├── st/                 # STM32-specific adapters
│   │   ├── ti/                 # TI Tiva C-specific adapters
│   │   └── Host/               # Host simulation adapters
│   ├── services/               # Application-level services
│   └── application/            # Motor-specific implementations
│       ├── sync_foc_sensored/  # Synchronous FOC with sensors
│       └── hardware_test/      # Hardware validation tests
├── embedded-infra-lib/         # Submodule: infrastructure and utilities
├── numerical-toolbox/          # Generic algorithms (PID, filters, etc.)
├── hal/                        # Hardware Abstraction Layer
│   ├── st/                     # ST microcontroller HAL
│   └── ti/                     # TI microcontroller HAL
├── simulator/                  # C++ simulators for motor models
│   ├── pmsm/                   # PMSM mathematical models
│   └── speed_control/          # Speed control simulation
└── build/                      # Build artifacts (host, windows targets)
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
- **Hardware Abstraction**: Clean separation between application logic and hardware
- **Interface-Driven Design**: Pure virtual interfaces enable mocking and testing

## Documentation

For detailed information about the project structure and coding guidelines, see:
- [FOC Theory](documentation/theory/foc.md) - Field-Oriented Control theory and control block diagrams
- [Alignment Theory](documentation/theory/alignment.md) - Motor alignment and offset detection
- [Parameter Estimation](documentation/theory/resistance-inductance-estimation.md) - Resistance and inductance estimation theory
- [Copilot Instructions](/.github/copilot-instructions.md) - Development guidelines and patterns
- [Performance Optimization Guide](documentation/performance-optimization/README.md) - Writing high-performance embedded code, analyzing assembly, measuring cycles
- [embedded-infra-lib Documentation](embedded-infra-lib/README.md) - Infrastructure library reference
- [numerical-toolbox Documentation](numerical-toolbox/README.md) - Reusable algorithms

## Contributing

We welcome contributions! Please follow these guidelines:

### Development Workflow
1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Follow the coding standards in [copilot-instructions.md](/.github/copilot-instructions.md)
    and keep paths aligned with the current `source/` layout.
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
