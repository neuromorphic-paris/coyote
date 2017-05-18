![coyote](coyoteBanner.png "The Coyote banner")

# Coyote

Coyote is a header-only library to communicate with the FT2232H chip. This repository provides a tool called changeId that can setup a FT2232H to work with the library.

# Installation

## Dependencies

Coyote relies on [Premake 4.x](https://github.com/premake/premake-4.x) (x ≥ 3), which is a utility to ease the install process. Follow these steps:
  - __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt-get install premake4`.
  - __Fedora__: Open a terminal and execute the command `sudo dnf install premake`. Then, run<br />
  `echo '/usr/local/lib' | sudo tee /etc/ld.so.conf.d/neuromorphic-paris.conf > /dev/null`.
  - __OS X__: Open a terminal and execute the command `brew install premake`. If the command is not found, you need to install Homebrew first with the command<br />
  `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`.

Coyote also depends on libusb, which is a cross-platform USB library. Follow these steps:
  - __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt-get install libusb-1.0-0-dev`.
  - __Fedora__: Open a terminal and execute the command `sudo dnf install libusb`.
  - __OS X__: Open a terminal and execute the command `brew install libusb`.

## Install

To install the source, go to the *coyote* directory and run:
  - __Linux__: `premake4 install`
  - __OS X__: `premake4 install`
The library files are installed in */usr/local/include*.

## Uninstall

To uninstall the library, run `premake4 uninstall` from the *coyote* directory.

## Test

To test the library, run the following commands:
  - Go to the *coyote* directory and run `premake4 gmake && cd build && make`.
  - Run the executable *Release/coyoteTest*. The tests require having a FT2232H reading and writing bytes from a device (such as a FPGA).

# Documentation

## Coyote

Basic example for writing bytes:

```cpp
#include <coyote.hpp>

int main(int argc, char* argv[]) {
    const auto driverGuard = coyote::DriverGuard(); // unloads the default driver under MacOS (must be run as root)
                                                    // does nothing on other operating systems
                                                    // the driver is reloaded when the coyote::DriverGuard is destructed

    auto chip = coyote::Chip(); // connect to the first chip available

    auto bytes = std::vector<uint8_t>({0xc0, 0x10, 0x73}); // create a vector of bytes

    chip.write(bytes); // send the bytes

    return 0;
}
```

Basic example for reading bytes:

```cpp
#include <coyote.hpp>

int main(int argc, char* argv[]) {
    const auto driverGuard = coyote::DriverGuard(); // unloads the default driver under MacOS (must be run as root)
                                                    // does nothing on other operating systems
                                                    // the driver is reloaded when the coyote::DriverGuard is destructed

    auto chip = coyote::Chip(); // connect to the first chip available

    const auto bytes = chip.read(); // read bytes
                                    // read returns as soon as a USB packet is received
                                    // use the size method of the returned vector to determine how many bytes were read

    for (auto byte : bytes) {
        // loop over the read bytes and do amazing things
    }

    return 0;
}
```

`coyote::Chip` has the signature:
```cpp
namespace coyote {

    /// Chip represents a FT232H chip.
    class Chip {
        public:
            Chip(uint32_t timeout = 5000, uint16_t vendorId = 1027, uint16_t productId = 24596);
            Chip(std::string id, uint32_t timeout = 5000, uint16_t vendorId = 1027, uint16_t productId = 24596);

            /// write sends bytes to the chip.
            virtual void write(const std::vector<uint8_t>& bytes, bool flush = true);

            /// read receives bytes from the chip.
            virtual std::vector<uint8_t> read();
}
```

- `timeout` is the maximum time in milliseconds between a USB packet sending and its acknowledge. If the timeout is reached, an exception is thrown.
- `vendorId` is FTDI's USB identifier.
- `productId` is the FTH2232 chip's USB identifier.
- `bytes` is a vector of bytes to send. It can have any length. The Coyote library will take care of splitting the bytes to send into chunks with the optimal size.
- `flush` determines wether incomplete chunks are sent. As an example, if 1000000 bytes are passed to the `write` function and the packet size is 65536, fifteen complete chunks and one chunk with 16960 bytes are to be sent. If `flush` is `true` (default), the incomplete chunk is sent. Otherwise, the incomplete chunk is stored in a buffer, and will be sent with the next `write` call. The larger the chunks, the faster the transfer. However, waiting for chunks to be filled may result in an increased latency.

`coyote::Chip`has two constructors: the first one connects to the first chip available, whereas the second targets a chip with a specific id.

`coyote::DriverGuard` has the signature:
```cpp
namespace coyote {

    /// DriverGuard unloads the default OS X driver for ftdi chips when constructed, and reloads it when destructed.
    class DriverGuard {
        DriverGuard();
    }
}
```

## changeId

changeId sets up a FT2232H chip to work in FT245-style synchronous FIFO mode. It is used to define the chip's id, which is used by the Coyote library to connect to a specific chip. The chip's id is stored in the chip's eeprom: it will not be lost even if the chip is powered off.

To compile the changeId tool, go to the *coyote* directory and run `premake4 gmake && cd build && make`. This command generates the executable *build/Release/changeId*, which can be copied elsewhere in the computer if needed.

Running *changeId* will start an interactive shell. An empty command (pressing the RETURN key) or the command `help` (followed by RETURN) will display the list of available commands:

- `l` or `list` displays connected ftdi chips' number and id. The number is used only by this program and may change when plugging a new device.
- `a` or `listall` displays connected usb devices' vendor and product ids. If an ftdi chip does not have the correct vendor id (1027) and product id (24596), the Coyote library will not create a connection. The vendor and product ids can be changed by using the software provided by the chip manufacturer. Ftdi chips with the correct vendor and product ids are shown in green
- `s [number] [id]` or `set [number] [id]` changes the id of the device with number [number] to [id]. The id cannot have more than 32 characters. This command must be run after the `list` command and without connecting or disconnecting devices in-between. Changing the id will also setup the chip to work in FT245-style synchronous FIFO mode.
- `e` or `exit` terminates the program.
- `h` or `help` shows this help message.

# License

See the [LICENSE](LICENSE.txt) file for license rights and limitations (GNU GPLv3).
