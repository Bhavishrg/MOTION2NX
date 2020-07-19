// MIT License
//
// Copyright (c) 2019 Oleksandr Tkachenko, Lennart Braun
// Cryptography and Privacy Engineering Group (ENCRYPTO)
// TU Darmstadt, Germany
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "utility/bit_matrix.h"
#include "utility/bit_vector.h"
#include "utility/block.h"
#include "utility/meta.hpp"
#include "utility/reusable_future.h"

namespace ENCRYPTO {
class FiberCondition;
}  // namespace ENCRYPTO

namespace MOTION {

enum OTExtensionDataType : uint {
  rcv_masks = 0,
  rcv_corrections = 1,
  snd_messages = 2,
  OTExtension_invalid_data_type = 3
};

enum class OTMsgType {
  bit,
  block128,
  uint8,
  uint16,
  uint32,
  uint64,
  uint128
};

struct OTExtensionReceiverData {
  OTExtensionReceiverData();
  ~OTExtensionReceiverData() = default;

  [[nodiscard]] ENCRYPTO::ReusableFiberFuture<ENCRYPTO::block128_vector>
  RegisterForBlock128SenderMessage(std::size_t ot_id, std::size_t size);
  [[nodiscard]] ENCRYPTO::ReusableFiberFuture<ENCRYPTO::BitVector<>> RegisterForBitSenderMessage(
      std::size_t ot_id, std::size_t size);
  template <typename T>
  [[nodiscard]] ENCRYPTO::ReusableFiberFuture<std::vector<T>> RegisterForIntSenderMessage(
      std::size_t ot_id, std::size_t size);

  // matrix of the OT extension scheme
  // XXX: can't we delete this after setup?
  std::shared_ptr<ENCRYPTO::BitMatrix> T_;

  // if many OTs are received in batches, it is not necessary to store all of the flags
  // for received messages but only for the first OT id in the batch. Thus, use a hash table.
  std::unordered_set<std::size_t> received_outputs_;
  std::vector<ENCRYPTO::BitVector<>> outputs_;
  std::unordered_map<std::size_t, std::unique_ptr<ENCRYPTO::FiberCondition>> output_conds_;
  std::mutex received_outputs_mutex_;

  // how many messages need to be sent from sender to receiver?
  // GOT -> 2
  // COT -> 1
  // ROT -> 0 (not in map)
  std::unordered_map<std::size_t, std::size_t> num_messages_;
  std::mutex num_messages_mutex_;

  // is an OT batch of XOR correlated OT?
  std::unordered_set<std::size_t> xor_correlation_;

  std::mutex bitlengths_mutex_;
  // bit length of every OT
  std::vector<std::size_t> bitlengths_;

  // real choices for every OT?
  std::unique_ptr<ENCRYPTO::BitVector<>> real_choices_;
  std::unordered_map<std::size_t, std::unique_ptr<ENCRYPTO::FiberCondition>> real_choices_cond_;

  // store the message types of new-style OTs
  std::unordered_map<std::size_t, OTMsgType> msg_type_;

  // Promises for the sender messages
  // ot_id -> (vector size, vector promise)
  std::unordered_map<std::size_t,
                     std::pair<std::size_t, ENCRYPTO::ReusableFiberPromise<ENCRYPTO::BitVector<>>>>
      message_promises_bit_;
  std::unordered_map<
      std::size_t,
      std::pair<std::size_t, ENCRYPTO::ReusableFiberPromise<ENCRYPTO::block128_vector>>>
      message_promises_block128_;

  template <typename T>
  using promise_map_t =
      std::unordered_map<std::size_t,
                         std::pair<std::size_t, ENCRYPTO::ReusableFiberPromise<std::vector<T>>>>;
  ENCRYPTO::type_map<promise_map_t, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
                     __uint128_t>
      message_promises_int_;

  // have we already set the choices for this OT batch?
  std::unordered_set<std::size_t> set_real_choices_;
  std::mutex real_choices_mutex_;

  // random choices from OT precomputation
  std::unique_ptr<ENCRYPTO::AlignedBitVector> random_choices_;

  // how many ots are in each batch?
  std::unordered_map<std::size_t, std::size_t> num_ots_in_batch_;

  // flag and condition variable: is setup is done?
  std::unique_ptr<ENCRYPTO::FiberCondition> setup_finished_cond_;
  std::atomic<bool> setup_finished_{false};

  std::atomic<std::size_t> consumed_offset_base_ots_{0};
  // XXX: unused
  std::atomic<std::size_t> consumed_offset_{0};
};

struct OTExtensionSenderData {
  OTExtensionSenderData();
  ~OTExtensionSenderData() = default;

  // width of the bit matrix
  std::atomic<std::size_t> bit_size_{0};

  /// receiver's mask that are needed to construct matrix @param V_
  std::array<ENCRYPTO::AlignedBitVector, 128> u_;

  std::array<ENCRYPTO::ReusableFiberPromise<std::size_t>, 128> u_promises_;
  std::array<ENCRYPTO::ReusableFiberFuture<std::size_t>, 128> u_futures_;
  std::mutex u_mutex_;
  std::size_t num_received_u_{0};
  // matrix of the OT extension scheme
  // XXX: can't we delete this after setup?
  std::shared_ptr<ENCRYPTO::BitMatrix> V_;

  // offset, num_ots
  std::unordered_map<std::size_t, std::size_t> num_ots_in_batch_;

  // corrections for GOTs, i.e., if random choice bit is not the real choice bit
  // send 1 to flip the messages before encoding or 0 otherwise for each GOT
  std::unordered_set<std::size_t> received_correction_offsets_;
  std::unordered_map<std::size_t, std::unique_ptr<ENCRYPTO::FiberCondition>>
      received_correction_offsets_cond_;
  ENCRYPTO::BitVector<> corrections_;
  mutable std::mutex corrections_mutex_;

  // random sender outputs
  // XXX: why not aligned?
  std::vector<ENCRYPTO::BitVector<>> y0_, y1_;

  // bit length of every OT
  std::vector<std::size_t> bitlengths_;

  // flag and condition variable: is setup is done?
  std::unique_ptr<ENCRYPTO::FiberCondition> setup_finished_cond_;
  std::atomic<bool> setup_finished_{false};

  std::atomic<std::size_t> consumed_offset_base_ots_{0};
  // XXX: unused
  std::atomic<std::size_t> consumed_offset_{0};
};

struct OTExtensionData {
  void MessageReceived(const std::uint8_t* message, std::size_t message_size,
                       const OTExtensionDataType type, const std::size_t ot_id = 0);

  OTExtensionReceiverData& GetReceiverData() { return receiver_data_; }
  const OTExtensionReceiverData& GetReceiverData() const { return receiver_data_; }
  OTExtensionSenderData& GetSenderData() { return sender_data_; }
  const OTExtensionSenderData& GetSenderData() const { return sender_data_; }

  OTExtensionReceiverData receiver_data_;
  OTExtensionSenderData sender_data_;
};

}  // namespace MOTION
