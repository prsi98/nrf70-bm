![Documentation Build](https://github.com/nrfconnect/nrf70-bm/actions/workflows/docbuild.yml/badge.svg?branch=main)

nRF70 BM Driver README
======================

Setup Instructions
==================

This section provides instructions on how to set up and use the nRF70 BM driver with `west` for nRF boards or `git submodule` for customer boards.


Using `west` for nRF Boards
---------------------------

For applications integrating the nRF70 Series BM driver it is recommended to use `west` and the nRF Connect SDK Toolchain,
for building and programming, when testing and evaluating the BM driver using official Nordic development boards.

1. **Toolchain and environment setup:**

    Install the nRF Command Line tools (for programming and debugging applications on Nordic development boards)
    downloading them from https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools.

    Follow the instructions on the official nRF Connect SDK documentation on how to install the required toolchain.
    https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/installation/install_ncs.html


    When using the command line and nRF Util method, `west` and all the required tools will be installed. Alternatively,
    `west` can be installed as described in the following step.

2. **Install West:**

    Follow the instructions on the official Zephyr Project documentation to install `west`:
    https://docs.zephyrproject.org/latest/guides/west/install.html

3. **Clone the Repository:**

    Clone the main repository using `west`:

    ```sh
    west init -m https://github.com/nrfconnect/nrf70-bm
    cd nrf70-bm
    west update
    ```

4. **Build the Project:**

    Use `west` to build the project for your specific board and type i.e., `CONFIG_NRF70_BOARD_TYPE_DK` (default) or `CONFIG_NRF70_BOARD_TYPE_EK`

    ```sh
    west build -b your_board_name -- -D<BOARD_TYPE>=y
    ```

5. **Flash the Board:**

    Flash the firmware onto your nRF board:

    ```sh
    west flash
    ```

Using `git submodule` for third-party boards
--------------------------------------------

1. **Clone the Repository:**

    Clone the main repository and its submodules:

    ```sh
    git clone https://github.com/nrfconnect/nrf70-bm --recurse-submodules
    ```

    If you have already cloned the repository without submodules, you can initialize and update the submodules with:

    ```sh
    git submodule update --init --recursive
    ```

2. Build and flashing instructions depend on the third-party board and the build system used. Please use board type as `CONFIG_NRF70_BOARD_TYPE_CUSTOM` and also add the TX power ceiling header file for the custom board at `nrf70_bm_lib/include`, see `nrf70_bm_lib/include/nrf70_tx_pwr_ceil_dk.h` for reference.


Source directory Structure
==========================

The source directory structure shall look like this:

.. code-block:: none

    nrf70_bm_lib/         # nRF70 BM driver library
    sdk_nrfxlib/          # nRF Connect SDK nrfxlib
      nrf_wifi/           # nRF70 Wi-Fi firmware patch blobs
    nrf_wifi/             # nRF Wi-Fi driver OS agnostic code
    samples/              # Sample applications
      radio_test_bm/      # Radio test sample application
      scan_bm/            # Scan sample application
      scan_rt_bm/         # Scan and Radio test combo sample application
    nrf70_zephyr_shim/    # Zephyr shim for nRF70, reference for third-party boards


Porting guide
=============

The nRF70 Series BM driver is designed to be portable across different platforms.
The `nrf70_zephyr_shim` directory contains a reference implementation for Zephyr.
This reference implementation can be used as a guide to port the library to other platforms.

Please refer to `nrf70_bm_lib/docs` for more information on the BM driver and the porting guide.

Documentation
=============

The documentation for the nRF70 BM driver is available in the `nrf70_bm_lib/docs` directory.
To build the documentation in a Linux environment, follow the below steps:

> **Note:** Currently, only Linux platforms are supported.

1. **Install python requirements:**

    Install the required python packages using `pip`:

    ```sh
    pip install -r nrf70_bm_lib/docs/requirements.txt
    ```
2. **Install doxygen:**

    Install doxygen using `apt` package manager (or any other package manager based on your OS):

    ```sh
    sudo apt install doxygen
    ```
3. **Build the Documentation:**

    Build the documentation using the following command:

    ```sh
    ./build-docs.sh
    ```

    The generated HTML files will be available in `nrf70_bm_lib/docs/build/html`.

    Open the `index.html` file in a browser to view the documentation.


Example code
============

The repository includes the below sample applications for the testing and the evaluation of the nRF70 Bare Metal library:

1. `BM radio test` sample to perform basic RF testing of the nRF70 Series device, as well as
    Factory Information Configuration Registers (FICR) programming.

2. `BM scan` sample to test Wi-Fi SSID scanning with the nRF70 Series devices.

3. `BM scan and radio test combo` sample to demonstrate runtime switching capability between scan and radio test operational modes for the BM library

The samples can be found under the ``samples/`` directory. The samples can be built using `west`, as explained above.

Tests
======

The repository includes a comprehensive test suite for validating the nRF70 BM driver functionality:

## Bustest Test Suite

The `tests/bustest/` directory contains Ztest-based unit tests for the nRF70 bus library. These tests validate:

### Register Tests
- **RDSR0 Test** (`test_rdsr0`): Validates reading of RDSR0 register (0x05) and verifies expected value
- **RDSR1 Test** (`test_rdsr1`): Validates reading of RDSR1 register (0x1F) and checks RPU awake status
- **RDSR2 Tests** (`test_rdsr2_*`): Comprehensive tests for RDSR2 register (0x2F) functionality:
  - Pattern tests: Validates writing and reading specific bit patterns (0xAA, 0x54)
  - Walking bit tests: Tests individual bit manipulation with walking '1' and walking '0' patterns
  - WRSR2 integration: Tests writing to WRSR2 register (0x3F) and reading back via RDSR2

### Bus Interface Tests
- **System Bus Test** (`test_sysbus`): Validates system bus read operations across multiple addresses
- **Peripheral Bus Test** (`test_peripbus`): Tests peripheral bus read/write operations with 24-bit addressing
- **Data RAM Test** (`test_dataram`): Memory test for data RAM region

### Test Configuration
The tests use the following register address macros for maintainability:
```c
#define RDSR0_ADDR    0x05
#define RDSR1_ADDR    0x1F
#define RDSR2_ADDR    0x2F
#define WRSR2_ADDR    0x3F
```

### Running the Tests
To run the bustest suite:

```sh
west build -b your_board_name tests/bustest
west flash
```

The tests will execute automatically and report results via the console output.

### Test Coverage
The bustest suite provides comprehensive coverage of:
- Register read/write operations via QSPI and SPI interfaces
- Bus interface functionality (system and peripheral buses)
- Memory operations and validation
- Error handling and edge cases
