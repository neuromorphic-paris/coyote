#pragma once

#include <libusb-1.0/libusb.h>

#include <stdexcept>
#include <array>
#include <vector>
#include <string>
#include <iterator>
#include <cstdlib>
#ifdef __APPLE__
    #include <unistd.h>
#endif

/// coyote is a communication library for the FT232H chip.
namespace coyote {

    /// Chip represents a FT232H chip.
    class Chip {
        public:
            Chip(uint32_t timeout = 5000, uint16_t vendorId = 1027, uint16_t productId = 24596) :
                _timeout(timeout),
                _usbContext(nullptr),
                _usbHandle(nullptr)
            {
                checkUsbError(libusb_init(&_usbContext), "initialize libusb");

                libusb_device** usbDevices;
                const auto numberOfDevices = libusb_get_device_list(_usbContext, &usbDevices);
                if (numberOfDevices < 0) {
                    libusb_exit(_usbContext);
                    throw std::runtime_error("getting the devices list failed");
                }
                for (std::size_t index = 0; index < numberOfDevices; ++index) {
                    libusb_device_descriptor descriptor;
                    const auto error = libusb_get_device_descriptor(usbDevices[index], &descriptor);
                    if (error != 0) {
                        libusb_exit(_usbContext);
                        throw std::runtime_error("retrieving the device descriptor failed with the error " + std::to_string(error));
                    }
                    if (descriptor.idVendor == vendorId && descriptor.idProduct == productId) {
                        const auto error = libusb_open(usbDevices[index], &_usbHandle);
                        if (error != 0) {
                            libusb_exit(_usbContext);
                            throw std::runtime_error("opening the device failed with the error " + std::to_string(error));
                        }
                        break;
                    }
                }
                if (_usbHandle == nullptr) {
                    libusb_exit(_usbContext);
                    throw std::runtime_error("no device with the correct vendor and product ids could be find");
                }
                libusb_free_device_list(usbDevices, 1);
                configure();
            }
            Chip(std::string id, uint32_t timeout = 5000, uint16_t vendorId = 1027, uint16_t productId = 24596) :
                _timeout(timeout),
                _usbContext(nullptr),
                _usbHandle(nullptr)
            {
                if (id.size() > 32) {
                    throw std::runtime_error("the id cannot have more than 32 characters");
                }
                checkUsbError(libusb_init(&_usbContext), "initialize libusb");
                libusb_device** usbDevices;
                const auto numberOfDevices = libusb_get_device_list(_usbContext, &usbDevices);
                if (numberOfDevices < 0) {
                    libusb_exit(_usbContext);
                    throw std::runtime_error("getting the devices list failed");
                }
                for (std::size_t index = 0; index < numberOfDevices; ++index) {
                    libusb_device_descriptor descriptor;
                    const auto error = libusb_get_device_descriptor(usbDevices[index], &descriptor);
                    if (error != 0) {
                        libusb_exit(_usbContext);
                        throw std::runtime_error("retrieving the device descriptor failed with the error " + std::to_string(error));
                    }
                    if (descriptor.idVendor == vendorId && descriptor.idProduct == productId) {
                        const auto error = libusb_open(usbDevices[index], &_usbHandle);
                        if (error != 0) {
                            libusb_exit(_usbContext);
                            throw std::runtime_error("opening the device failed with the error " + std::to_string(error));
                        }
                        auto retrievedId = std::string();
                        for (auto registerIndex = static_cast<uint16_t>(86); registerIndex < 128; ++registerIndex) {
                            auto buffer = std::array<uint8_t, 2>{};
                            checkUsbTransferError(libusb_control_transfer(
                                _usbHandle,
                                inputRequestType(),
                                0x90,
                                0,
                                registerIndex,
                                buffer.data(),
                                buffer.size(),
                                5000
                            ), buffer.size(), "reading the eeprom");
                            if (std::get<0>(buffer) == 0x10 && std::get<1>(buffer) == 0x03) {
                                break;
                            }
                            retrievedId.push_back(std::get<0>(buffer));
                        }
                        if (retrievedId == id) {
                            break;
                        }
                        libusb_close(_usbHandle);
                        _usbHandle = nullptr;
                        continue;
                    }
                }
                if (_usbHandle == nullptr) {
                    libusb_exit(_usbContext);
                    throw std::runtime_error("the requested device could not be find");
                }
                libusb_free_device_list(usbDevices, 1);
                configure();
            }
            Chip(const Chip&) = delete;
            Chip(Chip&&) = default;
            Chip& operator=(const Chip&) = delete;
            Chip& operator=(Chip&&) = default;
            virtual ~Chip() {
                libusb_close(_usbHandle);
                libusb_exit(_usbContext);
            }

            /// write sends bytes to the chip.
            virtual void write(const std::vector<uint8_t>& bytes, bool flush = true) {
                int32_t bytesSent;
                auto begin = bytes.begin();

                // complete the buffer
                if (!_writeBuffer.empty()) {
                    const auto spaceLeft = chunkSize() - _writeBuffer.size();
                    if (bytes.size() < spaceLeft) {
                        _writeBuffer.insert(_writeBuffer.end(), bytes.begin(), bytes.end());
                        if (flush) {
                            checkUsbError(libusb_bulk_transfer(
                                _usbHandle,
                                2,
                                const_cast<uint8_t*>(_writeBuffer.data()),
                                static_cast<int32_t>(_writeBuffer.size()),
                                &bytesSent,
                                _timeout
                            ), "writing bytes");
                            checkSize(_writeBuffer.size(), bytesSent, "writing bytes");
                            _writeBuffer.clear();
                        }
                        return;
                    } else {
                        begin = std::next(bytes.begin(), spaceLeft);
                        _writeBuffer.insert(_writeBuffer.end(), bytes.begin(), begin);
                        checkUsbError(libusb_bulk_transfer(
                            _usbHandle,
                            2,
                            const_cast<uint8_t*>(_writeBuffer.data()),
                            static_cast<int32_t>(_writeBuffer.size()),
                            &bytesSent,
                            _timeout
                        ), "writing bytes");
                        checkSize(_writeBuffer.size(), bytesSent, "writing bytes");
                        _writeBuffer.clear();
                    }
                }

                // send complete chunks
                for (std::size_t chunkIndex = 0; chunkIndex < (bytes.end() - begin) / chunkSize(); ++chunkIndex) {
                    checkUsbError(libusb_bulk_transfer(
                        _usbHandle,
                        2,
                        const_cast<uint8_t*>(&*begin),
                        static_cast<int32_t>(chunkSize()),
                        &bytesSent,
                        _timeout
                    ), "writing bytes");
                    checkSize(chunkSize(), bytesSent, "writing bytes");
                    std::advance(begin, chunkSize());
                }

                // flush the extra bytes or fill the buffer
                if (begin != bytes.end()) {
                    if (flush) {
                        const auto left = bytes.end() - begin;
                        checkUsbError(libusb_bulk_transfer(
                            _usbHandle,
                            2,
                            const_cast<uint8_t*>(&*begin),
                            static_cast<int32_t>(left),
                            &bytesSent,
                            _timeout
                        ), "writing bytes");
                        checkSize(left, bytesSent, "writing bytes");
                    } else {
                        _writeBuffer.insert(_writeBuffer.begin(), begin, bytes.end());
                    }
                }
            }

            /// read receives bytes from the chip.
            virtual std::vector<uint8_t> read() {
                auto bytes = std::vector<uint8_t>(chunkSize());
                auto actualSize = 0;
                checkUsbError(libusb_bulk_transfer(_usbHandle, 129, bytes.data(), static_cast<int32_t>(bytes.size()), &actualSize, 5000), "reading bytes");
                if (actualSize > 2) {
                    const auto fullPackets = actualSize / 512;
                    for (auto packetIndex = static_cast<std::size_t>(0); packetIndex < fullPackets; ++packetIndex) {
                        std::copy(
                            std::next(bytes.begin(), 512 * packetIndex + 2),
                            std::next(bytes.begin(), 512 * (packetIndex + 1)),
                            std::next(bytes.begin(), 510 * packetIndex)
                        );
                    }
                    const auto lastPacketSize = actualSize % 512;
                    if (lastPacketSize > 2) {
                        std::copy(
                            std::prev(bytes.end(), lastPacketSize - 2),
                            bytes.end(),
                            std::next(bytes.begin(), 510 * fullPackets)
                        );
                        bytes.resize(510 * fullPackets + lastPacketSize - 2);
                    } else {
                        bytes.resize(510 * fullPackets);
                    }
                } else {
                    bytes.clear();
                }
                return bytes;
            }

        protected:

            /// checkUsbError throws an exception if the returned code is not zero.
            static void checkUsbError(int32_t error, std::string message) {
                if (error != 0) {
                    throw std::runtime_error(message + " failed with the error " + libusb_error_name(error));
                }
            }

            /// checkUsbTransferError throws an exception if the returned code is negative or if the unexpected number of bytes is transferred.
            static void checkUsbTransferError(int32_t error, std::size_t expectedLength, std::string message) {
                if (error < 0) {
                    throw std::runtime_error(message + " failed with the error " + libusb_error_name(error));
                }
                if (error != expectedLength) {
                    throw std::runtime_error(
                        message
                        + " failed (expected a transfer of "
                        + std::to_string(expectedLength)
                        + " bytes, transferred "
                        + std::to_string(error)
                        + ")"
                    );
                }
            }

            /// checkSize throws an error if the expected number of bytes is not equal to the actual number of bytes.
            static void checkSize(std::size_t expected, int32_t actual, std::string message) {
                if (expected != actual) {
                    throw std::runtime_error(message + " failed (expected " + std::to_string(expected) + " bytes, got " + std::to_string(actual) + " bytes");
                }
            }

            /// chunkSize returns the chunk size used when reading and writting data.
            static std::size_t chunkSize() {
                return 65536;
            }

            /// inputRequestType returns the libusb type for input requests.
            static uint8_t inputRequestType() {
                return LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN;
            }

            /// outputRequestType returns the libusb type for output requests.
            static uint8_t outputRequestType() {
                return LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT;
            }

            /// configure prepares the device for the FT245 style synchronous FIFO mode.
            void configure() {
                {
                    const auto error = libusb_claim_interface(_usbHandle, 0);
                    if (error == LIBUSB_ERROR_BUSY) {
                        throw std::runtime_error("the requested device is busy");
                    } else {
                        checkUsbError(error, "claiming the interface");
                    }
                }
                try {
                    checkUsbTransferError(
                        libusb_control_transfer(_usbHandle, outputRequestType(), 0, 0, 1, nullptr, 0, _timeout),
                        0,
                        "resetting the device"
                    );
                    checkUsbTransferError(
                        libusb_control_transfer(_usbHandle, outputRequestType(), 11, 16639, 1, nullptr, 0, _timeout),
                        0,
                        "setting the bitmode"
                    );
                    checkUsbTransferError(
                        libusb_control_transfer(_usbHandle, outputRequestType(), 1, 257, 1, nullptr, 0, _timeout),
                        0,
                        "enabling the data-terminal-ready line"
                    );
                    checkUsbTransferError(
                        libusb_control_transfer(_usbHandle, outputRequestType(), 1, 547, 1, nullptr, 0, _timeout),
                        0,
                        "clearing the request-to-send line"
                    );
                    checkUsbTransferError(
                        libusb_control_transfer(_usbHandle, outputRequestType(), 2, 0, 257, nullptr, 0, _timeout),
                        0,
                        "enabling the flow control"
                    );
                    checkUsbTransferError(
                        libusb_control_transfer(_usbHandle, outputRequestType(), 9, 16, 1, nullptr, 0, _timeout),
                        0,
                        "setting the latency timer"
                    );
                } catch (const std::runtime_error& exception) {
                    libusb_release_interface(_usbHandle, 0);
                    libusb_close(_usbHandle);
                    libusb_exit(_usbContext);
                    throw exception;
                }
                _readBuffer.reserve(chunkSize());
                _writeBuffer.reserve(chunkSize());
            }

            uint32_t _timeout;
            libusb_context* _usbContext;
            libusb_device_handle* _usbHandle;
            std::vector<uint8_t> _readBuffer;
            std::vector<uint8_t> _writeBuffer;
    };

    /// DriverGuard unloads the default OS X driver for ftdi chips when constructed, and reloads it when destructed.
    class DriverGuard {
        public:
            DriverGuard() {
                #ifdef __APPLE__
                    if (getuid() != 0) {
                        throw std::runtime_error("root privileges are required to unload the default driver");
                    }
                    std::system("kextunload -q -b com.apple.driver.AppleUSBFTDI");
                #endif
            }
            DriverGuard(const DriverGuard&) = delete;
            DriverGuard(DriverGuard&&) = default;
            DriverGuard& operator=(const DriverGuard&) = delete;
            DriverGuard& operator=(DriverGuard&&) = default;
            virtual ~DriverGuard() {
                #ifdef __APPLE__
                    std::system("kextload -q -b com.apple.driver.AppleUSBFTDI");
                #endif
            }
    };
}
