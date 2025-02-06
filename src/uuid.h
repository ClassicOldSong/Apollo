/**
 * @file src/uuid.h
 * @brief Declarations for UUID generation.
 */
#pragma once

// standard includes
#include <random>
#include <cstdio>

/**
 * @brief UUID utilities.
 */
namespace uuid_util {
  union uuid_t {
    std::uint8_t b8[16];
    std::uint16_t b16[8];
    std::uint32_t b32[4];
    std::uint64_t b64[2];

    static uuid_t generate(std::default_random_engine &engine) {
      std::uniform_int_distribution<std::uint8_t> dist(0, std::numeric_limits<std::uint8_t>::max());

      uuid_t buf;
      for (auto &el : buf.b8) {
        el = dist(engine);
      }

      buf.b8[7] &= (std::uint8_t) 0b00101111;
      buf.b8[9] &= (std::uint8_t) 0b10011111;

      return buf;
    }

    static uuid_t generate() {
      std::random_device r;

      std::default_random_engine engine {r()};

      return generate(engine);
    }

    static uuid_t
    parse(std::string& uuid_str) {
      if (uuid_str.length() != 36) {
        throw std::invalid_argument("Invalid UUID string length");
      }

      uuid_t uuid;
      unsigned int temp16_1;
      unsigned int temp32_1, temp32_2;

      // Parse UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
      unsigned int data1, data2;
      std::sscanf(
        uuid_str.c_str(), "%8x-%4x-%4x-%4x-%8x%4x",
        &uuid.b32[0], &data1, &data2, &temp16_1, &temp32_1, &temp32_2
      );

      // Assign parsed values into uuid_t structure
      uuid.b16[2] = static_cast<std::uint16_t>(data1);
      uuid.b16[3] = static_cast<std::uint16_t>(data2);

      // Manually splitting the last segments into bytes
      uuid.b8[8] = (temp16_1 >> 8) & 0xFF;
      uuid.b8[9] = temp16_1 & 0xFF;
      uuid.b8[10] = (temp32_1 >> 24) & 0xFF;
      uuid.b8[11] = (temp32_1 >> 16) & 0xFF;
      uuid.b8[12] = (temp32_1 >> 8) & 0xFF;
      uuid.b8[13] = temp32_1 & 0xFF;
      uuid.b8[14] = (temp32_2 >> 8) & 0xFF;
      uuid.b8[15] = temp32_2 & 0xFF;

      return uuid;
    }

    [[nodiscard]] std::string string() const {
      std::string result;

      result.reserve(sizeof(uuid_t) * 2 + 4);

      auto hex = util::hex(*this, true);
      auto hex_view = hex.to_string_view();

      std::string_view slices[] = {
        hex_view.substr(0, 8),
        hex_view.substr(8, 4),
        hex_view.substr(12, 4),
        hex_view.substr(16, 4)
      };
      auto last_slice = hex_view.substr(20, 12);

      for (auto &slice : slices) {
        std::copy(std::begin(slice), std::end(slice), std::back_inserter(result));

        result.push_back('-');
      }

      std::copy(std::begin(last_slice), std::end(last_slice), std::back_inserter(result));

      return result;
    }

    constexpr bool operator==(const uuid_t &other) const {
      return b64[0] == other.b64[0] && b64[1] == other.b64[1];
    }

    constexpr bool operator<(const uuid_t &other) const {
      return (b64[0] < other.b64[0] || (b64[0] == other.b64[0] && b64[1] < other.b64[1]));
    }

    constexpr bool operator>(const uuid_t &other) const {
      return (b64[0] > other.b64[0] || (b64[0] == other.b64[0] && b64[1] > other.b64[1]));
    }
  };
}  // namespace uuid_util
