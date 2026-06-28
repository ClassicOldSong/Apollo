/**
 * @file src/platform/usbip.h
 * @brief Shared helpers for packing USB/IP protocol frames.
 */
#pragma once

// standard includes
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace platf::usbip {
  constexpr std::uint16_t VERSION = 0x0111;

  constexpr std::size_t OP_COMMON_SIZE = 8;
  constexpr std::size_t BASIC_HEADER_SIZE = 20;
  constexpr std::size_t SUBMIT_HEADER_SIZE = 48;
  constexpr std::size_t USB_DEVICE_PATH_SIZE = 256;
  constexpr std::size_t USB_DEVICE_BUSID_SIZE = 32;
  constexpr std::size_t USB_DEVICE_RECORD_SIZE = 312;
  constexpr std::size_t USB_INTERFACE_RECORD_SIZE = 4;

  enum class op_code : std::uint16_t {
    req_import = 0x8003,
    rep_import = 0x0003,
    req_devlist = 0x8005,
    rep_devlist = 0x0005,
  };

  enum class command : std::uint32_t {
    cmd_submit = 0x0001,
    cmd_unlink = 0x0002,
    ret_submit = 0x0003,
    ret_unlink = 0x0004,
  };

  enum class direction : std::uint32_t {
    out = 0,
    in = 1,
  };

  enum class device_speed : std::uint32_t {
    unknown = 0,
    low = 1,
    full = 2,
    high = 3,
    wireless = 4,
    super = 5,
  };

  struct device_identity_t {
    std::string_view path;
    std::string_view busid;
    std::uint32_t busnum;
    std::uint32_t devnum;
    device_speed speed;
    std::uint16_t vendor_id;
    std::uint16_t product_id;
    std::uint16_t bcd_device;
    std::uint8_t device_class;
    std::uint8_t device_subclass;
    std::uint8_t device_protocol;
    std::uint8_t configuration_value;
    std::uint8_t configuration_count;
    std::uint8_t interface_count;
  };

  struct interface_identity_t {
    std::uint8_t interface_class;
    std::uint8_t interface_subclass;
    std::uint8_t interface_protocol;
  };

  struct op_common_t {
    std::uint16_t version;
    op_code code;
    std::uint32_t status;
  };

  struct import_request_t {
    op_common_t common;
    std::string busid;
  };

  struct basic_header_t {
    command command_code;
    std::uint32_t seqnum;
    std::uint32_t devid;
    direction transfer_direction;
    std::uint32_t endpoint;
  };

  struct submit_request_t {
    basic_header_t header;
    std::uint32_t transfer_flags;
    std::uint32_t transfer_buffer_length;
    std::uint32_t start_frame;
    std::uint32_t number_of_packets;
    std::uint32_t interval;
    std::array<std::uint8_t, 8> setup;
  };

  struct unlink_request_t {
    basic_header_t header;
    std::uint32_t unlink_seqnum;
  };

  inline void append_u8(std::vector<std::uint8_t> &out, std::uint8_t value) {
    out.push_back(value);
  }

  inline void append_be16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  }

  inline void append_be32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  }

  inline bool has_bytes(std::span<const std::uint8_t> data, std::size_t offset, std::size_t size) {
    return offset <= data.size() && size <= data.size() - offset;
  }

  inline std::uint16_t read_be16(std::span<const std::uint8_t> data, std::size_t offset) {
    if (!has_bytes(data, offset, 2)) {
      return 0;
    }

    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[offset]) << 8) |
                                      static_cast<std::uint16_t>(data[offset + 1]));
  }

  inline std::uint32_t read_be32(std::span<const std::uint8_t> data, std::size_t offset) {
    if (!has_bytes(data, offset, 4)) {
      return 0;
    }

    return (static_cast<std::uint32_t>(data[offset]) << 24) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 8) |
           static_cast<std::uint32_t>(data[offset + 3]);
  }

  inline std::string read_fixed_string(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width) {
    if (offset >= data.size() || width == 0) {
      return {};
    }

    width = std::min(width, data.size() - offset);
    std::size_t end = offset;
    while (end < offset + width && data[end] != 0) {
      ++end;
    }

    return {reinterpret_cast<const char *>(data.data() + offset), end - offset};
  }

  inline op_common_t parse_op_common(std::span<const std::uint8_t> data) {
    return op_common_t {
      .version = read_be16(data, 0),
      .code = static_cast<op_code>(read_be16(data, 2)),
      .status = read_be32(data, 4),
    };
  }

  inline import_request_t parse_import_request(std::span<const std::uint8_t> data) {
    return import_request_t {
      .common = parse_op_common(data),
      .busid = read_fixed_string(data, OP_COMMON_SIZE, USB_DEVICE_BUSID_SIZE),
    };
  }

  inline basic_header_t parse_basic_header(std::span<const std::uint8_t> data) {
    return basic_header_t {
      .command_code = static_cast<command>(read_be32(data, 0)),
      .seqnum = read_be32(data, 4),
      .devid = read_be32(data, 8),
      .transfer_direction = static_cast<direction>(read_be32(data, 12)),
      .endpoint = read_be32(data, 16),
    };
  }

  inline submit_request_t parse_submit_request(std::span<const std::uint8_t> data) {
    submit_request_t request {
      .header = parse_basic_header(data),
      .transfer_flags = read_be32(data, 20),
      .transfer_buffer_length = read_be32(data, 24),
      .start_frame = read_be32(data, 28),
      .number_of_packets = read_be32(data, 32),
      .interval = read_be32(data, 36),
      .setup = {},
    };

    if (has_bytes(data, 40, request.setup.size())) {
      std::copy_n(data.begin() + 40, request.setup.size(), request.setup.begin());
    }

    return request;
  }

  inline unlink_request_t parse_unlink_request(std::span<const std::uint8_t> data) {
    return unlink_request_t {
      .header = parse_basic_header(data),
      .unlink_seqnum = read_be32(data, 20),
    };
  }

  inline void append_fixed_string(std::vector<std::uint8_t> &out, std::string_view text, std::size_t width) {
    const auto copy_len = width == 0 ? 0 : std::min(text.size(), width - 1);
    out.insert(out.end(), text.begin(), text.begin() + copy_len);
    out.resize(out.size() + width - copy_len, 0);
  }

  inline void append_op_common(std::vector<std::uint8_t> &out, op_code code, std::uint32_t status) {
    append_be16(out, VERSION);
    append_be16(out, static_cast<std::uint16_t>(code));
    append_be32(out, status);
  }

  inline void append_device_record(std::vector<std::uint8_t> &out, const device_identity_t &device) {
    append_fixed_string(out, device.path, USB_DEVICE_PATH_SIZE);
    append_fixed_string(out, device.busid, USB_DEVICE_BUSID_SIZE);
    append_be32(out, device.busnum);
    append_be32(out, device.devnum);
    append_be32(out, static_cast<std::uint32_t>(device.speed));
    append_be16(out, device.vendor_id);
    append_be16(out, device.product_id);
    append_be16(out, device.bcd_device);
    append_u8(out, device.device_class);
    append_u8(out, device.device_subclass);
    append_u8(out, device.device_protocol);
    append_u8(out, device.configuration_value);
    append_u8(out, device.configuration_count);
    append_u8(out, device.interface_count);
  }

  inline void append_interface_record(std::vector<std::uint8_t> &out, const interface_identity_t &interface) {
    append_u8(out, interface.interface_class);
    append_u8(out, interface.interface_subclass);
    append_u8(out, interface.interface_protocol);
    append_u8(out, 0);
  }

  inline std::vector<std::uint8_t> make_import_reply(const device_identity_t &device) {
    std::vector<std::uint8_t> out;
    out.reserve(OP_COMMON_SIZE + USB_DEVICE_RECORD_SIZE);

    append_op_common(out, op_code::rep_import, 0);
    append_device_record(out, device);

    return out;
  }

  inline std::vector<std::uint8_t> make_import_error_reply(std::uint32_t status) {
    std::vector<std::uint8_t> out;
    out.reserve(OP_COMMON_SIZE);

    append_op_common(out, op_code::rep_import, status);

    return out;
  }

  inline std::vector<std::uint8_t> make_devlist_reply(const device_identity_t &device, std::span<const interface_identity_t> interfaces) {
    std::vector<std::uint8_t> out;
    out.reserve(OP_COMMON_SIZE + sizeof(std::uint32_t) + USB_DEVICE_RECORD_SIZE + interfaces.size() * USB_INTERFACE_RECORD_SIZE);

    append_op_common(out, op_code::rep_devlist, 0);
    append_be32(out, 1);
    append_device_record(out, device);
    for (const auto &interface : interfaces) {
      append_interface_record(out, interface);
    }

    return out;
  }

  inline void append_basic_header(
    std::vector<std::uint8_t> &out,
    command command,
    std::uint32_t seqnum,
    std::uint32_t devid,
    direction direction,
    std::uint32_t endpoint
  ) {
    append_be32(out, static_cast<std::uint32_t>(command));
    append_be32(out, seqnum);
    append_be32(out, devid);
    append_be32(out, static_cast<std::uint32_t>(direction));
    append_be32(out, endpoint);
  }

  inline std::vector<std::uint8_t> make_ret_submit(
    std::uint32_t seqnum,
    std::uint32_t devid,
    direction direction,
    std::uint32_t endpoint,
    std::int32_t status,
    std::span<const std::uint8_t> payload = {}
  ) {
    std::vector<std::uint8_t> out;
    out.reserve(SUBMIT_HEADER_SIZE + payload.size());

    append_basic_header(out, command::ret_submit, seqnum, devid, direction, endpoint);
    append_be32(out, static_cast<std::uint32_t>(status));
    append_be32(out, static_cast<std::uint32_t>(payload.size()));
    append_be32(out, 0);  // start_frame
    append_be32(out, 0);  // number_of_packets
    append_be32(out, 0);  // error_count
    for (std::size_t i = 0; i < 8; ++i) {
      append_u8(out, 0);
    }
    out.insert(out.end(), payload.begin(), payload.end());

    return out;
  }

  inline std::vector<std::uint8_t> make_ret_unlink(std::uint32_t seqnum, std::uint32_t devid, std::int32_t status) {
    std::vector<std::uint8_t> out;
    out.reserve(SUBMIT_HEADER_SIZE);

    append_basic_header(out, command::ret_unlink, seqnum, devid, direction::out, 0);
    append_be32(out, static_cast<std::uint32_t>(status));
    for (std::size_t i = 0; i < 24; ++i) {
      append_u8(out, 0);
    }

    return out;
  }
}  // namespace platf::usbip
