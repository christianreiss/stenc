// SPDX-FileCopyrightText: 2022 stenc authors
//
// SPDX-License-Identifier: GPL-2.0-or-later

/*
Header file to send and recieve SPIN/SPOUT commands to SCSI device

Original program copyright 2010 John D. Coleman

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef _SCSIENC_H
#define _SCSIENC_H

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <arpa/inet.h>

#ifdef HAVE_SYS_MACHINE_H
#include <sys/machine.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

namespace scsi {

enum class encrypt_mode : std::uint8_t {
  off = 0u,
  external = 1u,
  on = 2u,
};

inline std::ostream& operator<<(std::ostream& os, encrypt_mode m)
{
  if (m == encrypt_mode::off) {
    os << "off";
  } else if (m == encrypt_mode::external) {
    os << "external";
  } else {
    os << "on";
  }
  return os;
}

enum class decrypt_mode : std::uint8_t {
  off = 0u,
  raw = 1u,
  on = 2u,
  mixed = 3u,
};

inline std::ostream& operator<<(std::ostream& os, decrypt_mode m)
{
  if (m == decrypt_mode::off) {
    os << "off";
  } else if (m == decrypt_mode::raw) {
    os << "raw";
  } else if (m == decrypt_mode::on) {
    os << "on";
  } else {
    os << "mixed";
  }
  return os;
}

enum class kad_type : std::uint8_t {
  ukad = 0u,  // unauthenticated key-associated data
  akad = 1u,  // authenticated key-associated data
  nonce = 2u, // nonce value
  mkad = 3u,  // metadata key-associated data
  wkkad = 4u, // wrapped key key-associated data
};

enum class kadf : std::uint8_t {
  unspecified = 0u,
  binary_key_name = 1u,
  ascii_key_name = 2u,
};

// key-associated data
struct __attribute__((packed)) kad {
  kad_type type;
  std::byte flags;
  static constexpr auto flags_authenticated_pos {0u};
  static constexpr std::byte flags_authenticated_mask {
      7u << flags_authenticated_pos};
  std::uint16_t length;
  std::uint8_t descriptor[];
};
static_assert(sizeof(kad) == 4u);

// common 4-byte header of all SP-IN and SP-OUT pages
struct __attribute__((packed)) page_header {
  std::uint16_t page_code;
  std::uint16_t length;
};
static_assert(sizeof(page_header) == 4u);

// device encryption status page
struct __attribute__((packed)) page_des {
  std::uint16_t page_code;
  std::uint16_t length;
  std::byte scope;
  static constexpr auto scope_it_nexus_pos {5u};
  static constexpr std::byte scope_it_nexus_mask {7u << scope_it_nexus_pos};
  static constexpr auto scope_encryption_pos {0u};
  static constexpr std::byte scope_encryption_mask {7u << scope_encryption_pos};
  encrypt_mode encryption_mode;
  decrypt_mode decryption_mode;
  std::uint8_t algorithm_index;
  std::uint32_t key_instance_counter;
  std::byte flags;
  static constexpr auto flags_parameters_control_pos {4u};
  static constexpr std::byte flags_parameters_control_mask {
      7u << flags_parameters_control_pos};
  // volume contains encrypted logical blocks
  static constexpr auto flags_vcelb_pos {3u};
  static constexpr std::byte flags_vcelb_mask {1u << flags_vcelb_pos};
  // check external encryption mode status
  static constexpr auto flags_ceems_pos {1u};
  static constexpr std::byte flags_ceems_mask {3u << flags_ceems_pos};
  // raw decryption mode disabled
  static constexpr auto flags_rdmd_pos {0u};
  static constexpr std::byte flags_rdmd_mask {1u << flags_rdmd_pos};
  kadf kad_format;
  std::uint16_t asdk_count;
  std::byte reserved[8];
  kad kads[];
};
static_assert(sizeof(page_des) == 24u);

constexpr std::size_t SSP_PAGE_ALLOCATION = 8192;
using page_buffer = std::uint8_t[SSP_PAGE_ALLOCATION];

// set data encryption page
struct __attribute__((packed)) page_sde {
  std::uint16_t page_code;
  std::uint16_t length;
  std::byte control;
  static constexpr auto control_scope_pos {5u};
  static constexpr std::byte control_scope_mask {7u << control_scope_pos};
  static constexpr auto control_lock_pos {0u};
  static constexpr std::byte control_lock_mask {1u << control_lock_pos};
  std::byte flags;
  // check external encryption mode
  static constexpr auto flags_ceem_pos {6u};
  static constexpr std::byte flags_ceem_mask {3u << flags_ceem_pos};
  // raw decryption mode control
  static constexpr auto flags_rdmc_pos {4u};
  static constexpr std::byte flags_rdmc_mask {3u << flags_rdmc_pos};
  // supplemental decryption key
  static constexpr auto flags_sdk_pos {3u};
  static constexpr std::byte flags_sdk_mask {1u << flags_sdk_pos};
  // clear key on demount
  static constexpr auto flags_ckod_pos {2u};
  static constexpr std::byte flags_ckod_mask {1u << flags_ckod_pos};
  // clear key on reservation preempt
  static constexpr auto flags_ckorp_pos {1u};
  static constexpr std::byte flags_ckorp_mask {1u << flags_ckorp_pos};
  // clear key on reservation loss
  static constexpr auto flags_ckorl_pos {0u};
  static constexpr std::byte flags_ckorl_mask {1u << flags_ckorl_pos};
  encrypt_mode encryption_mode;
  decrypt_mode decryption_mode;
  std::uint8_t algorithm_index;
  std::uint8_t key_format;
  kadf kad_format;
  std::byte reserved[7];
  std::uint16_t key_length;
  std::uint8_t key[];
};
static_assert(sizeof(page_sde) == 20u);

enum class sde_rdmc : std::uint8_t {
  algorithm_default = 0u << page_sde::flags_rdmc_pos,
  enabled = 2u << page_sde::flags_rdmc_pos, // corresponds to --allow-raw-read
                                            // command line option
  disabled =
      3u << page_sde::flags_rdmc_pos, // corresponds to --no-allow-raw-read
                                      // command line option
};

// next block encryption status page
struct __attribute__((packed)) page_nbes {
  std::uint16_t page_code;
  std::uint16_t length;
  std::uint64_t logical_object_number;
  std::byte status;
  static constexpr auto status_compression_pos {4u};
  static constexpr std::byte status_compression_mask {
      15u << status_compression_pos};
  static constexpr auto status_encryption_pos {0u};
  static constexpr std::byte status_encryption_mask {15u
                                                     << status_encryption_pos};
  std::uint8_t algorithm_index;
  std::byte flags;
  // encryption mode external status
  static constexpr auto flags_emes_pos {1u};
  static constexpr std::byte flags_emes_mask {1u << flags_emes_pos};
  // raw decryption mode disabled status
  static constexpr auto flags_rdmds_pos {0u};
  static constexpr std::byte flags_rdmds_mask {1u << flags_rdmds_pos};
  kadf kad_format;
  kad kads[];
};
static_assert(sizeof(page_nbes) == 16u);

struct __attribute__((packed)) algorithm_descriptor {
  std::uint8_t algorithm_index;
  std::byte reserved1;
  std::uint16_t length;
  std::byte flags1;
  // algorithm valid for mounted volume
  static constexpr auto flags1_avfmv_pos {7u};
  static constexpr std::byte flags1_avfmv_mask {1u << flags1_avfmv_pos};
  // supplemental decryption key capable
  static constexpr auto flags1_sdk_c_pos {6u};
  static constexpr std::byte flags1_sdk_c_mask {1u << flags1_sdk_c_pos};
  // message authentication code capable
  static constexpr auto flags1_mac_c_pos {5u};
  static constexpr std::byte flags1_mac_c_mask {1u << flags1_mac_c_pos};
  // distinguish encrypted logical block capable
  static constexpr auto flags1_delb_c_pos {4u};
  static constexpr std::byte flags1_delb_c_mask {1u << flags1_delb_c_pos};
  // decryption capabilities
  static constexpr auto flags1_decrypt_c_pos {2u};
  static constexpr std::byte flags1_decrypt_c_mask {3u << flags1_decrypt_c_pos};
  // encryption capabilities
  static constexpr auto flags1_encrypt_c_pos {0u};
  static constexpr std::byte flags1_encrypt_c_mask {3u << flags1_encrypt_c_pos};
  std::byte flags2;
  // algorithm valid for current logical position
  static constexpr auto flags2_avfcp_pos {6u};
  static constexpr std::byte flags2_avfcp_mask {3u << flags2_avfcp_pos};
  // nonce capabilities
  static constexpr auto flags2_nonce_pos {4u};
  static constexpr std::byte flags2_nonce_mask {3u << flags2_nonce_pos};
  // KAD format capable
  static constexpr auto flags2_kadf_c_pos {3u};
  static constexpr std::byte flags2_kadf_c_mask {1u << flags2_kadf_c_pos};
  // volume contains encrypted logical blocks capable
  static constexpr auto flags2_vcelb_c_pos {2u};
  static constexpr std::byte flags2_vcelb_c_mask {1u << flags2_vcelb_c_pos};
  // U-KAD fixed
  static constexpr auto flags2_ukadf_pos {1u};
  static constexpr std::byte flags2_ukadf_mask {1u << flags2_ukadf_pos};
  // A-KAD fixed
  static constexpr auto flags2_akadf_pos {0u};
  static constexpr std::byte flags2_akadf_mask {1u << flags2_akadf_pos};
  std::uint16_t maximum_ukad_length;
  std::uint16_t maximum_akad_length;
  std::uint16_t key_length;
  std::byte flags3;
  // decryption capabilities
  static constexpr auto flags3_dkad_c_pos {6u};
  static constexpr std::byte flags3_dkad_c_mask {3u << flags3_dkad_c_pos};
  // external encryption mode control capabilities
  static constexpr auto flags3_eemc_c_pos {4u};
  static constexpr std::byte flags3_eemc_c_mask {3u << flags3_eemc_c_pos};
  // raw decryption mode control capabilities
  static constexpr auto flags3_rdmc_c_pos {1u};
  static constexpr std::byte flags3_rdmc_c_mask {7u << flags3_rdmc_c_pos};
  // encryption algorithm records encryption mode
  static constexpr auto flags3_earem_pos {0u};
  static constexpr std::byte flags3_earem_mask {1u << flags3_earem_pos};
  std::uint8_t maximum_eedk_count;
  static constexpr auto maximum_eedk_count_pos {0u};
  static constexpr std::uint8_t maximum_eedk_count_mask {
      15u << maximum_eedk_count_pos};
  std::uint16_t msdk_count;
  std::uint16_t maximum_eedk_size;
  std::byte reserved2[2];
  std::uint32_t security_algorithm_code;

  static constexpr std::size_t header_size {4u};
};
static_assert(sizeof(algorithm_descriptor) == 24u);

// device encryption capabilities page
struct __attribute__((packed)) page_dec {
  std::uint16_t page_code;
  std::uint16_t length;
  std::byte flags;
  // external data encryption control capable
  static constexpr auto flags_extdecc_pos {2u};
  static constexpr std::byte flags_extdecc_mask {3u << flags_extdecc_pos};
  // configuration prevented
  static constexpr auto flags_cfg_p_pos {0u};
  static constexpr std::byte flags_cfg_p_mask {3u << flags_cfg_p_pos};
  std::byte reserved[15];
  algorithm_descriptor ads[];
};
static_assert(sizeof(page_dec) == 20u);

struct __attribute__((packed)) inquiry_data {
  // bitfield definitions omitted since stenc only uses vendor and product info
  std::byte peripheral;
  std::byte flags1;
  std::uint8_t version;
  std::byte flags2;
  std::uint8_t additional_length;
  std::byte flags3;
  std::byte flags4;
  std::byte flags5;
  char vendor[8];
  char product_id[16];
  char product_rev[4];
  std::uint8_t vendor_specific[20];
  std::byte reserved1[2];
  std::uint16_t version_descriptor[8];
  std::byte reserved2[22];

  static constexpr std::size_t header_size {5u};
};
static_assert(sizeof(inquiry_data) == 96u);

struct __attribute__((packed)) sense_data {
  std::byte response;
  static constexpr auto response_valid_pos {7u};
  static constexpr std::byte response_valid_mask {1u << response_valid_pos};
  static constexpr auto response_code_pos {0u};
  static constexpr std::byte response_code_mask {127u << response_code_pos};
  std::byte reserved;
  std::byte flags;
  static constexpr auto flags_filemark_pos {7u};
  static constexpr std::byte flags_filemark_mask {1u << flags_filemark_pos};
  static constexpr auto flags_eom_pos {6u}; // end of medium
  static constexpr std::byte flags_eom_mask {1u << flags_eom_pos};
  static constexpr auto flags_ili_pos {5u}; // incorrect length indicator
  static constexpr std::byte flags_ili_mask {1u << flags_ili_pos};
  static constexpr auto flags_sdat_ovfl_pos {4u}; // sense data overflow
  static constexpr std::byte flags_sdat_ovfl_mask {1u << flags_sdat_ovfl_pos};
  static constexpr auto flags_sense_key_pos {0u};
  static constexpr std::byte flags_sense_key_mask {15u << flags_sense_key_pos};
  std::uint8_t information[4];
  std::uint8_t additional_sense_length;
  std::uint8_t command_specific_information[4];
  std::uint8_t additional_sense_code;
  std::uint8_t additional_sense_qualifier;
  std::uint8_t field_replaceable_unit_code;
  std::uint8_t sense_key_specific[3];
  std::uint8_t additional_sense_bytes[];

  static constexpr std::byte no_sense {0u};
  static constexpr std::byte recovered_error {1u};
  static constexpr std::byte not_ready {2u};
  static constexpr std::byte medium_error {3u};
  static constexpr std::byte hardware_error {4u};
  static constexpr std::byte illegal_request {5u};
  static constexpr std::byte unit_attention {6u};
  static constexpr std::byte data_protect {7u};
  static constexpr std::byte blank_check {8u};

  static constexpr std::size_t header_size {8u};
  static constexpr std::size_t maximum_size {252u}; // per SPC-5
};
static_assert(sizeof(sense_data) == 18u);

// declared as std::array instead of std::uint8_t[] because
// std::unique_ptr does not allow construction of fixed-sized arrays
using sense_buffer = std::array<std::uint8_t, sense_data::maximum_size>;

class scsi_error : public std::runtime_error {
public:
  explicit scsi_error(std::unique_ptr<sense_buffer>&& buf)
      : std::runtime_error {"SCSI I/O error"}, sense_buf {std::move(buf)}
  {}
  const sense_data& get_sense() const
  {
    return reinterpret_cast<sense_data&>(*sense_buf->data());
  }

private:
  std::unique_ptr<sense_buffer> sense_buf;
};

// Extract pointers to kad structures within a variable-length page.
// Page must have a page_header layout
template <typename Page>
std::vector<std::reference_wrapper<const kad>> read_page_kads(const Page& page)
{
  const auto start {reinterpret_cast<const std::uint8_t *>(&page)};
  auto it {start + sizeof(Page)};
  const auto end {start + ntohs(page.length) + sizeof(page_header)};
  std::vector<std::reference_wrapper<const kad>> v {};

  while (it < end) {
    auto elem {reinterpret_cast<const kad *>(it)};
    v.push_back(std::cref(*elem));
    it += ntohs(elem->length) + sizeof(kad);
  }
  return v;
}

// Check if a tape is loaded
bool is_device_ready(const std::string& device);
// Get SCSI inquiry data from device
inquiry_data get_inquiry(const std::string& device);
// Get data encryption status page
void get_des(const std::string& device, std::uint8_t *buffer,
             std::size_t length);
// Get next block encryption status page
void get_nbes(const std::string& device, std::uint8_t *buffer,
              std::size_t length);
// Get device encryption capabilities
void get_dec(const std::string& device, std::uint8_t *buffer,
             std::size_t length);
// Fill out a set data encryption page with parameters.
// Result is allocated and returned as a std::unique_ptr and should
// be sent to the device using scsi::write_sde
std::unique_ptr<const std::uint8_t[]>
make_sde(encrypt_mode enc_mode, decrypt_mode dec_mode,
         std::uint8_t algorithm_index, const std::vector<std::uint8_t>& key,
         const std::string& key_name, kadf key_format, sde_rdmc rdmc,
         bool ckod);
// Write set data encryption parameters to device
void write_sde(const std::string& device, const std::uint8_t *sde_buffer);
void print_sense_data(std::ostream& os, const sense_data& sd);
std::vector<std::reference_wrapper<const algorithm_descriptor>>
read_algorithms(const page_dec& page);

} // namespace scsi

#endif
