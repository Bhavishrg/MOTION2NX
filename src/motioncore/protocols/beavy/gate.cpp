// MIT License
//
// Copyright (c) 2020 Lennart Braun
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

#include <openssl/bn.h>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include "gate.h"

#include "base/gate_factory.h"
#include "beavy_provider.h"
#include "crypto/arithmetic_provider.h"
#include "crypto/motion_base_provider.h"
#include "crypto/oblivious_transfer/ot_flavors.h"
#include "crypto/oblivious_transfer/ot_provider.h"
#include "crypto/sharing_randomness_generator.h"
#include "utility/helpers.h"
#include "utility/logger.h"
#include "wire.h"

namespace MOTION::proto::beavy {

// Determine the total number of bits in a collection of wires.
static std::size_t count_bits(const BooleanBEAVYWireVector& wires) {
  return std::transform_reduce(std::begin(wires), std::end(wires), 0, std::plus<>(),
                               [](const auto& a) { return a->get_num_simd(); });
}

namespace detail {

BasicBooleanBEAVYBinaryGate::BasicBooleanBEAVYBinaryGate(std::size_t gate_id,
                                                         BooleanBEAVYWireVector&& in_b,
                                                         BooleanBEAVYWireVector&& in_a)
    : NewGate(gate_id),
      num_wires_(in_a.size()),
      inputs_a_(std::move(in_a)),
      inputs_b_(std::move(in_b)) {
  if (num_wires_ == 0) {
    throw std::logic_error("number of wires need to be positive");
  }
  if (num_wires_ != inputs_b_.size()) {
    throw std::logic_error("number of wires need to be the same for both inputs");
  }
  auto num_simd = inputs_a_[0]->get_num_simd();
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    if (inputs_a_[wire_i]->get_num_simd() != num_simd ||
        inputs_b_[wire_i]->get_num_simd() != num_simd) {
      throw std::logic_error("number of SIMD values need to be the same for all wires");
    }
  }
  outputs_.reserve(num_wires_);
  std::generate_n(std::back_inserter(outputs_), num_wires_,
                  [num_simd] { return std::make_shared<BooleanBEAVYWire>(num_simd); });
}

BasicBooleanBEAVYUnaryGate::BasicBooleanBEAVYUnaryGate(std::size_t gate_id,
                                                       BooleanBEAVYWireVector&& in, bool forward)
    : NewGate(gate_id), num_wires_(in.size()), inputs_(std::move(in)) {
  if (num_wires_ == 0) {
    throw std::logic_error("number of wires need to be positive");
  }
  auto num_simd = inputs_[0]->get_num_simd();
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    if (inputs_[wire_i]->get_num_simd() != num_simd) {
      throw std::logic_error("number of SIMD values need to be the same for all wires");
    }
  }
  if (forward) {
    outputs_ = inputs_;
  } else {
    outputs_.reserve(num_wires_);
    std::generate_n(std::back_inserter(outputs_), num_wires_,
                    [num_simd] { return std::make_shared<BooleanBEAVYWire>(num_simd); });
  }
}

}  // namespace detail

BooleanBEAVYInputGateSender::BooleanBEAVYInputGateSender(
    std::size_t gate_id, BEAVYProvider& beavy_provider, std::size_t num_wires, std::size_t num_simd,
    ENCRYPTO::ReusableFiberFuture<std::vector<ENCRYPTO::BitVector<>>>&& input_future)
    : NewGate(gate_id),
      beavy_provider_(beavy_provider),
      num_wires_(num_wires),
      num_simd_(num_simd),
      input_id_(beavy_provider.get_next_input_id(num_wires)),
      input_future_(std::move(input_future)) {
  outputs_.reserve(num_wires_);
  std::generate_n(std::back_inserter(outputs_), num_wires_,
                  [num_simd] { return std::make_shared<BooleanBEAVYWire>(num_simd); });
}

void BooleanBEAVYInputGateSender::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYInputGateSender::evaluate_setup start", gate_id_));
    }
  }

  auto my_id = beavy_provider_.get_my_id();
  auto num_parties = beavy_provider_.get_num_parties();
  auto& mbp = beavy_provider_.get_motion_base_provider();
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    auto& wire = outputs_[wire_i];
    wire->get_secret_share() = ENCRYPTO::BitVector<>::Random(num_simd_);
    wire->set_setup_ready();
    wire->get_public_share() = wire->get_secret_share();
    for (std::size_t party_id = 0; party_id < num_parties; ++party_id) {
      if (party_id == my_id) {
        continue;
      }
      auto& rng = mbp.get_my_randomness_generator(party_id);
      wire->get_public_share() ^= rng.GetBits(input_id_ + wire_i, num_simd_);
    }
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYInputGateSender::evaluate_setup end", gate_id_));
    }
  }
}

void BooleanBEAVYInputGateSender::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYInputGateSender::evaluate_online start", gate_id_));
    }
  }

  // wait for input value
  const auto inputs = input_future_.get();

  ENCRYPTO::BitVector<> public_shares;
  public_shares.Reserve(Helpers::Convert::BitsToBytes(num_wires_ * num_simd_));

  // compute my share
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    auto& w_o = outputs_[wire_i];
    auto& public_share = w_o->get_public_share();
    const auto& input_bits = inputs.at(wire_i);
    if (input_bits.GetSize() != num_simd_) {
      throw std::runtime_error("size of input bit vector != num_simd_");
    }
    public_share ^= input_bits;
    w_o->set_online_ready();
    public_shares.Append(public_share);
  }
  beavy_provider_.broadcast_bits_message(gate_id_, public_shares);

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYInputGateSender::evaluate_online end", gate_id_));
    }
  }
}

BooleanBEAVYInputGateReceiver::BooleanBEAVYInputGateReceiver(std::size_t gate_id,
                                                             BEAVYProvider& beavy_provider,
                                                             std::size_t num_wires,
                                                             std::size_t num_simd,
                                                             std::size_t input_owner)
    : NewGate(gate_id),
      beavy_provider_(beavy_provider),
      num_wires_(num_wires),
      num_simd_(num_simd),
      input_owner_(input_owner),
      input_id_(beavy_provider.get_next_input_id(num_wires)) {
  outputs_.reserve(num_wires_);
  std::generate_n(std::back_inserter(outputs_), num_wires_,
                  [num_simd] { return std::make_shared<BooleanBEAVYWire>(num_simd); });
  public_share_future_ =
      beavy_provider_.register_for_bits_message(input_owner_, gate_id_, num_wires * num_simd);
}

void BooleanBEAVYInputGateReceiver::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYInputGateReceiver::evaluate_setup start", gate_id_));
    }
  }

  auto& mbp = beavy_provider_.get_motion_base_provider();
  auto& rng = mbp.get_their_randomness_generator(input_owner_);
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    auto& wire = outputs_[wire_i];
    wire->get_secret_share() = rng.GetBits(input_id_ + wire_i, num_simd_);
    wire->set_setup_ready();
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYInputGateReceiver::evaluate_setup end", gate_id_));
    }
  }
}

void BooleanBEAVYInputGateReceiver::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYInputGateReceiver::evaluate_online start", gate_id_));
    }
  }

  auto public_shares = public_share_future_.get();
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    auto& wire = outputs_[wire_i];
    wire->get_public_share() = public_shares.Subset(wire_i * num_simd_, (wire_i + 1) * num_simd_);
    wire->set_online_ready();
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYInputGateReceiver::evaluate_online end", gate_id_));
    }
  }
}

BooleanBEAVYOutputGate::BooleanBEAVYOutputGate(std::size_t gate_id, BEAVYProvider& beavy_provider,
                                               BooleanBEAVYWireVector&& inputs,
                                               std::size_t output_owner)
    : NewGate(gate_id),
      beavy_provider_(beavy_provider),
      num_wires_(inputs.size()),
      output_owner_(output_owner),
      inputs_(std::move(inputs)) {
  std::size_t my_id = beavy_provider_.get_my_id();
  auto num_bits = count_bits(inputs_);
  if (output_owner_ == ALL_PARTIES || output_owner_ == my_id) {
    share_futures_ = beavy_provider_.register_for_bits_messages(gate_id_, num_bits);
  }
  my_secret_share_.Reserve(Helpers::Convert::BitsToBytes(num_bits));
}

ENCRYPTO::ReusableFiberFuture<std::vector<ENCRYPTO::BitVector<>>>
BooleanBEAVYOutputGate::get_output_future() {
  std::size_t my_id = beavy_provider_.get_my_id();
  if (output_owner_ == ALL_PARTIES || output_owner_ == my_id) {
    return output_promise_.get_future();
  } else {
    throw std::logic_error("not this parties output");
  }
}

void BooleanBEAVYOutputGate::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYOutputGate::evaluate_setup start", gate_id_));
    }
  }

  for (const auto& wire : inputs_) {
    wire->wait_setup();
    my_secret_share_.Append(wire->get_secret_share());
  }
  std::size_t my_id = beavy_provider_.get_my_id();
  if (output_owner_ != my_id) {
    if (output_owner_ == ALL_PARTIES) {
      beavy_provider_.broadcast_bits_message(gate_id_, my_secret_share_);
    } else {
      beavy_provider_.send_bits_message(output_owner_, gate_id_, my_secret_share_);
    }
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYOutputGate::evaluate_setup end", gate_id_));
    }
  }
}

void BooleanBEAVYOutputGate::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYOutputGate::evaluate_online start", gate_id_));
    }
  }

  std::size_t my_id = beavy_provider_.get_my_id();
  if (output_owner_ == ALL_PARTIES || output_owner_ == my_id) {
    std::size_t num_parties = beavy_provider_.get_num_parties();
    for (std::size_t party_id = 0; party_id < num_parties; ++party_id) {
      if (party_id == my_id) {
        continue;
      }
      const auto other_share = share_futures_[party_id].get();
      my_secret_share_ ^= other_share;
    }
    std::vector<ENCRYPTO::BitVector<>> outputs;
    outputs.reserve(num_wires_);
    std::size_t bit_offset = 0;
    for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
      auto num_simd = inputs_[wire_i]->get_num_simd();
      auto& output =
          outputs.emplace_back(my_secret_share_.Subset(bit_offset, bit_offset + num_simd));
      inputs_[wire_i]->wait_online();
      output ^= inputs_[wire_i]->get_public_share();
      bit_offset += num_simd;
    }
    output_promise_.set_value(std::move(outputs));
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYOutputGate::evaluate_online end", gate_id_));
    }
  }
}

BooleanBEAVYINVGate::BooleanBEAVYINVGate(std::size_t gate_id, const BEAVYProvider& beavy_provider,
                                         BooleanBEAVYWireVector&& in)
    : detail::BasicBooleanBEAVYUnaryGate(gate_id, std::move(in),
                                         !beavy_provider.is_my_job(gate_id)),
      is_my_job_(beavy_provider.is_my_job(gate_id)) {}

void BooleanBEAVYINVGate::evaluate_setup() {
  if (!is_my_job_) {
    return;
  }

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& w_in = inputs_[wire_i];
    w_in->wait_setup();
    auto& w_o = outputs_[wire_i];
    w_o->get_secret_share() = ~w_in->get_secret_share();
    w_o->set_setup_ready();
  }
}

void BooleanBEAVYINVGate::evaluate_online() {
  if (!is_my_job_) {
    return;
  }

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& w_in = inputs_[wire_i];
    w_in->wait_online();
    auto& w_o = outputs_[wire_i];
    w_o->get_public_share() = w_in->get_public_share();
    w_o->set_online_ready();
  }
}

template <typename T>
BEAVYAHAMGate<T>::BEAVYAHAMGate(std::size_t gate_id, BEAVYProvider& beavy_provider,
                                         ArithmeticBEAVYWireP<T>&& in)
    : NewGate(gate_id), input_(std::move(in)), beavy_provider_(beavy_provider) {
  
    std::size_t num_simd = input_->get_num_simd();
    std::size_t num_ri = ENCRYPTO::bit_size_v<T>;
    std::size_t my_id = beavy_provider_.get_my_id();

    bit2a_gates_.resize(num_ri);
    arithmetic_wires_.resize(num_ri);

    for (int i = 0; i < num_ri; i++) {
      boolean_wires_.push_back(std::make_shared<BooleanBEAVYWire>(num_simd));
    }

    beavy_arithmetic_wires_.resize(num_ri);
    for (int i = 0; i < num_ri; i++) {
      auto gate_wires = beavy_provider.
      external_make_convert_bit_to_arithmetic_beavy_gate<std::uint64_t>(boolean_wires_[i]);
      bit2a_gates_[i] = std::move(gate_wires.first);
      arithmetic_wires_[i] = std::move(gate_wires.second);
      assert(arithmetic_wires_[i].size() == 1); // Wire vector size should be 1.
      auto arith_wire_p = dynamic_pointer_cast<ArithmeticBEAVYWire<T>>(arithmetic_wires_[i][0]);
      beavy_arithmetic_wires_[i].push_back({arith_wire_p});
      assert(beavy_arithmetic_wires_[i].size() == 1);
    }

    output_ = std::make_shared<ArithmeticBEAVYWire<T>>(num_simd);
    share_future_ = beavy_provider_.register_for_bits_message(1 - my_id, gate_id_, num_simd);
  }

template <typename T>
void BEAVYAHAMGate<T>::evaluate_setup_with_context(ExecutionContext& context) {
  std::size_t num_simd = input_->get_num_simd();
  std::size_t my_id = beavy_provider_.get_my_id();

  // Make wires!
  std::size_t num_ri = ENCRYPTO::bit_size_v<T>;
  random_values_.resize(num_ri);
  
  for (int i = 0; i < num_ri; i++) {
    random_values_[i] = ENCRYPTO::BitVector<>::Random(num_simd);
    boolean_wires_[i]->get_public_share() = ENCRYPTO::BitVector<>(num_simd);
    // TODO(pranav): Make sure that the above value is all zero bits.
    boolean_wires_[i]->get_secret_share() = random_values_[i];
    boolean_wires_[i]->set_setup_ready();
    boolean_wires_[i]->set_online_ready();
  }

  // Run the Bit2A protocol!
  // TODO(pranav): Change this to parallel running.
  for (int i = 0; i < num_ri; i++) {
    bit2a_gates_[i]->evaluate_setup();
  }

  for (int i = 0; i < num_ri; i++) {
    beavy_arithmetic_wires_[i][0]->wait_setup();
    bit2a_gates_[i]->evaluate_online();
  }

  for (int i = 0; i < num_ri; i++) {
    beavy_arithmetic_wires_[i][0]->wait_online();
  }

  
  // for (int i = 0; i < num_ri; i++){
  //   r->get_public_share() += (beavy_arithmetic_wires_[i][0]->get_public_share());
  //   r->get_secret_share() += (beavy_arithmetic_wires_[i][0]->get_secret_share());
  // }

  // ENCRYPTO::BitVector<> for_other_party;
  // for (int i = 0; i < this->num_wires_; i++) {
  //   this->input_[i]->wait_setup();
  //   auto tmp = (this->input_[i]->get_secret_share()) ^ random_values_[i]; // simd = num_simd;
  //   for_other_party.Append(tmp);
  // }
  // beavy_provider_.send_bits_message(1 - my_id, gate_id_, for_other_party);
  // auto other_party_share = share_future_.get();

  // for (int i = 0; i < this->num_wires_; i++) {
  //   auto myy = for_other_party.Subset(i * num_simd, (i + 1) * num_simd);
  //   auto otherr = other_party_share.Subset(i * num_simd, (i + 1) * num_simd);
  //   public_bits_.push_back(myy ^ otherr);
  // }

  // assert(public_bits_.size() == this->num_wires_);

  output_->get_secret_share() = Helpers::RandomVector<T>(num_simd);
  output_->set_setup_ready();
}

template <typename T>
void BEAVYAHAMGate<T>::evaluate_online_with_context(ExecutionContext& context) {
  std::size_t num_simd = input_->get_num_simd();
  std::size_t my_id = beavy_provider_.get_my_id();
  std::size_t num_ri = ENCRYPTO::bit_size_v<T>;
  this->output_->wait_setup();

  // std::vector<ENCRYPTO::BitVector<>> a;
  // for (int i = 0; i < this->num_wires_; i++) {
  //   this->input_[i]->wait_online();
  //   auto public_value = this->input_[i]->get_public_share();
  //   a.push_back(public_value ^ public_bits_[i]);
  //   #pragma omp parallel for
  //   for (int j = 0; j < num_simd; ++j) {
  //     uint64_t ai = a.back().Get(j);
  //     if (my_id == 0) {
  //       if (i == 0) {
  //         output_->get_public_share()[j] = ai + beavy_arithmetic_wires_[i][0]
  //         ->get_public_share()[j] - 2*ai*beavy_arithmetic_wires_[i][0]->get_public_share()[j];
  //       } else {
  //         output_->get_public_share()[j] += ai + beavy_arithmetic_wires_[i][0]
  //         ->get_public_share()[j] - 2*ai*beavy_arithmetic_wires_[i][0]->get_public_share()[j];
  //       }
  //     }
  //     if (i == 0) {
  //       output_->get_public_share()[j] = beavy_arithmetic_wires_[i][0]->get_secret_share()[j] * (2*ai - 1);
  //     } else {
  //       output_->get_public_share()[j] += beavy_arithmetic_wires_[i][0]->get_secret_share()[j] * (2*ai - 1);
  //     }
  //   }
  // }
  output_->get_public_share() = Helpers::RandomVector<T>(num_simd);
  // IMPORTANT: output_ public shares are set. They are different for both the parties!
  output_->set_online_ready();
}

template <typename T>
void BEAVYAHAMGate<T>::evaluate_setup() {
    throw std::logic_error("BEAVYAHAMGate::evaluate_setup() not implemented");
}

template <typename T>
void BEAVYAHAMGate<T>::evaluate_online() {
    throw std::logic_error("BEAVYAHAMGate::evaluate_online() not implemented");
}

template class BEAVYAHAMGate<std::uint8_t>;
template class BEAVYAHAMGate<std::uint16_t>;
template class BEAVYAHAMGate<std::uint32_t>;
template class BEAVYAHAMGate<std::uint64_t>;

template <typename T>
BooleanBEAVYHAMGate<T>::BooleanBEAVYHAMGate(std::size_t gate_id, BEAVYProvider& beavy_provider,
                                         BooleanBEAVYWireVector&& in)
    : NewGate(gate_id), input_(std::move(in)), beavy_provider_(beavy_provider) {
    std::size_t num_simd = input_[0]->get_num_simd();
    this->num_wires_ = input_.size();
    std::size_t my_id = beavy_provider_.get_my_id();
    bit2a_gates_.resize(this->num_wires_);
    arithmetic_wires_.resize(this->num_wires_);
    for (int i = 0; i < this->num_wires_; i++) {
      boolean_wires_.push_back(std::make_shared<BooleanBEAVYWire>(num_simd));
    }
    beavy_arithmetic_wires_.resize(this->num_wires_);
    for (int i = 0; i < this->num_wires_; i++) {
      auto gate_wires = beavy_provider.
      external_make_convert_bit_to_arithmetic_beavy_gate<std::uint64_t>(boolean_wires_[i]);
      bit2a_gates_[i] = std::move(gate_wires.first);
      arithmetic_wires_[i] = std::move(gate_wires.second);
      assert(arithmetic_wires_[i].size() == 1); // Wire vector size should be 1.
      auto arith_wire_p = dynamic_pointer_cast<ArithmeticBEAVYWire<T>>(arithmetic_wires_[i][0]);
      beavy_arithmetic_wires_[i].push_back({arith_wire_p});
      assert(beavy_arithmetic_wires_[i].size() == 1);
    }

    output_ = std::make_shared<ArithmeticBEAVYWire<T>>(num_simd);
    share_future_ = beavy_provider_.register_for_bits_message(1 - my_id, gate_id_, num_wires_ * num_simd);
  }

template <typename T>
void BooleanBEAVYHAMGate<T>::evaluate_setup_with_context(ExecutionContext& context) {
  std::size_t num_simd = input_[0]->get_num_simd();
  std::size_t my_id = beavy_provider_.get_my_id();

  // Make wires!

  random_values_.resize(this->num_wires_);

  for (int i = 0; i < this->num_wires_; i++) {
    random_values_[i] = ENCRYPTO::BitVector<>::Random(num_simd);
    boolean_wires_[i]->get_public_share() = ENCRYPTO::BitVector<>(num_simd);
    // TODO(pranav): Make sure that the above value is all zero bits.
    boolean_wires_[i]->get_secret_share() = random_values_[i];
    boolean_wires_[i]->set_setup_ready();
    boolean_wires_[i]->set_online_ready();
  }

  // Run the Bit2A protocol!
  // TODO(pranav): Change this to parallel running.
  for (int i = 0; i < this->num_wires_; i++) {
    bit2a_gates_[i]->evaluate_setup();
  }

  for (int i = 0; i < this->num_wires_; i++) {
    beavy_arithmetic_wires_[i][0]->wait_setup();
    bit2a_gates_[i]->evaluate_online();
  }


  for (int i = 0; i < this->num_wires_; i++) {
    beavy_arithmetic_wires_[i][0]->wait_online();
  }
  ENCRYPTO::BitVector<> for_other_party;
  for (int i = 0; i < this->num_wires_; i++) {
    this->input_[i]->wait_setup();
    auto tmp = (this->input_[i]->get_secret_share()) ^ random_values_[i]; // simd = num_simd;
    for_other_party.Append(tmp);
  }
  beavy_provider_.send_bits_message(1 - my_id, gate_id_, for_other_party);
  auto other_party_share = share_future_.get();

  for (int i = 0; i < this->num_wires_; i++) {
    auto myy = for_other_party.Subset(i * num_simd, (i + 1) * num_simd);
    auto otherr = other_party_share.Subset(i * num_simd, (i + 1) * num_simd);
    public_bits_.push_back(myy ^ otherr);
  }

  assert(public_bits_.size() == this->num_wires_);

  output_->get_secret_share() = Helpers::RandomVector<T>(num_simd);
  output_->set_setup_ready();
}

template <typename T>
void BooleanBEAVYHAMGate<T>::evaluate_online_with_context(ExecutionContext& context) {
  std::size_t num_simd = input_[0]->get_num_simd();
  std::size_t my_id = beavy_provider_.get_my_id();

  this->output_->wait_setup();

  std::vector<ENCRYPTO::BitVector<>> a;
  for (int i = 0; i < this->num_wires_; i++) {
    this->input_[i]->wait_online();
    auto public_value = this->input_[i]->get_public_share();
    a.push_back(public_value ^ public_bits_[i]);
    #pragma omp parallel for
    for (int j = 0; j < num_simd; ++j) {
      uint64_t ai = a.back().Get(j);
      if (my_id == 0) {
        if (i == 0) {
          output_->get_public_share()[j] = ai + beavy_arithmetic_wires_[i][0]
          ->get_public_share()[j] - 2*ai*beavy_arithmetic_wires_[i][0]->get_public_share()[j];
        } else {
          output_->get_public_share()[j] += ai + beavy_arithmetic_wires_[i][0]
          ->get_public_share()[j] - 2*ai*beavy_arithmetic_wires_[i][0]->get_public_share()[j];
        }
      }
      if (i == 0) {
        output_->get_public_share()[j] = beavy_arithmetic_wires_[i][0]->get_secret_share()[j] * (2*ai - 1);
      } else {
        output_->get_public_share()[j] += beavy_arithmetic_wires_[i][0]->get_secret_share()[j] * (2*ai - 1);
      }
    }
  }
  // IMPORTANT: output_ public shares are set. They are different for both the parties!
  output_->set_online_ready();
}

template <typename T>
void BooleanBEAVYHAMGate<T>::evaluate_setup() {
    throw std::logic_error("BooleanBEAVYHAMGate::evaluate_setup() not implemented");
}

template <typename T>
void BooleanBEAVYHAMGate<T>::evaluate_online() {
    throw std::logic_error("BooleanBEAVYHAMGate::evaluate_online() not implemented");
}

template class BooleanBEAVYHAMGate<std::uint8_t>;
template class BooleanBEAVYHAMGate<std::uint16_t>;
template class BooleanBEAVYHAMGate<std::uint32_t>;
template class BooleanBEAVYHAMGate<std::uint64_t>;

template <typename T>
BooleanBEAVYCOUNTGate<T>::BooleanBEAVYCOUNTGate(std::size_t gate_id,
                                                              BEAVYProvider& beavy_provider,
                                                              BooleanBEAVYWireVector&& in)
    : NewGate(gate_id), inputs_(std::move(in)), beavy_provider_(beavy_provider) {
  const auto num_wires = inputs_.size();
  // if (num_wires != ENCRYPTO::bit_size_v<T>) {
  //   throw std::logic_error("number of wires need to be equal to bit size of T");
  // }
  const auto num_simd = inputs_.at(0)->get_num_simd();
  output_ = std::make_shared<beavy::ArithmeticBEAVYWire<T>>(num_simd);
  const auto my_id = beavy_provider_.get_my_id();
  auto& ot_provider = beavy_provider_.get_ot_manager().get_provider(1 - my_id);
  if (my_id == 0) {
    ot_sender_ = ot_provider.RegisterSendACOT<T>(num_wires * num_simd);
  } else {
    assert(my_id == 1);
    ot_receiver_ = ot_provider.RegisterReceiveACOT<T>(num_wires * num_simd);
  }
  share_future_ = beavy_provider_.register_for_ints_message<T>(1 - my_id, gate_id_, num_simd);
  

}

template <typename T>
BooleanBEAVYCOUNTGate<T>::~BooleanBEAVYCOUNTGate() = default;

template <typename T>
void BooleanBEAVYCOUNTGate<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYCOUNTGate<T>::evaluate_setup start", gate_id_));
    }
  }
  std::cout<< "count setup" << std::endl;
  const auto num_wires = inputs_.size();
  const auto num_simd = output_->get_num_simd();

  output_->get_secret_share() = Helpers::RandomVector<T>(num_simd);
  output_->set_setup_ready();

  std::vector<T> ot_output;
  if (ot_sender_ != nullptr) {
    std::vector<T> correlations(num_wires * num_simd);
    for (std::size_t wire_i = 0; wire_i < num_wires; ++wire_i) {
      const auto& wire_in = inputs_[wire_i];
      wire_in->wait_setup();
      const auto& secret_share = wire_in->get_secret_share();
      for (std::size_t simd_j = 0; simd_j < num_simd; ++simd_j) {
        if (secret_share.Get(simd_j)) {
          correlations[wire_i * num_simd + simd_j] = 1;
        }
      }
    }
    ot_sender_->SetCorrelations(std::move(correlations));
    ot_sender_->SendMessages();
    ot_sender_->ComputeOutputs();
    ot_output = ot_sender_->GetOutputs();
    for (std::size_t wire_i = 0; wire_i < num_wires; ++wire_i) {
      const auto& secret_share = inputs_[wire_i]->get_secret_share();
      for (std::size_t simd_j = 0; simd_j < num_simd; ++simd_j) {
        T bit = secret_share.Get(simd_j);
        ot_output[wire_i * num_simd + simd_j] = bit + 2 * ot_output[wire_i * num_simd + simd_j];
      }
    }
  } else {
    assert(ot_receiver_ != nullptr);
    ENCRYPTO::BitVector<> choices;
    choices.Reserve(Helpers::Convert::BitsToBytes(num_wires * num_simd));
    for (std::size_t wire_i = 0; wire_i < num_wires; ++wire_i) {
      const auto& wire_in = inputs_[wire_i];
      wire_in->wait_setup();
      choices.Append(wire_in->get_secret_share());
    }
    ot_receiver_->SetChoices(std::move(choices));
    ot_receiver_->SendCorrections();
    ot_receiver_->ComputeOutputs();
    ot_output = ot_receiver_->GetOutputs();
    for (std::size_t wire_i = 0; wire_i < num_wires; ++wire_i) {
      const auto& secret_share = inputs_[wire_i]->get_secret_share();
      for (std::size_t simd_j = 0; simd_j < num_simd; ++simd_j) {
        T bit = secret_share.Get(simd_j);
        ot_output[wire_i * num_simd + simd_j] = bit - 2 * ot_output[wire_i * num_simd + simd_j];
      }
    }
  }
  std::cout<<"wefwefd"<<std::endl;
  arithmetized_secret_share_ = std::move(ot_output);
  
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYCOUNTGate<T>::evaluate_setup end", gate_id_));
    }
  }
}

template <typename T>
void BooleanBEAVYCOUNTGate<T>::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYCOUNTGate<T>::evaluate_online start", gate_id_));
    }
  }
  std::cout<< "count online" << std::endl;
  const auto num_wires = inputs_.size();
  const auto num_simd = output_->get_num_simd();
  const auto my_id = beavy_provider_.get_my_id();
  std::vector<T> arithmetized_public_share(num_wires * num_simd);

  for (std::size_t wire_i = 0; wire_i < num_wires; ++wire_i) {
    const auto& wire_in = inputs_[wire_i];
    wire_in->wait_online();
    const auto& public_share = wire_in->get_public_share();
    for (std::size_t simd_j = 0; simd_j < num_simd; ++simd_j) {
      if (public_share.Get(simd_j)) {
        arithmetized_public_share[wire_i * num_simd + simd_j] = 1;
      }
    }
  }

  auto tmp = output_->get_secret_share();
  if (beavy_provider_.is_my_job(gate_id_)) {
    for (std::size_t wire_i = 0; wire_i < num_wires; ++wire_i) {
      for (std::size_t simd_j = 0; simd_j < num_simd; ++simd_j) {
        const auto p = arithmetized_public_share[wire_i * num_simd + simd_j];
        const auto s = arithmetized_secret_share_[wire_i * num_simd + simd_j];
        tmp[simd_j] += (p + (1 - 2 * p) * s);
      }
    }
  } else {
    for (std::size_t wire_i = 0; wire_i < num_wires; ++wire_i) {
      for (std::size_t simd_j = 0; simd_j < num_simd; ++simd_j) {
        const auto p = arithmetized_public_share[wire_i * num_simd + simd_j];
        const auto s = arithmetized_secret_share_[wire_i * num_simd + simd_j];
        tmp[simd_j] += ((1 - 2 * p) * s);
      }
    }
  }
  beavy_provider_.send_ints_message(1 - my_id, gate_id_, tmp);
  const auto other_share = share_future_.get();
  std::transform(std::begin(tmp), std::end(tmp), std::begin(other_share), std::begin(tmp),
                 std::plus{});
  output_->get_public_share() = std::move(tmp);
  output_->set_online_ready();
  
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: BooleanBEAVYCOUNTGate<T>::evaluate_online end", gate_id_));
    }
  }
}

template class BooleanBEAVYCOUNTGate<std::uint8_t>;
template class BooleanBEAVYCOUNTGate<std::uint16_t>;
template class BooleanBEAVYCOUNTGate<std::uint32_t>;
template class BooleanBEAVYCOUNTGate<std::uint64_t>;

BooleanBEAVYXORGate::BooleanBEAVYXORGate(std::size_t gate_id, BEAVYProvider&,
                                         BooleanBEAVYWireVector&& in_a,
                                         BooleanBEAVYWireVector&& in_b)
    : detail::BasicBooleanBEAVYBinaryGate(gate_id, std::move(in_a), std::move(in_b)) {}

void BooleanBEAVYXORGate::evaluate_setup() {
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& w_a = inputs_a_[wire_i];
    const auto& w_b = inputs_b_[wire_i];
    w_a->wait_setup();
    w_b->wait_setup();
    auto& w_o = outputs_[wire_i];
    w_o->get_secret_share() = w_a->get_secret_share() ^ w_b->get_secret_share();
    w_o->set_setup_ready();
  }
}

void BooleanBEAVYXORGate::evaluate_online() {
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& w_a = inputs_a_[wire_i];
    const auto& w_b = inputs_b_[wire_i];
    w_a->wait_online();
    w_b->wait_online();
    auto& w_o = outputs_[wire_i];
    w_o->get_public_share() = w_a->get_public_share() ^ w_b->get_public_share();
    w_o->set_online_ready();
  }
}

BooleanBEAVYANDGate::BooleanBEAVYANDGate(std::size_t gate_id, BEAVYProvider& beavy_provider,
                                         BooleanBEAVYWireVector&& in_a,
                                         BooleanBEAVYWireVector&& in_b)
    : detail::BasicBooleanBEAVYBinaryGate(gate_id, std::move(in_a), std::move(in_b)),
      beavy_provider_(beavy_provider),
      ot_sender_(nullptr),
      ot_receiver_(nullptr) {
  auto num_bits = count_bits(inputs_a_);
  auto my_id = beavy_provider_.get_my_id();
  share_future_ = beavy_provider_.register_for_bits_message(1 - my_id, gate_id_, num_bits);
  auto& otp = beavy_provider_.get_ot_manager().get_provider(1 - my_id);
  ot_sender_ = otp.RegisterSendXCOTBit(num_bits);
  ot_receiver_ = otp.RegisterReceiveXCOTBit(num_bits);
}

BooleanBEAVYANDGate::~BooleanBEAVYANDGate() = default;

void BooleanBEAVYANDGate::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYANDGate::evaluate_setup start", gate_id_));
    }
  }

  for (auto& wire_o : outputs_) {
    wire_o->get_secret_share() = ENCRYPTO::BitVector<>::Random(wire_o->get_num_simd());
    wire_o->set_setup_ready();
  }

  auto num_simd = inputs_a_[0]->get_num_simd();
  std::cout<< "num_simd: " << num_simd << std::endl;
  auto num_bytes = Helpers::Convert::BitsToBytes(num_wires_ * num_simd); // what is this?
  delta_a_share_.Reserve(num_bytes);
  delta_b_share_.Reserve(num_bytes);
  Delta_y_share_.Reserve(num_bytes);

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& wire_a = inputs_a_[wire_i];
    const auto& wire_b = inputs_b_[wire_i];
    const auto& wire_o = outputs_[wire_i];
    wire_a->wait_setup();
    wire_b->wait_setup();
    delta_a_share_.Append(wire_a->get_secret_share());
    delta_b_share_.Append(wire_b->get_secret_share());
    Delta_y_share_.Append(wire_o->get_secret_share());
  }

  auto delta_ab_share = delta_a_share_ & delta_b_share_;

  ot_receiver_->SetChoices(delta_a_share_);
  ot_receiver_->SendCorrections();
  ot_sender_->SetCorrelations(delta_b_share_);
  ot_sender_->SendMessages();
  ot_receiver_->ComputeOutputs();
  ot_sender_->ComputeOutputs();
  delta_ab_share ^= ot_sender_->GetOutputs();
  delta_ab_share ^= ot_receiver_->GetOutputs();
  Delta_y_share_ ^= delta_ab_share;

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYANDGate::evaluate_setup end", gate_id_));
    }
  }
}

void BooleanBEAVYANDGate::evaluate_online() {
  auto num_simd = inputs_a_[0]->get_num_simd();
  auto num_bits = num_wires_ * num_simd;
  ENCRYPTO::BitVector<> Delta_a;
  ENCRYPTO::BitVector<> Delta_b;
  Delta_a.Reserve(Helpers::Convert::BitsToBytes(num_bits));
  Delta_b.Reserve(Helpers::Convert::BitsToBytes(num_bits));

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& wire_a = inputs_a_[wire_i];
    wire_a->wait_online();
    Delta_a.Append(wire_a->get_public_share());
    const auto& wire_b = inputs_b_[wire_i];
    wire_b->wait_online();
    Delta_b.Append(wire_b->get_public_share());
  }

  Delta_y_share_ ^= (Delta_a & delta_b_share_);
  Delta_y_share_ ^= (Delta_b & delta_a_share_);

  if (beavy_provider_.is_my_job(gate_id_)) {
    Delta_y_share_ ^= (Delta_a & Delta_b);
  }

  beavy_provider_.broadcast_bits_message(gate_id_, Delta_y_share_);
  Delta_y_share_ ^= share_future_.get();

  // distribute data among wires
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    auto& wire_o = outputs_[wire_i];
    wire_o->get_public_share() = Delta_y_share_.Subset(wire_i * num_simd, (wire_i + 1) * num_simd);
    wire_o->set_online_ready();
  }
}

BooleanBEAVYAND4Gate::BooleanBEAVYAND4Gate(std::size_t gate_id, BEAVYProvider& beavy_provider,
                                         BooleanBEAVYWireVector&& in_a,
                                         BooleanBEAVYWireVector&& in_b)
    : detail::BasicBooleanBEAVYBinaryGate(gate_id, std::move(in_a), std::move(in_b)),
      beavy_provider_(beavy_provider),
      ot_sender_(nullptr),
      ot_receiver_(nullptr) {
  auto num_bits = count_bits(inputs_a_);
  auto my_id = beavy_provider_.get_my_id();
  share_future_ = beavy_provider_.register_for_bits_message(1 - my_id, gate_id_, num_bits);
  auto& otp = beavy_provider_.get_ot_manager().get_provider(1 - my_id);
  ot_sender_ = otp.RegisterSendXCOTBit(num_bits);
  ot_receiver_ = otp.RegisterReceiveXCOTBit(num_bits);
}

BooleanBEAVYAND4Gate::~BooleanBEAVYAND4Gate() = default;

void BooleanBEAVYAND4Gate::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYAND4Gate::evaluate_setup start", gate_id_));
    }
  }

  for (auto& wire_o : outputs_) {
    wire_o->get_secret_share() = ENCRYPTO::BitVector<>::Random(wire_o->get_num_simd());
    wire_o->set_setup_ready();
  }

  auto num_simd = inputs_a_[0]->get_num_simd();
  std::cout<< "num_simd: " << num_simd << std::endl;
  auto num_bytes = Helpers::Convert::BitsToBytes(num_wires_ * num_simd); // what is this?
  delta_a_share_.Reserve(num_bytes);
  delta_b_share_.Reserve(num_bytes);
  Delta_y_share_.Reserve(num_bytes);

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& wire_a = inputs_a_[wire_i];
    const auto& wire_b = inputs_b_[wire_i];
    const auto& wire_o = outputs_[wire_i];
    wire_a->wait_setup();
    wire_b->wait_setup();
    delta_a_share_.Append(wire_a->get_secret_share());
    delta_b_share_.Append(wire_b->get_secret_share());
    Delta_y_share_.Append(wire_o->get_secret_share());
  }

  auto delta_ab_share = delta_a_share_ & delta_b_share_;

  ot_receiver_->SetChoices(delta_a_share_);
  ot_receiver_->SendCorrections();
  ot_sender_->SetCorrelations(delta_b_share_);
  ot_sender_->SendMessages();
  ot_receiver_->ComputeOutputs();
  ot_sender_->ComputeOutputs();
  delta_ab_share ^= ot_sender_->GetOutputs();
  delta_ab_share ^= ot_receiver_->GetOutputs();
  Delta_y_share_ ^= delta_ab_share;

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYAND4Gate::evaluate_setup end", gate_id_));
    }
  }
}

void BooleanBEAVYAND4Gate::evaluate_online() {
  auto num_simd = inputs_a_[0]->get_num_simd();
  auto num_bits = num_wires_ * num_simd;
  ENCRYPTO::BitVector<> Delta_a;
  ENCRYPTO::BitVector<> Delta_b;
  Delta_a.Reserve(Helpers::Convert::BitsToBytes(num_bits));
  Delta_b.Reserve(Helpers::Convert::BitsToBytes(num_bits));

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& wire_a = inputs_a_[wire_i];
    wire_a->wait_online();
    Delta_a.Append(wire_a->get_public_share());
    const auto& wire_b = inputs_b_[wire_i];
    wire_b->wait_online();
    Delta_b.Append(wire_b->get_public_share());
  }

  Delta_y_share_ ^= (Delta_a & delta_b_share_);
  Delta_y_share_ ^= (Delta_b & delta_a_share_);

  if (beavy_provider_.is_my_job(gate_id_)) {
    Delta_y_share_ ^= (Delta_a & Delta_b);
  }

  beavy_provider_.broadcast_bits_message(gate_id_, Delta_y_share_);
  Delta_y_share_ ^= share_future_.get();

  // distribute data among wires
  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    auto& wire_o = outputs_[wire_i];
    wire_o->get_public_share() = Delta_y_share_.Subset(wire_i * num_simd, (wire_i + 1) * num_simd);
    wire_o->set_online_ready();
  }
}


BooleanBEAVYMSGGate::BooleanBEAVYMSGGate(std::size_t gate_id, BEAVYProvider& beavy_provider,
                                         BooleanBEAVYWireVector&& in_a,
                                         BooleanBEAVYWireVector&& in_b)
    : detail::BasicBooleanBEAVYBinaryGate(gate_id, std::move(in_a), std::move(in_b)),
      beavy_provider_(beavy_provider),
      ot_sender_(nullptr),
      ot_receiver_(nullptr) {
  
  num_wires_ = 10;
  auto num_simd = 991;
  auto rounds = 2;
  auto num_bits = num_simd*num_wires_;

  // auto num_bits = count_bits(inputs_a_);
  auto my_id = beavy_provider_.get_my_id();
  
  auto& otp = beavy_provider_.get_ot_manager().get_provider(1 - my_id);
  ot_sender_ = otp.RegisterSendXCOTBit(num_bits);
  ot_receiver_ = otp.RegisterReceiveXCOTBit(num_bits);
}

BooleanBEAVYMSGGate::~BooleanBEAVYMSGGate() = default;

void BooleanBEAVYMSGGate::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYMSGGate::evaluate_setup start", gate_id_));
    }
  }

  for (auto& wire_o : outputs_) {
    wire_o->get_secret_share() = ENCRYPTO::BitVector<>::Random(wire_o->get_num_simd());
    wire_o->set_setup_ready();
  }

  num_wires_ = 10;
  auto num_simd = 991;
  auto rounds = 2;
  auto num_bits = num_simd*num_wires_;

  auto num_bytes = Helpers::Convert::BitsToBytes(num_wires_ * num_simd);
  
  Delta_y_share_.Reserve(num_bytes);
  auto rand = ENCRYPTO::BitVector<>(num_wires_ * num_simd, false);
  auto randa = ENCRYPTO::BitVector<>(num_wires_ * num_simd, false);
  auto randb = ENCRYPTO::BitVector<>(num_wires_ * num_simd, false);
  delta_a_share_.Reserve(num_bytes);
  delta_b_share_.Reserve(num_bytes);

  Delta_y_share_.Append(rand);
  delta_a_share_.Append(randa);
  delta_b_share_.Append(randb);
  ot_receiver_->SetChoices(delta_a_share_);
  ot_receiver_->SendCorrections();
  ot_sender_->SetCorrelations(delta_b_share_);
  ot_sender_->SendMessages();
  ot_receiver_->ComputeOutputs();
  ot_sender_->ComputeOutputs();
  Delta_y_share_ ^= ot_sender_->GetOutputs();
  Delta_y_share_ ^= ot_receiver_->GetOutputs();
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYMSGGate::evaluate_setup end", gate_id_));
    }
  }
}

void BooleanBEAVYMSGGate::evaluate_online() {
  
  num_wires_ = 10;
  auto num_simd = 991;
  auto rounds = 2;
  auto num_bits = num_simd*num_wires_;
  auto my_id = beavy_provider_.get_my_id();
  auto rand = ENCRYPTO::BitVector<>::Random(num_bits);
  
  // r1
  num_bits = num_bits/4;
  std::cout<< "nb: "<< num_bits<< std::endl;
  rand = ENCRYPTO::BitVector<>::Random(num_bits);
  share_future_ = beavy_provider_.register_for_bits_message(1 - my_id, gate_id_, num_bits, 0);
  beavy_provider_.broadcast_bits_message(gate_id_, rand, 0);
  Delta_y_share_ = share_future_.get();

  // r2
  // num_bits = num_bits/4;
  std::cout<< "nb: "<< num_bits<< std::endl;
  rand = ENCRYPTO::BitVector<>::Random(num_bits/4);
  auto share_future_1 = beavy_provider_.register_for_bits_message(1 - my_id, gate_id_, num_bits/4, 1);
  beavy_provider_.broadcast_bits_message(gate_id_, rand, 1);
  Delta_y_share_ = share_future_1.get();


  // distribute data among wires
  for (auto& wire_o : outputs_) {
    wire_o->get_public_share() = ENCRYPTO::BitVector<>::Random(wire_o->get_num_simd());
    wire_o->set_online_ready();
  }
}


BooleanBEAVYDOTGate::BooleanBEAVYDOTGate(std::size_t gate_id, BEAVYProvider& beavy_provider,
                                         BooleanBEAVYWireVector&& in_a,
                                         BooleanBEAVYWireVector&& in_b)
    : detail::BasicBooleanBEAVYBinaryGate(gate_id, std::move(in_a), std::move(in_b)),
      beavy_provider_(beavy_provider),
      ot_sender_(nullptr),
      ot_receiver_(nullptr) {
  auto num_bits = count_bits(inputs_a_);
  auto num_wires = inputs_a_.size();
  auto num_simd = inputs_a_[0]->get_num_simd();
  auto my_id = beavy_provider_.get_my_id();
  share_future_ = beavy_provider_.register_for_bits_message(1 - my_id, gate_id_, num_simd);
  auto& otp = beavy_provider_.get_ot_manager().get_provider(1 - my_id);
  ot_sender_ = otp.RegisterSendXCOTBit(num_bits);
  ot_receiver_ = otp.RegisterReceiveXCOTBit(num_bits);
}


BooleanBEAVYDOTGate::~BooleanBEAVYDOTGate() = default;

void BooleanBEAVYDOTGate::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYDOTGate::evaluate_setup start", gate_id_));
    }
  }

  auto num_bits = count_bits(inputs_a_);
  auto num_wires = inputs_a_.size();

  

  auto num_simd = inputs_a_[0]->get_num_simd();
  auto num_bytes = Helpers::Convert::BitsToBytes(num_wires_ * num_simd);
  delta_a_share_.Reserve(num_bytes);
  delta_b_share_.Reserve(num_bytes);
  Delta_y_share_.Reserve(num_bytes);

  outputs_.resize(1);
  outputs_[0]->get_secret_share() = ENCRYPTO::BitVector<>::Random(num_simd);
  outputs_[0]->set_setup_ready();

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& wire_a = inputs_a_[wire_i];
    const auto& wire_b = inputs_b_[wire_i];
    wire_a->wait_setup();
    wire_b->wait_setup();
    delta_a_share_.Append(wire_a->get_secret_share());
    delta_b_share_.Append(wire_b->get_secret_share());
  }


  auto delta_ab_share = delta_a_share_ & delta_b_share_;

  ot_receiver_->SetChoices(delta_a_share_);
  ot_receiver_->SendCorrections();
  ot_sender_->SetCorrelations(delta_b_share_);
  ot_sender_->SendMessages();
  ot_receiver_->ComputeOutputs();
  ot_sender_->ComputeOutputs();
  delta_ab_share ^= ot_sender_->GetOutputs();
  delta_ab_share ^= ot_receiver_->GetOutputs();
  Delta_y_share_.Append(delta_ab_share);
  

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYDOTGate::evaluate_setup end", gate_id_));
    }
  }
}

void BooleanBEAVYDOTGate::evaluate_online() {
  auto num_simd = inputs_a_[0]->get_num_simd();
  auto num_bits = num_wires_ * num_simd;
  ENCRYPTO::BitVector<> Delta_a;
  ENCRYPTO::BitVector<> Delta_b;
  Delta_a.Reserve(Helpers::Convert::BitsToBytes(num_bits));
  Delta_b.Reserve(Helpers::Convert::BitsToBytes(num_bits));

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    const auto& wire_a = inputs_a_[wire_i];
    wire_a->wait_online();
    Delta_a.Append(wire_a->get_public_share());
    const auto& wire_b = inputs_b_[wire_i];
    wire_b->wait_online();
    Delta_b.Append(wire_b->get_public_share());
  }
  
  Delta_y_share_ ^= (Delta_a & delta_b_share_);
  Delta_y_share_ ^= (Delta_b & delta_a_share_);

  if (beavy_provider_.is_my_job(gate_id_)) {
    Delta_y_share_ ^= (Delta_a & Delta_b);
  }

  ENCRYPTO::BitVector<> Delta_y_share;
  Delta_y_share.Reserve(num_simd);
  Delta_y_share.Append(outputs_[0]->get_secret_share());

  for (std::size_t wire_i = 0; wire_i < num_wires_; ++wire_i) {
    std::cout<<"here"<<std::endl;
    Delta_y_share ^= Delta_y_share_.Subset(wire_i * num_simd, (wire_i + 1) * num_simd);
  }

  std::cout<< "delu: "<< Delta_y_share.GetSize()<<std::endl;
  beavy_provider_.broadcast_bits_message(gate_id_, Delta_y_share);
  Delta_y_share ^= share_future_.get();

  // distribute data among wires
  outputs_[0]->get_public_share() = Delta_y_share;
  outputs_[0]->set_online_ready();
}


template <typename T>
ArithmeticBEAVYInputGateSender<T>::ArithmeticBEAVYInputGateSender(
    std::size_t gate_id, BEAVYProvider& beavy_provider, std::size_t num_simd,
    ENCRYPTO::ReusableFiberFuture<std::vector<T>>&& input_future)
    : NewGate(gate_id),
      beavy_provider_(beavy_provider),
      num_simd_(num_simd),
      input_id_(beavy_provider.get_next_input_id(1)),
      input_future_(std::move(input_future)),
      output_(std::make_shared<ArithmeticBEAVYWire<T>>(num_simd)) {
  output_->get_public_share().resize(num_simd, 0);
}

template <typename T>
void ArithmeticBEAVYInputGateSender<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: ArithmeticBEAVYInputGateSender<T>::evaluate_setup start", gate_id_));
    }
  }

  auto my_id = beavy_provider_.get_my_id();
  auto num_parties = beavy_provider_.get_num_parties();
  auto& mbp = beavy_provider_.get_motion_base_provider();
  auto& my_secret_share = output_->get_secret_share();
  auto& my_public_share = output_->get_public_share();
  my_secret_share = Helpers::RandomVector<T>(num_simd_);
  output_->set_setup_ready();
  my_public_share = my_secret_share;
  for (std::size_t party_id = 0; party_id < num_parties; ++party_id) {
    if (party_id == my_id) {
      continue;
    }
    auto& rng = mbp.get_my_randomness_generator(party_id);
    std::transform(std::begin(my_public_share), std::end(my_public_share),
                   std::begin(rng.GetUnsigned<T>(input_id_, num_simd_)),
                   std::begin(my_public_share), std::plus{});
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYInputGateSender<T>::evaluate_setup end", gate_id_));
    }
  }
}

template <typename T>
void ArithmeticBEAVYInputGateSender<T>::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: ArithmeticBEAVYInputGateSender<T>::evaluate_online start", gate_id_));
    }
  }

  // wait for input value
  const auto input = input_future_.get();
  if (input.size() != num_simd_) {
    throw std::runtime_error("size of input bit vector != num_simd_");
  }

  // compute my share
  auto& my_public_share = output_->get_public_share();
  std::transform(std::begin(my_public_share), std::end(my_public_share), std::begin(input),
                 std::begin(my_public_share), std::plus{});
  output_->set_online_ready();
  beavy_provider_.broadcast_ints_message(gate_id_, my_public_share);

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYInputGateSender<T>::evaluate_online end", gate_id_));
    }
  }
}

template class ArithmeticBEAVYInputGateSender<std::uint8_t>;
template class ArithmeticBEAVYInputGateSender<std::uint16_t>;
template class ArithmeticBEAVYInputGateSender<std::uint32_t>;
template class ArithmeticBEAVYInputGateSender<std::uint64_t>;

template <typename T>
ArithmeticBEAVYInputGateReceiver<T>::ArithmeticBEAVYInputGateReceiver(std::size_t gate_id,
                                                                      BEAVYProvider& beavy_provider,
                                                                      std::size_t num_simd,
                                                                      std::size_t input_owner)
    : NewGate(gate_id),
      beavy_provider_(beavy_provider),
      num_simd_(num_simd),
      input_owner_(input_owner),
      input_id_(beavy_provider.get_next_input_id(1)),
      output_(std::make_shared<ArithmeticBEAVYWire<T>>(num_simd)) {
  public_share_future_ =
      beavy_provider_.register_for_ints_message<T>(input_owner_, gate_id_, num_simd);
}

template <typename T>
void ArithmeticBEAVYInputGateReceiver<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: ArithmeticBEAVYInputGateReceiver<T>::evaluate_setup start", gate_id_));
    }
  }

  auto& mbp = beavy_provider_.get_motion_base_provider();
  auto& rng = mbp.get_their_randomness_generator(input_owner_);
  output_->get_secret_share() = rng.GetUnsigned<T>(input_id_, num_simd_);
  output_->set_setup_ready();

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: ArithmeticBEAVYInputGateReceiver<T>::evaluate_setup end", gate_id_));
    }
  }
}

template <typename T>
void ArithmeticBEAVYInputGateReceiver<T>::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: ArithmeticBEAVYInputGateReceiver<T>::evaluate_online start", gate_id_));
    }
  }

  output_->get_public_share() = public_share_future_.get();
  output_->set_online_ready();

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: ArithmeticBEAVYInputGateReceiver<T>::evaluate_online end", gate_id_));
    }
  }
}

template class ArithmeticBEAVYInputGateReceiver<std::uint8_t>;
template class ArithmeticBEAVYInputGateReceiver<std::uint16_t>;
template class ArithmeticBEAVYInputGateReceiver<std::uint32_t>;
template class ArithmeticBEAVYInputGateReceiver<std::uint64_t>;

template <typename T>
ArithmeticBEAVYOutputGate<T>::ArithmeticBEAVYOutputGate(std::size_t gate_id,
                                                        BEAVYProvider& beavy_provider,
                                                        ArithmeticBEAVYWireP<T>&& input,
                                                        std::size_t output_owner)
    : NewGate(gate_id),
      beavy_provider_(beavy_provider),
      output_owner_(output_owner),
      input_(std::move(input)) {
  std::size_t my_id = beavy_provider_.get_my_id();
  if (output_owner_ == ALL_PARTIES || output_owner_ == my_id) {
    share_future_ =
        beavy_provider_.register_for_ints_message<T>(1 - my_id, gate_id_, input_->get_num_simd());
  }
}

template <typename T>
ENCRYPTO::ReusableFiberFuture<std::vector<T>> ArithmeticBEAVYOutputGate<T>::get_output_future() {
  std::size_t my_id = beavy_provider_.get_my_id();
  if (output_owner_ == ALL_PARTIES || output_owner_ == my_id) {
    return output_promise_.get_future();
  } else {
    throw std::logic_error("not this parties output");
  }
}

template <typename T>
void ArithmeticBEAVYOutputGate<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYOutputGate<T>::evaluate_setup start", gate_id_));
    }
  }

  std::size_t my_id = beavy_provider_.get_my_id();
  if (output_owner_ != my_id) {
    input_->wait_setup();
    auto my_secret_share = input_->get_secret_share();
    if (output_owner_ == ALL_PARTIES) {
      beavy_provider_.broadcast_ints_message(gate_id_, my_secret_share);
    } else {
      beavy_provider_.send_ints_message(output_owner_, gate_id_, my_secret_share);
    }
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYOutputGate<T>::evaluate_setup end", gate_id_));
    }
  }
}

template <typename T>
void ArithmeticBEAVYOutputGate<T>::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYOutputGate<T>::evaluate_online start", gate_id_));
    }
  }

  std::size_t my_id = beavy_provider_.get_my_id();
  if (output_owner_ == ALL_PARTIES || output_owner_ == my_id) {
    input_->wait_setup();
    auto my_secret_share = input_->get_secret_share();
    const auto other_secret_share = share_future_.get();
    std::transform(std::begin(my_secret_share), std::end(my_secret_share),
                   std::begin(other_secret_share), std::begin(my_secret_share), std::plus{});
    input_->wait_online();
    std::transform(std::begin(input_->get_public_share()), std::end(input_->get_public_share()),
                   std::begin(my_secret_share), std::begin(my_secret_share), std::minus{});
    output_promise_.set_value(std::move(my_secret_share));
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYOutputGate<T>::evaluate_online end", gate_id_));
    }
  }
}

template class ArithmeticBEAVYOutputGate<std::uint8_t>;
template class ArithmeticBEAVYOutputGate<std::uint16_t>;
template class ArithmeticBEAVYOutputGate<std::uint32_t>;
template class ArithmeticBEAVYOutputGate<std::uint64_t>;

template <typename T>
ArithmeticBEAVYOutputShareGate<T>::ArithmeticBEAVYOutputShareGate(std::size_t gate_id,
                                                    ArithmeticBEAVYWireP<T>&& input)
    : NewGate(gate_id),
      input_(std::move(input)) {
}

template <typename T>
ENCRYPTO::ReusableFiberFuture<std::vector<T>> ArithmeticBEAVYOutputShareGate<T>::get_public_share_future() {
  return public_share_promise_.get_future();
}

template <typename T>
ENCRYPTO::ReusableFiberFuture<std::vector<T>> ArithmeticBEAVYOutputShareGate<T>::get_secret_share_future() {
  return secret_share_promise_.get_future();
}

template <typename T>
void ArithmeticBEAVYOutputShareGate<T>::evaluate_setup() {
  input_->wait_setup();
  secret_share_promise_.set_value(input_->get_secret_share());
}

template <typename T>
void ArithmeticBEAVYOutputShareGate<T>::evaluate_online() {
  input_->wait_online();
  public_share_promise_.set_value(input_->get_public_share());
}

template class ArithmeticBEAVYOutputShareGate<std::uint8_t>;
template class ArithmeticBEAVYOutputShareGate<std::uint16_t>;
template class ArithmeticBEAVYOutputShareGate<std::uint32_t>;
template class ArithmeticBEAVYOutputShareGate<std::uint64_t>;

namespace detail {

template <typename T>
BasicArithmeticBEAVYBinaryGate<T>::BasicArithmeticBEAVYBinaryGate(std::size_t gate_id,
                                                                  BEAVYProvider&,
                                                                  ArithmeticBEAVYWireP<T>&& in_a,
                                                                  ArithmeticBEAVYWireP<T>&& in_b)
    : NewGate(gate_id),
      input_a_(std::move(in_a)),
      input_b_(std::move(in_b)),
      output_(std::make_shared<ArithmeticBEAVYWire<T>>(input_a_->get_num_simd())) {
  if (input_a_->get_num_simd() != input_b_->get_num_simd()) {
    throw std::logic_error("number of SIMD values need to be the same for all wires");
  }
}

template class BasicArithmeticBEAVYBinaryGate<std::uint8_t>;
template class BasicArithmeticBEAVYBinaryGate<std::uint16_t>;
template class BasicArithmeticBEAVYBinaryGate<std::uint32_t>;
template class BasicArithmeticBEAVYBinaryGate<std::uint64_t>;

template <typename T>
BasicArithmeticBEAVYUnaryGate<T>::BasicArithmeticBEAVYUnaryGate(std::size_t gate_id, BEAVYProvider&,
                                                                ArithmeticBEAVYWireP<T>&& in)
    : NewGate(gate_id),
      input_(std::move(in)),
      output_(std::make_shared<ArithmeticBEAVYWire<T>>(input_->get_num_simd())) {}

template class BasicArithmeticBEAVYUnaryGate<std::uint8_t>;
template class BasicArithmeticBEAVYUnaryGate<std::uint16_t>;
template class BasicArithmeticBEAVYUnaryGate<std::uint32_t>;
template class BasicArithmeticBEAVYUnaryGate<std::uint64_t>;

template <typename T>
BasicBooleanXArithmeticBEAVYBinaryGate<T>::BasicBooleanXArithmeticBEAVYBinaryGate(
    std::size_t gate_id, BEAVYProvider&, BooleanBEAVYWireP&& in_a, ArithmeticBEAVYWireP<T>&& in_b)
    : NewGate(gate_id),
      input_bool_(std::move(in_a)),
      input_arith_(std::move(in_b)),
      output_(std::make_shared<ArithmeticBEAVYWire<T>>(input_arith_->get_num_simd())) {
  if (input_arith_->get_num_simd() != input_bool_->get_num_simd()) {
    throw std::logic_error("number of SIMD values need to be the same for all wires");
  }
}

template class BasicBooleanXArithmeticBEAVYBinaryGate<std::uint8_t>;
template class BasicBooleanXArithmeticBEAVYBinaryGate<std::uint16_t>;
template class BasicBooleanXArithmeticBEAVYBinaryGate<std::uint32_t>;
template class BasicBooleanXArithmeticBEAVYBinaryGate<std::uint64_t>;

template <typename T>
BasicArithmeticBooleanBEAVYBinaryGate<T>::BasicArithmeticBooleanBEAVYBinaryGate(
    std::size_t gate_id, BEAVYProvider&, ArithmeticBEAVYWireP<T>&& in_a, ArithmeticBEAVYWireP<T>&& in_b)
    : NewGate(gate_id),
      num_wires_(1),
      input_a_(std::move(in_a)),
      input_b_(std::move(in_b)) {
  auto num_simd = input_a_->get_num_simd();
  if (input_a_->get_num_simd() != input_b_->get_num_simd()) {
    throw std::logic_error("number of SIMD values need to be the same for all wires");
  }
  outputs_.reserve(num_wires_);
  std::generate_n(std::back_inserter(outputs_), num_wires_,
                  [num_simd] { return std::make_shared<BooleanBEAVYWire>(num_simd); });
}

template class BasicArithmeticBooleanBEAVYBinaryGate<std::uint8_t>;
template class BasicArithmeticBooleanBEAVYBinaryGate<std::uint16_t>;
template class BasicArithmeticBooleanBEAVYBinaryGate<std::uint32_t>;
template class BasicArithmeticBooleanBEAVYBinaryGate<std::uint64_t>;

}  // namespace detail

template <typename T>
ArithmeticBEAVYNEGGate<T>::ArithmeticBEAVYNEGGate(std::size_t gate_id,
                                                  BEAVYProvider& beavy_provider,
                                                  ArithmeticBEAVYWireP<T>&& in)
    : detail::BasicArithmeticBEAVYUnaryGate<T>(gate_id, beavy_provider, std::move(in)) {
  this->output_->get_public_share().resize(this->input_->get_num_simd());
  this->output_->get_secret_share().resize(this->input_->get_num_simd());
}

template <typename T>
void ArithmeticBEAVYNEGGate<T>::evaluate_setup() {
  this->input_->wait_setup();
  assert(this->output_->get_secret_share().size() == this->input_->get_num_simd());
  std::transform(std::begin(this->input_->get_secret_share()),
                 std::end(this->input_->get_secret_share()),
                 std::begin(this->output_->get_secret_share()), std::negate{});
  this->output_->set_setup_ready();
}

template <typename T>
void ArithmeticBEAVYNEGGate<T>::evaluate_online() {
  this->input_->wait_online();
  assert(this->output_->get_public_share().size() == this->input_->get_num_simd());
  std::transform(std::begin(this->input_->get_public_share()),
                 std::end(this->input_->get_public_share()),
                 std::begin(this->output_->get_public_share()), std::negate{});
  this->output_->set_online_ready();
}

template class ArithmeticBEAVYNEGGate<std::uint8_t>;
template class ArithmeticBEAVYNEGGate<std::uint16_t>;
template class ArithmeticBEAVYNEGGate<std::uint32_t>;
template class ArithmeticBEAVYNEGGate<std::uint64_t>;

template <typename T>
ArithmeticBEAVYADDGate<T>::ArithmeticBEAVYADDGate(std::size_t gate_id,
                                                  BEAVYProvider& beavy_provider,
                                                  ArithmeticBEAVYWireP<T>&& in_a,
                                                  ArithmeticBEAVYWireP<T>&& in_b)
    : detail::BasicArithmeticBEAVYBinaryGate<T>(gate_id, beavy_provider, std::move(in_a),
                                                std::move(in_b)) {
  this->output_->get_public_share().resize(this->input_a_->get_num_simd());
  this->output_->get_secret_share().resize(this->input_a_->get_num_simd());
}

template <typename T>
void ArithmeticBEAVYADDGate<T>::evaluate_setup() {
  this->input_a_->wait_setup();
  this->input_b_->wait_setup();
  assert(this->output_->get_secret_share().size() == this->input_a_->get_num_simd());
  assert(this->output_->get_secret_share().size() == this->input_b_->get_num_simd());
  std::transform(std::begin(this->input_a_->get_secret_share()),
                 std::end(this->input_a_->get_secret_share()),
                 std::begin(this->input_b_->get_secret_share()),
                 std::begin(this->output_->get_secret_share()), std::plus{});
  this->output_->set_setup_ready();
}

template <typename T>
void ArithmeticBEAVYADDGate<T>::evaluate_online() {
  this->input_a_->wait_online();
  this->input_b_->wait_online();
  assert(this->output_->get_public_share().size() == this->input_a_->get_num_simd());
  std::transform(std::begin(this->input_a_->get_public_share()),
                 std::end(this->input_a_->get_public_share()),
                 std::begin(this->input_b_->get_public_share()),
                 std::begin(this->output_->get_public_share()), std::plus{});
  this->output_->set_online_ready();
}

template class ArithmeticBEAVYADDGate<std::uint8_t>;
template class ArithmeticBEAVYADDGate<std::uint16_t>;
template class ArithmeticBEAVYADDGate<std::uint32_t>;
template class ArithmeticBEAVYADDGate<std::uint64_t>;

template <typename T>
ArithmeticBEAVYMULNIGate<T>::ArithmeticBEAVYMULNIGate(std::size_t gate_id,
                                                  BEAVYProvider& beavy_provider,
                                                  ArithmeticBEAVYWireP<T>&& in_a,
                                                  ArithmeticBEAVYWireP<T>&& in_b)
    : detail::BasicArithmeticBEAVYBinaryGate<T>(gate_id, beavy_provider, std::move(in_a),
                                                std::move(in_b)),
      beavy_provider_(beavy_provider) {
  auto my_id = beavy_provider_.get_my_id();
  auto num_simd = this->input_a_->get_num_simd();
  // share_future_ = beavy_provider_.register_for_ints_message<T>(1 - my_id, this->gate_id_,
  //                                                              this->input_a_->get_num_simd());
  auto& ap = beavy_provider_.get_arith_manager().get_provider(1 - my_id);
  mult_sender_ = ap.template register_integer_multiplication_send<T>(num_simd);
  mult_receiver_ = ap.template register_integer_multiplication_receive<T>(num_simd);
}

template <typename T>
ArithmeticBEAVYMULNIGate<T>::~ArithmeticBEAVYMULNIGate() = default;

template <typename T>
void ArithmeticBEAVYMULNIGate<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYMULNIGate<T>::evaluate_setup start", this->gate_id_));
    }
  }

  auto num_simd = this->input_a_->get_num_simd();

  this->output_->get_secret_share() = Helpers::RandomVector<T>(num_simd); 
  this->output_->set_setup_ready();

  this->input_a_->wait_setup();
  this->input_b_->wait_setup();
  const auto& delta_a_share = this->input_a_->get_secret_share();
  const auto& delta_b_share = this->input_b_->get_secret_share();
  const auto& delta_y_share = this->output_->get_secret_share();

  mult_receiver_->set_inputs(delta_a_share);
  mult_sender_->set_inputs(delta_b_share);

  Delta_y_share_.resize(num_simd);
  // [Delta_y]_i = [delta_a]_i * [delta_b]_i
  std::transform(std::begin(delta_a_share), std::end(delta_a_share), std::begin(delta_b_share),
                 std::begin(Delta_y_share_), std::multiplies{});
  // [Delta_y]_i += [delta_y]_i // don't do this
  // std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(delta_y_share),
  //                std::begin(Delta_y_share_), std::plus{});

  mult_receiver_->compute_outputs();
  mult_sender_->compute_outputs();
  // [[delta_a]_i * [delta_b]_(1-i)]_i
  auto delta_ab_share1 = mult_receiver_->get_outputs();
  // [[delta_b]_i * [delta_a]_(1-i)]_i
  auto delta_ab_share2 = mult_sender_->get_outputs();
  // [Delta_y]_i += [[delta_a]_i * [delta_b]_(1-i)]_i
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(delta_ab_share1),
                 std::begin(Delta_y_share_), std::plus{});
  // [Delta_y]_i += [[delta_b]_i * [delta_a]_(1-i)]_i
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(delta_ab_share2),
                 std::begin(Delta_y_share_), std::plus{});

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYMULNIGate::evaluate_setup end", this->gate_id_));
    }
  }
}

template <typename T>
void ArithmeticBEAVYMULNIGate<T>::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYMULNIGate<T>::evaluate_online start", this->gate_id_));
    }
  }

  auto num_simd = this->input_a_->get_num_simd();
  this->input_a_->wait_online();
  this->input_b_->wait_online();
  const auto& Delta_a = this->input_a_->get_public_share();
  const auto& Delta_b = this->input_b_->get_public_share();
  const auto& delta_a_share = this->input_a_->get_secret_share();
  const auto& delta_b_share = this->input_b_->get_secret_share();
  std::vector<T> tmp(num_simd);

  // after setup phase, `Delta_y_share_` contains [delta_y]_i + [delta_ab]_i

  // [Delta_y]_i -= Delta_a * [delta_b]_i
  std::transform(std::begin(Delta_a), std::end(Delta_a), std::begin(delta_b_share), std::begin(tmp),
                 std::multiplies{});
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(tmp),
                 std::begin(Delta_y_share_), std::minus{});

  // [Delta_y]_i -= Delta_b * [delta_a]_i
  std::transform(std::begin(Delta_b), std::end(Delta_b), std::begin(delta_a_share), std::begin(tmp),
                 std::multiplies{});
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(tmp),
                 std::begin(Delta_y_share_), std::minus{});

  // [Delta_y]_i += Delta_ab (== Delta_a * Delta_b)
  if (beavy_provider_.is_my_job(this->gate_id_)) {
    std::transform(std::begin(Delta_a), std::end(Delta_a), std::begin(Delta_b), std::begin(tmp),
                   std::multiplies{});
    std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(tmp),
                   std::begin(Delta_y_share_), std::plus{});
  }
  // broadcast [Delta_y]_i
  // beavy_provider_.broadcast_ints_message(this->gate_id_, Delta_y_share_);
  // Delta_y = [Delta_y]_i + [Delta_y]_(1-i)
  // std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_),
                //  std::begin(share_future_.get()), std::begin(Delta_y_share_), std::plus{});
  
  this->output_->get_public_share() = std::move(Delta_y_share_); // !Parties have different public shares which corresponds to additive shares
  this->output_->set_online_ready();

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYMULNIGate<T>::evaluate_online end", this->gate_id_));
    }
  }
}

template class ArithmeticBEAVYMULNIGate<std::uint8_t>;
template class ArithmeticBEAVYMULNIGate<std::uint16_t>;
template class ArithmeticBEAVYMULNIGate<std::uint32_t>;
template class ArithmeticBEAVYMULNIGate<std::uint64_t>;

template <typename T>
ArithmeticBEAVYMULGate<T>::ArithmeticBEAVYMULGate(std::size_t gate_id,
                                                  BEAVYProvider& beavy_provider,
                                                  ArithmeticBEAVYWireP<T>&& in_a,
                                                  ArithmeticBEAVYWireP<T>&& in_b)
    : detail::BasicArithmeticBEAVYBinaryGate<T>(gate_id, beavy_provider, std::move(in_a),
                                                std::move(in_b)),
      beavy_provider_(beavy_provider) {
  auto my_id = beavy_provider_.get_my_id();
  auto num_simd = this->input_a_->get_num_simd();
  share_future_ = beavy_provider_.register_for_ints_message<T>(1 - my_id, this->gate_id_,
                                                               this->input_a_->get_num_simd());
  auto& ap = beavy_provider_.get_arith_manager().get_provider(1 - my_id);
  mult_sender_ = ap.template register_integer_multiplication_send<T>(num_simd);
  mult_receiver_ = ap.template register_integer_multiplication_receive<T>(num_simd);
}

template <typename T>
ArithmeticBEAVYMULGate<T>::~ArithmeticBEAVYMULGate() = default;

template <typename T>
void ArithmeticBEAVYMULGate<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYMULGate<T>::evaluate_setup start", this->gate_id_));
    }
  }

  auto num_simd = this->input_a_->get_num_simd();

  this->output_->get_secret_share() = Helpers::RandomVector<T>(num_simd);
  this->output_->set_setup_ready();

  this->input_a_->wait_setup();
  this->input_b_->wait_setup();
  const auto& delta_a_share = this->input_a_->get_secret_share();
  const auto& delta_b_share = this->input_b_->get_secret_share();
  const auto& delta_y_share = this->output_->get_secret_share();

  mult_receiver_->set_inputs(delta_a_share);
  mult_sender_->set_inputs(delta_b_share);

  Delta_y_share_.resize(num_simd);
  // [Delta_y]_i = [delta_a]_i * [delta_b]_i
  std::transform(std::begin(delta_a_share), std::end(delta_a_share), std::begin(delta_b_share),
                 std::begin(Delta_y_share_), std::multiplies{});
  // [Delta_y]_i += [delta_y]_i
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(delta_y_share),
                 std::begin(Delta_y_share_), std::plus{});

  mult_receiver_->compute_outputs();
  mult_sender_->compute_outputs();
  // [[delta_a]_i * [delta_b]_(1-i)]_i
  auto delta_ab_share1 = mult_receiver_->get_outputs();
  // [[delta_b]_i * [delta_a]_(1-i)]_i
  auto delta_ab_share2 = mult_sender_->get_outputs();
  // [Delta_y]_i += [[delta_a]_i * [delta_b]_(1-i)]_i
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(delta_ab_share1),
                 std::begin(Delta_y_share_), std::plus{});
  // [Delta_y]_i += [[delta_b]_i * [delta_a]_(1-i)]_i
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(delta_ab_share2),
                 std::begin(Delta_y_share_), std::plus{});

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYMULGate::evaluate_setup end", this->gate_id_));
    }
  }
}

template <typename T>
void ArithmeticBEAVYMULGate<T>::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYMULGate<T>::evaluate_online start", this->gate_id_));
    }
  }

  auto num_simd = this->input_a_->get_num_simd();
  this->input_a_->wait_online();
  this->input_b_->wait_online();
  const auto& Delta_a = this->input_a_->get_public_share();
  const auto& Delta_b = this->input_b_->get_public_share();
  const auto& delta_a_share = this->input_a_->get_secret_share();
  const auto& delta_b_share = this->input_b_->get_secret_share();
  std::vector<T> tmp(num_simd);

  // after setup phase, `Delta_y_share_` contains [delta_y]_i + [delta_ab]_i

  // [Delta_y]_i -= Delta_a * [delta_b]_i
  std::transform(std::begin(Delta_a), std::end(Delta_a), std::begin(delta_b_share), std::begin(tmp),
                 std::multiplies{});
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(tmp),
                 std::begin(Delta_y_share_), std::minus{});

  // [Delta_y]_i -= Delta_b * [delta_a]_i
  std::transform(std::begin(Delta_b), std::end(Delta_b), std::begin(delta_a_share), std::begin(tmp),
                 std::multiplies{});
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(tmp),
                 std::begin(Delta_y_share_), std::minus{});

  // [Delta_y]_i += Delta_ab (== Delta_a * Delta_b)
  if (beavy_provider_.is_my_job(this->gate_id_)) {
    std::transform(std::begin(Delta_a), std::end(Delta_a), std::begin(Delta_b), std::begin(tmp),
                   std::multiplies{});
    std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(tmp),
                   std::begin(Delta_y_share_), std::plus{});
  }
  // broadcast [Delta_y]_i
  beavy_provider_.broadcast_ints_message(this->gate_id_, Delta_y_share_);
  // Delta_y = [Delta_y]_i + [Delta_y]_(1-i)
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_),
                 std::begin(share_future_.get()), std::begin(Delta_y_share_), std::plus{});
  this->output_->get_public_share() = std::move(Delta_y_share_);
  this->output_->set_online_ready();

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYMULGate<T>::evaluate_online end", this->gate_id_));
    }
  }
}

template class ArithmeticBEAVYMULGate<std::uint8_t>;
template class ArithmeticBEAVYMULGate<std::uint16_t>;
template class ArithmeticBEAVYMULGate<std::uint32_t>;
template class ArithmeticBEAVYMULGate<std::uint64_t>;

template <typename T>
ArithmeticBEAVYEQEXPGate<T>::ArithmeticBEAVYEQEXPGate(std::size_t gate_id,
                                                  BEAVYProvider& beavy_provider,
                                                  ArithmeticBEAVYWireP<T>&& in_a,
                                                  ArithmeticBEAVYWireP<T>&& in_b)
    : detail::BasicArithmeticBooleanBEAVYBinaryGate<T>(gate_id, beavy_provider, std::move(in_a),
                                                std::move(in_b)),
      beavy_provider_(beavy_provider) {
  auto my_id = beavy_provider_.get_my_id();
  auto num_simd = this->input_a_->get_num_simd();
  size_t vec_size = this->input_b_->get_public_share()[0];
  std::cout<< "vs: " << vec_size << std::endl;
  assert(vec_size < 100000);
  assert(vec_size > 0);
  share_future_1 = beavy_provider_.register_for_bits_message(1 - my_id, this->gate_id_, vec_size*num_simd, 0);
  share_future_2 = beavy_provider_.register_for_bits_message(1 - my_id, this->gate_id_, num_simd, 1);
  auto& otp = beavy_provider_.get_ot_manager().get_provider(1 - my_id);
  ot_sender_ = otp.RegisterSendXCOTBit(vec_size*num_simd);
  ot_receiver_ = otp.RegisterReceiveXCOTBit(vec_size*num_simd);
}

template <typename T>
ArithmeticBEAVYEQEXPGate<T>::~ArithmeticBEAVYEQEXPGate() = default;

template <typename T>
void ArithmeticBEAVYEQEXPGate<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanBEAVYDOTGate::evaluate_setup start", this->gate_id));
    }
  }
  std::cout<< "eqexp setup" << std::endl;

  auto num_simd = this->input_a_->get_num_simd();
  size_t vec_size = this->input_b_->get_public_share()[0];

  auto num_bytes = Helpers::Convert::BitsToBytes(vec_size * num_simd);
  delta_a_share_.Reserve(num_bytes);
  delta_b_share_.Reserve(num_bytes);
  Delta_y_share_.Reserve(num_bytes);
  ENCRYPTO::BitVector<> rv = ENCRYPTO::BitVector(vec_size * num_simd, false);
  delta_a_share_.Append(rv);
  delta_b_share_.Append(rv);

  auto delta_ab_share = delta_a_share_ & delta_b_share_;

  ot_receiver_->SetChoices(delta_a_share_);
  ot_receiver_->SendCorrections();
  ot_sender_->SetCorrelations(delta_b_share_);
  ot_sender_->SendMessages();
  ot_receiver_->ComputeOutputs();
  ot_sender_->ComputeOutputs();
  delta_ab_share ^= ot_sender_->GetOutputs();
  delta_ab_share ^= ot_receiver_->GetOutputs();
  Delta_y_share_.Append(delta_ab_share);

  

  this->outputs_[0]->get_secret_share() = ENCRYPTO::BitVector<>::Random(this->input_a_->get_num_simd());
  this->outputs_[0]->set_setup_ready();


  
  
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: ArithmeticBEAVYEQEXPGate::evaluate_setup end", this->gate_id));
    }
  }
}

template <typename T>
void ArithmeticBEAVYEQEXPGate<T>::evaluate_online() {
  std::cout<< "eqexp online" << std::endl;
  auto my_id = beavy_provider_.get_my_id();
  auto num_simd = this->input_a_->get_num_simd();
  size_t vec_size = this->input_b_->get_public_share()[0];
  
 
  pub_val_b_.Reserve(vec_size*num_simd);
  pub_val_a_.Reserve(vec_size*num_simd);
  auto tmp = ENCRYPTO::BitVector<>(num_simd*vec_size); 

  #pragma omp parallel for
  for(int i=0; i<num_simd; ++i){
    auto pos = this->input_a_->get_public_share()[i] % vec_size;
    tmp.Set(true, (pos)*num_simd + i);
  }
  
  if(my_id==0){
    pub_val_a_.Append(tmp);
    beavy_provider_.broadcast_bits_message(this->gate_id_, pub_val_a_, 0);
    pub_val_b_ = share_future_1.get();

  }
  else{
    
    pub_val_b_.Append(tmp);
    beavy_provider_.broadcast_bits_message(this->gate_id_, pub_val_b_, 0);
    pub_val_a_ = share_future_1.get();
  }
  
  ENCRYPTO::BitVector<> Delta_a;
  ENCRYPTO::BitVector<> Delta_b;
  auto num_bits = num_simd*vec_size;
  Delta_a.Reserve(Helpers::Convert::BitsToBytes(num_bits));
  Delta_b.Reserve(Helpers::Convert::BitsToBytes(num_bits));

  Delta_a.Append(pub_val_a_);
  Delta_b.Append(pub_val_b_);
  
  Delta_y_share_ ^= (Delta_a & delta_b_share_);
  Delta_y_share_ ^= (Delta_b & delta_a_share_);

  if (my_id == 0) {
    Delta_y_share_ ^= (Delta_a & Delta_b);
  }

  ENCRYPTO::BitVector<> Delta_y_share;
  Delta_y_share.Reserve(num_simd);
  Delta_y_share.Append(this->outputs_[0]->get_secret_share());

  for (std::size_t i = 0; i < vec_size; ++i) {
    Delta_y_share ^= Delta_y_share_.Subset(i * num_simd, (i + 1) * num_simd);
  }

  beavy_provider_.broadcast_bits_message(this->gate_id_, Delta_y_share, 1);
  Delta_y_share ^= share_future_2.get();
  
  this->outputs_[0]->get_public_share() = Delta_y_share;
  this->outputs_[0]->set_online_ready();
}

template class ArithmeticBEAVYEQEXPGate<std::uint8_t>;
template class ArithmeticBEAVYEQEXPGate<std::uint16_t>;
template class ArithmeticBEAVYEQEXPGate<std::uint32_t>;
template class ArithmeticBEAVYEQEXPGate<std::uint64_t>;


template <typename T>
ArithmeticBEAVYSQRGate<T>::ArithmeticBEAVYSQRGate(std::size_t gate_id,
                                                  BEAVYProvider& beavy_provider,
                                                  ArithmeticBEAVYWireP<T>&& in)
    : detail::BasicArithmeticBEAVYUnaryGate<T>(gate_id, beavy_provider, std::move(in)),
      beavy_provider_(beavy_provider) {
  auto my_id = beavy_provider_.get_my_id();
  auto num_simd = this->input_->get_num_simd();
  share_future_ = beavy_provider_.register_for_ints_message<T>(1 - my_id, this->gate_id_, num_simd);
  auto& ap = beavy_provider_.get_arith_manager().get_provider(1 - my_id);
  if (my_id == 0) {
    mult_sender_ = ap.template register_integer_multiplication_send<T>(num_simd);
    mult_receiver_ = nullptr;
  } else {
    mult_receiver_ = ap.template register_integer_multiplication_receive<T>(num_simd);
    mult_sender_ = nullptr;
  }
}

template <typename T>
ArithmeticBEAVYSQRGate<T>::~ArithmeticBEAVYSQRGate() = default;

template <typename T>
void ArithmeticBEAVYSQRGate<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYSQRGate<T>::evaluate_setup start", this->gate_id_));
    }
  }

  auto num_simd = this->input_->get_num_simd();

  this->output_->get_secret_share() = Helpers::RandomVector<T>(num_simd);
  this->output_->set_setup_ready();

  const auto& delta_a_share = this->input_->get_secret_share();
  const auto& delta_y_share = this->output_->get_secret_share();

  if (mult_sender_) {
    mult_sender_->set_inputs(delta_a_share);
  } else {
    mult_receiver_->set_inputs(delta_a_share);
  }

  Delta_y_share_.resize(num_simd);
  // [Delta_y]_i = [delta_a]_i * [delta_a]_i
  std::transform(std::begin(delta_a_share), std::end(delta_a_share), std::begin(Delta_y_share_),
                 [](auto x) { return x * x; });
  // [Delta_y]_i += [delta_y]_i
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(delta_y_share),
                 std::begin(Delta_y_share_), std::plus{});

  // [[delta_a]_i * [delta_a]_(1-i)]_i
  std::vector<T> delta_aa_share;
  if (mult_sender_) {
    mult_sender_->compute_outputs();
    delta_aa_share = mult_sender_->get_outputs();
  } else {
    mult_receiver_->compute_outputs();
    delta_aa_share = mult_receiver_->get_outputs();
  }
  // [Delta_y]_i += 2 * [[delta_a]_i * [delta_a]_(1-i)]_i
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(delta_aa_share),
                 std::begin(Delta_y_share_), [](auto x, auto y) { return x + 2 * y; });

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYSQRGate::evaluate_setup end", this->gate_id_));
    }
  }
}

template <typename T>
void ArithmeticBEAVYSQRGate<T>::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYSQRGate<T>::evaluate_online start", this->gate_id_));
    }
  }

  auto num_simd = this->input_->get_num_simd();
  this->input_->wait_online();
  const auto& Delta_a = this->input_->get_public_share();
  const auto& delta_a_share = this->input_->get_secret_share();
  std::vector<T> tmp(num_simd);

  // after setup phase, `Delta_y_share_` contains [delta_y]_i + [delta_ab]_i

  // [Delta_y]_i -= 2 * Delta_a * [delta_a]_i
  std::transform(std::begin(Delta_a), std::end(Delta_a), std::begin(delta_a_share), std::begin(tmp),
                 [](auto x, auto y) { return 2 * x * y; });
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(tmp),
                 std::begin(Delta_y_share_), std::minus{});

  // [Delta_y]_i += Delta_aa (== Delta_a * Delta_a)
  if (beavy_provider_.is_my_job(this->gate_id_)) {
    std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_), std::begin(Delta_a),
                   std::begin(Delta_y_share_), [](auto x, auto y) { return x + y * y; });
  }
  // broadcast [Delta_y]_i
  beavy_provider_.broadcast_ints_message(this->gate_id_, Delta_y_share_);
  // Delta_y = [Delta_y]_i + [Delta_y]_(1-i)
  std::transform(std::begin(Delta_y_share_), std::end(Delta_y_share_),
                 std::begin(share_future_.get()), std::begin(Delta_y_share_), std::plus{});
  this->output_->get_public_share() = std::move(Delta_y_share_);
  this->output_->set_online_ready();

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(
          fmt::format("Gate {}: ArithmeticBEAVYSQRGate<T>::evaluate_online end", this->gate_id_));
    }
  }
}

template class ArithmeticBEAVYSQRGate<std::uint8_t>;
template class ArithmeticBEAVYSQRGate<std::uint16_t>;
template class ArithmeticBEAVYSQRGate<std::uint32_t>;
template class ArithmeticBEAVYSQRGate<std::uint64_t>;

template <typename T>
BooleanXArithmeticBEAVYMULGate<T>::BooleanXArithmeticBEAVYMULGate(std::size_t gate_id,
                                                                  BEAVYProvider& beavy_provider,
                                                                  BooleanBEAVYWireP&& in_a,
                                                                  ArithmeticBEAVYWireP<T>&& in_b)
    : detail::BasicBooleanXArithmeticBEAVYBinaryGate<T>(gate_id, beavy_provider, std::move(in_a),
                                                        std::move(in_b)),
      beavy_provider_(beavy_provider) {
  if (beavy_provider_.get_num_parties() != 2) {
    throw std::logic_error("currently only two parties are supported");
  }
  const auto my_id = beavy_provider_.get_my_id();
  auto num_simd = this->input_arith_->get_num_simd();
  auto& ap = beavy_provider_.get_arith_manager().get_provider(1 - my_id);
  if (beavy_provider_.is_my_job(this->gate_id_)) {
    mult_int_side_ = ap.register_bit_integer_multiplication_int_side<T>(num_simd, 2);
    mult_bit_side_ = ap.register_bit_integer_multiplication_bit_side<T>(num_simd, 1);
  } else {
    mult_int_side_ = ap.register_bit_integer_multiplication_int_side<T>(num_simd, 1);
    mult_bit_side_ = ap.register_bit_integer_multiplication_bit_side<T>(num_simd, 2);
  }
  delta_b_share_.resize(num_simd);
  delta_b_x_delta_n_share_.resize(num_simd);
  share_future_ = beavy_provider_.register_for_ints_message<T>(1 - my_id, this->gate_id_, num_simd);
}

template <typename T>
BooleanXArithmeticBEAVYMULGate<T>::~BooleanXArithmeticBEAVYMULGate() = default;

template <typename T>
void BooleanXArithmeticBEAVYMULGate<T>::evaluate_setup() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: BooleanXArithmeticBEAVYMULGate<T>::evaluate_setup start", this->gate_id_));
    }
  }

  auto num_simd = this->input_arith_->get_num_simd();

  this->output_->get_secret_share() = Helpers::RandomVector<T>(num_simd);
  this->output_->set_setup_ready();

  this->input_arith_->wait_setup();
  this->input_bool_->wait_setup();
  const auto& int_sshare = this->input_arith_->get_secret_share();
  assert(int_sshare.size() == num_simd);
  const auto& bit_sshare = this->input_bool_->get_secret_share();
  assert(bit_sshare.GetSize() == num_simd);

  // Use the optimized variant from Lennart's thesis to compute the setup phase
  // using only two (vector) OTs per multiplication.

  std::vector<T> bit_sshare_as_ints(num_simd);
  for (std::size_t int_i = 0; int_i < num_simd; ++int_i) {
    bit_sshare_as_ints[int_i] = bit_sshare.Get(int_i);
  }

  mult_bit_side_->set_inputs(bit_sshare);

  if (beavy_provider_.is_my_job(this->gate_id_)) {
    std::vector<T> mult_inputs(2 * num_simd);
    for (std::size_t int_i = 0; int_i < num_simd; ++int_i) {
      mult_inputs[2 * int_i] = bit_sshare_as_ints[int_i];
      mult_inputs[2 * int_i + 1] =
          int_sshare[int_i] - 2 * bit_sshare_as_ints[int_i] * int_sshare[int_i];
    }
    mult_int_side_->set_inputs(std::move(mult_inputs));
  } else {
    std::vector<T> mult_inputs(num_simd);
    std::transform(std::begin(int_sshare), std::end(int_sshare), std::begin(bit_sshare_as_ints),
                   std::begin(mult_inputs), [](auto n, auto b) { return n - 2 * b * n; });
    mult_int_side_->set_inputs(std::move(mult_inputs));
  }

  mult_bit_side_->compute_outputs();
  mult_int_side_->compute_outputs();
  auto mult_bit_side_out = mult_bit_side_->get_outputs();
  auto mult_int_side_out = mult_int_side_->get_outputs();

  // compute [delta_b]^A and [delta_b * delta_n]^A
  if (beavy_provider_.is_my_job(this->gate_id_)) {
    for (std::size_t int_i = 0; int_i < num_simd; ++int_i) {
      delta_b_share_[int_i] = bit_sshare_as_ints[int_i] - 2 * mult_int_side_out[2 * int_i];
      delta_b_x_delta_n_share_[int_i] = bit_sshare_as_ints[int_i] * int_sshare[int_i] +
                                        mult_int_side_out[2 * int_i + 1] + mult_bit_side_out[int_i];
    }
  } else {
    for (std::size_t int_i = 0; int_i < num_simd; ++int_i) {
      delta_b_share_[int_i] = bit_sshare_as_ints[int_i] - 2 * mult_bit_side_out[2 * int_i];
      delta_b_x_delta_n_share_[int_i] = bit_sshare_as_ints[int_i] * int_sshare[int_i] +
                                        mult_bit_side_out[2 * int_i + 1] + mult_int_side_out[int_i];
    }
  }

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format("Gate {}: BooleanXArithmeticBEAVYMULGate<T>::evaluate_setup end",
                                   this->gate_id_));
    }
  }
}

template <typename T>
void BooleanXArithmeticBEAVYMULGate<T>::evaluate_online() {
  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: BooleanXArithmeticBEAVYMULGate<T>::evaluate_online start", this->gate_id_));
    }
  }

  auto num_simd = this->input_arith_->get_num_simd();

  this->input_bool_->wait_online();
  this->input_arith_->wait_online();
  const auto& int_sshare = this->input_arith_->get_secret_share();
  const auto& int_pshare = this->input_arith_->get_public_share();
  assert(int_pshare.size() == num_simd);
  const auto& bit_pshare = this->input_bool_->get_public_share();
  assert(bit_pshare.GetSize() == num_simd);

  const auto& sshare = this->output_->get_secret_share();
  std::vector<T> pshare(num_simd);

  for (std::size_t simd_j = 0; simd_j < num_simd; ++simd_j) {
    T Delta_b = bit_pshare.Get(simd_j);
    auto Delta_n = int_pshare[simd_j];
    pshare[simd_j] = delta_b_share_[simd_j] * (Delta_n - 2 * Delta_b * Delta_n) -
                    Delta_b * int_sshare[simd_j] -
                    delta_b_x_delta_n_share_[simd_j] * (1 - 2 * Delta_b) + sshare[simd_j];
    if (beavy_provider_.is_my_job(this->gate_id_)) {
      pshare[simd_j] += Delta_b * Delta_n;
    }
  }

  beavy_provider_.broadcast_ints_message(this->gate_id_, pshare);
  const auto other_pshare = share_future_.get();
  std::transform(std::begin(pshare), std::end(pshare), std::begin(other_pshare), std::begin(pshare),
                 std::plus{});

  this->output_->get_public_share() = std::move(pshare);
  this->output_->set_online_ready();

  if constexpr (MOTION_VERBOSE_DEBUG) {
    auto logger = beavy_provider_.get_logger();
    if (logger) {
      logger->LogTrace(fmt::format(
          "Gate {}: BooleanXArithmeticBEAVYMULGate<T>::evaluate_online end", this->gate_id_));
    }
  }
}

template class BooleanXArithmeticBEAVYMULGate<std::uint8_t>;
template class BooleanXArithmeticBEAVYMULGate<std::uint16_t>;
template class BooleanXArithmeticBEAVYMULGate<std::uint32_t>;
template class BooleanXArithmeticBEAVYMULGate<std::uint64_t>;

}  // namespace MOTION::proto::beavy
