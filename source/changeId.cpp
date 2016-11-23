#include <libusb-1.0/libusb.h>

#include <iostream>
#include <iomanip>
#include <regex>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <iterator>
#include <algorithm>
#include <random>

const auto listRegex = std::regex("^\\s*(l|list)\\s*$");
const auto listallRegex = std::regex("^\\s*(a|listall)\\s*$");
const auto setRegex = std::regex("^\\s*(?:s|set)\\s*(\\d+)\\s+(.+?)\\s*$");
const auto exitRegex = std::regex("^\\s*(e|exit)\\s*$");
const auto helpRegex = std::regex("(^\\s*$|^\\s*h\\s*$|^\\s*help\\s*$)");

/// Libusb is a cpp wrapper for libusb.
class Libusb {

    /// Descriptor represents a device's description.
    struct Descriptor {
        uint16_t vendorId;
        uint16_t productId;
    };

    public:
        Libusb() {
            checkUsbError(libusb_init(&_usbContext), "initializing libusb");
            _numberOfDevices = libusb_get_device_list(_usbContext, &_usbDevices);
            if (_numberOfDevices < 0) {
                libusb_exit(_usbContext);
                throw std::runtime_error("getting the devices list failed");
            }
        }
        Libusb(const Libusb&) = delete;
        Libusb(Libusb&&) = default;
        Libusb& operator=(const Libusb&) = delete;
        Libusb& operator=(Libusb&&) = default;
        virtual ~Libusb() {
            libusb_free_device_list(_usbDevices, 1);
            libusb_exit(_usbContext);
        }

        /// refreshDevices refreshes the list of connected devices.
        virtual void refreshDevices() {
            libusb_free_device_list(_usbDevices, 1);
            _numberOfDevices = libusb_get_device_list(_usbContext, &_usbDevices);
            if (_numberOfDevices < 0) {
                throw std::runtime_error("getting the devices list failed");
            }
        }

        /// vendorAndProductIds returns the vendor and product ids of each connected device.
        virtual std::vector<Descriptor> descriptors() {
            std::vector<Descriptor> retrievedDescriptors;
            retrievedDescriptors.reserve(_numberOfDevices);
            for (std::size_t index = 0; index < _numberOfDevices; ++index) {
                libusb_device_descriptor descriptor;
                checkUsbError(libusb_get_device_descriptor(_usbDevices[index], &descriptor), "retrieving the device descriptor");
                retrievedDescriptors.push_back(Descriptor{descriptor.idVendor, descriptor.idProduct});
            }
            return retrievedDescriptors;
        }

        /// ids returns the connected ftdi chips ids.
        virtual std::vector<std::string> ids(uint16_t ftdiVendorId = 1027, uint16_t ftdiProductId = 24596) {
            std::vector<std::string> retrievedIds;
            for (std::size_t index = 0; index < _numberOfDevices; ++index) {
                libusb_device_descriptor descriptor;
                checkUsbError(libusb_get_device_descriptor(_usbDevices[index], &descriptor), "retrieving the device descriptor");
                if (descriptor.idVendor == ftdiVendorId && descriptor.idProduct == ftdiProductId) {
                    auto id = std::string();
                    libusb_device_handle* usbHandle;
                    checkUsbError(libusb_open(_usbDevices[index], &usbHandle), "opening the device");
                    for (auto registerIndex = static_cast<uint16_t>(86); registerIndex < 128; ++registerIndex) {
                        auto buffer = std::array<uint8_t, 2>{};
                        checkUsbTransferError(libusb_control_transfer(
                            usbHandle,
                            LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
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
                        id.push_back(std::get<0>(buffer));
                    }
                    libusb_close(usbHandle);
                    retrievedIds.push_back(std::move(id));
                    _indexByNumber.insert(std::make_pair(retrievedIds.size(), index));
                }
            }
            return retrievedIds;
        }

        /// setId writes the given id to the target chip's eprom.
        virtual void setId(std::size_t number, std::string id) {
            const auto numberAndIndex = _indexByNumber.find(number);
            if (numberAndIndex == _indexByNumber.end()) {
                throw std::runtime_error("the given number is not listed");
            }
            if (id.size() > 32) {
                throw std::runtime_error("the id cannot have more than 32 characters");
            }

            // generate the new eeprom content
            auto targetEepromContent = std::array<uint8_t, 256>{
                0x01, 0x00, 0x03, 0x04, 0x14, 0x60, 0x00, 0x09, 0xa0, 0x2d, 0x08, 0x00, 0x01, 0x00, 0xa0, 0x0a,
                    0xaa,
                    static_cast<uint8_t>((id.size() + 1) * 2),
                    static_cast<uint8_t>(172 + id.size() * 2),
                    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x00, 0x56, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0x0a, 0x03, 0x46, 0x00, 0x54, 0x00, 0x44, 0x00, 0x49, 0x00,
                    static_cast<uint8_t>((id.size() + 1) * 2),
                    0x03, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            };
            for (auto byteIterator = id.begin(); byteIterator != id.end(); ++byteIterator) {
                targetEepromContent[172 + (byteIterator - id.begin()) * 2] = *byteIterator;
            }
            targetEepromContent[172 + id.size() * 2] = 0x10;
            targetEepromContent[172 + id.size() * 2 + 1] = 0x03;
            targetEepromContent[172 + (id.size() + 1) * 2] = 'F';
            targetEepromContent[172 + (id.size() + 2) * 2] = 'T';
            {
                std::random_device randomDevice;
                auto generator = std::mt19937(randomDevice());
                const auto availableBytes = std::array<uint8_t, 36>{
                    'A', 'B', 'C', 'D', 'E', 'F',
                    'G', 'H', 'I', 'J', 'K', 'L',
                    'M', 'N', 'O', 'P', 'Q', 'R',
                    'S', 'T', 'U', 'V', 'W', 'X',
                    'Y', 'Z', '0', '1', '2', '3',
                    '4', '5', '6', '7', '8', '9',
                };
                auto distribution = std::uniform_int_distribution<std::size_t>(0, availableBytes.size() - 1);
                for (
                    auto byteIterator = std::next(targetEepromContent.begin(), 172 + (id.size() + 3) * 2);
                    byteIterator != std::next(targetEepromContent.begin(), 172 + (id.size() + 8) * 2);
                    std::advance(byteIterator, 2)
                ) {
                    *byteIterator = availableBytes[distribution(generator)];
                }
            }

            // calculate the checksum
            {
                auto checksum = static_cast<uint16_t>(0xaaaa);
                for (auto byteIterator = targetEepromContent.begin(); byteIterator != std::prev(targetEepromContent.end(), 2); std::advance(byteIterator, 2)) {
                    checksum ^= static_cast<uint16_t>(*byteIterator) | (static_cast<uint16_t>(*std::next(byteIterator)) << 8);
                    checksum = (checksum << 1) | (checksum >> 15);
                }
                targetEepromContent[254] = static_cast<uint8_t>(checksum & 0xff);
                targetEepromContent[255] = static_cast<uint8_t>(checksum >> 8);
            }

            libusb_device_handle* usbHandle;
            checkUsbError(libusb_open(_usbDevices[numberAndIndex->second], &usbHandle), "opening the device");

            // read the eeprom
            auto eepromContent = std::array<uint8_t, 256>{};
            for (auto byteIterator = eepromContent.begin(); byteIterator != eepromContent.end(); std::advance(byteIterator, 2)) {
                checkUsbTransferError(libusb_control_transfer(
                    usbHandle,
                    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                    0x90,
                    0,
                    (byteIterator - eepromContent.begin()) / 2,
                    &*byteIterator,
                    2,
                    5000
                ), 2, "reading the eeprom");
            }

            // write bytes different from what is expected
            for (
                auto byteIterator = eepromContent.begin(), targetByteIterator = targetEepromContent.begin();
                byteIterator != eepromContent.end() && targetByteIterator != targetEepromContent.end();
                std::advance(byteIterator, 2), std::advance(targetByteIterator, 2)
            ) {
                if (*byteIterator != *targetByteIterator || *std::next(byteIterator) != *std::next(targetByteIterator)) {
                    checkUsbTransferError(libusb_control_transfer(
                        usbHandle,
                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
                        0x91,
                        static_cast<uint16_t>(*targetByteIterator) | (static_cast<uint16_t>(*std::next(targetByteIterator)) << 8),
                        (byteIterator - eepromContent.begin()) / 2, nullptr, 0, 5000
                    ), 0, "writting the eeprom");
                }
            }

            libusb_close(usbHandle);
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

        libusb_context* _usbContext;
        libusb_device** _usbDevices;
        int64_t _numberOfDevices;
        std::unordered_map<std::size_t, std::size_t> _indexByNumber;
};

int main(int argc, char* argv[]) {
    try {
        auto libusb = Libusb();

        std::string command;
        std::smatch match;
        for (;;) {
            std::cout << "> ";
            std::cout.flush();
            std::getline(std::cin, command);

            if (!std::regex_match(command, match, helpRegex)) {
                try {
                    if (std::regex_match(command, match, listRegex)) {
                        libusb.refreshDevices();
                        const auto ids = libusb.ids();
                        if (ids.empty()) {
                            std::cout << "no ftdi chips were found, make sure that the chip has the correct product id (1027) and vendor id (24596) with the 'listall' command" << std::endl;
                        } else {
                            std::cout << "\e[1mnumber    id\e[0m\n";
                            for (auto idIterator = ids.begin(); idIterator != ids.end(); ++idIterator) {
                                std::cout << std::setw(6) << (idIterator - ids.begin() + 1) << "    " << *idIterator << "\n";
                            }
                            std::cout.flush();
                        }
                        continue;
                    }

                    if (std::regex_match(command, match, listallRegex)) {
                        libusb.refreshDevices();
                        const auto descriptors = libusb.descriptors();
                        if (descriptors.empty()) {
                            std::cout << "No usb devices are connected" << std::endl;
                        } else {
                            std::cout << "\e[1mvendor id    product id\e[0m\n";
                            for (auto&& descriptor : descriptors) {
                                if (descriptor.vendorId == 1027 && descriptor.productId == 24596) {
                                    std::cout << "\e[32m";
                                }
                                std::cout << std::setw(9) << descriptor.vendorId << "    " << std::setw(10) << descriptor.productId << "\e[0m\n";
                            }
                            std::cout.flush();
                        }
                        continue;
                    }

                    if (std::regex_match(command, match, exitRegex)) {
                        break;
                    }

                    if (std::regex_match(command, match, setRegex)) {
                        libusb.setId(std::stoull(match[1]), match[2]);
                        continue;
                    }

                    throw std::runtime_error("Unknown command");
                } catch (const std::runtime_error& exception) {
                    std::cout << "\e[31m" << exception.what() << "\e[0m" << std::endl;
                }
            }

            std::cout <<
                "Available commands:\n"
                "    l, list                               displays connected ftdi chips' number and id\n"
                "                                              the number is used only by this program and may change when plugging a new device\n"
                "                                              the id is stored in the chip's memory and will not change even when it is disconnected\n"
                "                                              the id can be used within the Coyote library to create a connection with a specific chip\n"
                "    a, listall                            displays connected usb devices' vendor and product ids\n"
                "                                              if an ftdi chip does not have the correct vendor id (1027) and product id (24596),\n"
                "                                              the Coyote library will not create a connection\n"
                "                                              the vendor and product ids can be changed by using the software provided by the chip manufacturer\n"
                "                                              ftdi chips with the correct vendor and product ids are shown in green\n"
                "    s [number] [id], set [number] [id]    changes the id of the device with number [number] to [id]\n"
                "                                              the id cannot have more than 32 characters\n"
                "    e, exit                               terminates the program\n"
                "    h, help                               shows this help message\n"
            << std::endl;
        }
    } catch (const std::runtime_error& exception) {
        std::cout << "\e[31m" << exception.what() << "\e[0m" << std::endl;
        return 1;
    }

    return 0;
}
