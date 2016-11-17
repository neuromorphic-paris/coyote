#include "catch.hpp"

#include "../source/coyote.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <mutex>

TEST_CASE("Connect to the first available chip", "[DriverGuard, Chip]") {
    const auto driverGuard = coyote::DriverGuard();
    REQUIRE_NOTHROW(coyote::Chip());
}

TEST_CASE("Connect to the chip with the given id", "[DriverGuard, Chip]") {
    const auto driverGuard = coyote::DriverGuard();
    REQUIRE_NOTHROW(coyote::Chip("writer"));
}

TEST_CASE("Connect to the chip with the given id and monitor the writing performance", "[DriverGuard, Chip]") {
    const auto driverGuard = coyote::DriverGuard();
    auto chip = coyote::Chip("writer");
    const auto bytes = std::vector<uint8_t>(10e6);
    const auto begin = std::chrono::high_resolution_clock::now();
    chip.write(bytes);
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - begin).count();
    std::cout << "Writing bitrate: " << bytes.size() / static_cast<double>(duration) << " MB/s" << std::endl;
}

TEST_CASE("Connect to the chip with the given id and monitor its reading performance", "[DriverGuard, Chip]") {
    const auto driverGuard = coyote::DriverGuard();
    auto chip = coyote::Chip("reader");
    const auto target = static_cast<std::size_t>(10e6);
    auto readBytes = static_cast<std::size_t>(0);
    const auto begin = std::chrono::high_resolution_clock::now();
    while (readBytes < target) {
            readBytes += chip.read().size();
    }
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - begin).count();
    std::cout << "Reading bitrate: " << readBytes / static_cast<double>(duration) << " MB/s" << std::endl;
}
