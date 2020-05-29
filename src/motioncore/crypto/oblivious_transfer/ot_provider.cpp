// MIT License
//
// Copyright (c) 2019 Oleksandr Tkachenko
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

#include "ot_provider.h"

#include "communication/communication_layer.h"
#include "communication/fbs_headers/ot_extension_generated.h"
#include "communication/message_handler.h"
#include "communication/ot_extension_message.h"
#include "crypto/base_ots/base_ot_provider.h"
#include "crypto/motion_base_provider.h"
#include "crypto/pseudo_random_generator.h"
#include "data_storage/base_ot_data.h"
#include "data_storage/ot_extension_data.h"
#include "ot_flavors.h"
#include "utility/bit_matrix.h"
#include "utility/config.h"
#include "utility/fiber_condition.h"
#include "utility/logger.h"

namespace ENCRYPTO::ObliviousTransfer {

OTProvider::OTProvider(std::function<void(flatbuffers::FlatBufferBuilder &&)> Send,
                       MOTION::OTExtensionData &data, std::size_t party_id,
                       std::shared_ptr<MOTION::Logger> logger)
    : Send_(Send),
      data_(data),
      receiver_provider_(data_.GetReceiverData(), party_id, logger),
      sender_provider_(data_.GetSenderData(), party_id, logger) {}

void OTProvider::WaitSetup() const {
  data_.GetReceiverData().setup_finished_cond_->Wait();
  data_.GetSenderData().setup_finished_cond_->Wait();
}

[[nodiscard]] std::unique_ptr<FixedXCOT128Sender> OTProvider::RegisterSendFixedXCOT128(
    std::size_t num_ots) {
  return sender_provider_.RegisterFixedXCOT128s(num_ots, Send_);
}

[[nodiscard]] std::unique_ptr<XCOTBitSender> OTProvider::RegisterSendXCOTBit(std::size_t num_ots) {
  return sender_provider_.RegisterXCOTBits(num_ots, Send_);
}

template <typename T>
[[nodiscard]] std::unique_ptr<ACOTSender<T>> OTProvider::RegisterSendACOT(std::size_t num_ots,
                                                                          std::size_t vector_size) {
  return sender_provider_.RegisterACOT<T>(num_ots, vector_size, Send_);
}

template std::unique_ptr<ACOTSender<std::uint8_t>> OTProvider::RegisterSendACOT(
    std::size_t num_ots, std::size_t vector_size);
template std::unique_ptr<ACOTSender<std::uint16_t>> OTProvider::RegisterSendACOT(
    std::size_t num_ots, std::size_t vector_size);
template std::unique_ptr<ACOTSender<std::uint32_t>> OTProvider::RegisterSendACOT(
    std::size_t num_ots, std::size_t vector_size);
template std::unique_ptr<ACOTSender<std::uint64_t>> OTProvider::RegisterSendACOT(
    std::size_t num_ots, std::size_t vector_size);
template std::unique_ptr<ACOTSender<__uint128_t>> OTProvider::RegisterSendACOT(
    std::size_t num_ots, std::size_t vector_size);

[[nodiscard]] std::unique_ptr<GOT128Sender> OTProvider::RegisterSendGOT128(std::size_t num_ots) {
  return sender_provider_.RegisterGOT128(num_ots, Send_);
}

[[nodiscard]] std::unique_ptr<GOTBitSender> OTProvider::RegisterSendGOTBit(std::size_t num_ots) {
  return sender_provider_.RegisterGOTBit(num_ots, Send_);
}

[[nodiscard]] std::unique_ptr<FixedXCOT128Receiver> OTProvider::RegisterReceiveFixedXCOT128(
    std::size_t num_ots) {
  return receiver_provider_.RegisterFixedXCOT128s(num_ots, Send_);
}

[[nodiscard]] std::unique_ptr<XCOTBitReceiver> OTProvider::RegisterReceiveXCOTBit(
    std::size_t num_ots) {
  return receiver_provider_.RegisterXCOTBits(num_ots, Send_);
}

template <typename T>
[[nodiscard]] std::unique_ptr<ACOTReceiver<T>> OTProvider::RegisterReceiveACOT(
    std::size_t num_ots, std::size_t vector_size) {
  return receiver_provider_.RegisterACOT<T>(num_ots, vector_size, Send_);
}

template std::unique_ptr<ACOTReceiver<std::uint8_t>> OTProvider::RegisterReceiveACOT(
    std::size_t num_ots, std::size_t vector_size);
template std::unique_ptr<ACOTReceiver<std::uint16_t>> OTProvider::RegisterReceiveACOT(
    std::size_t num_ots, std::size_t vector_size);
template std::unique_ptr<ACOTReceiver<std::uint32_t>> OTProvider::RegisterReceiveACOT(
    std::size_t num_ots, std::size_t vector_size);
template std::unique_ptr<ACOTReceiver<std::uint64_t>> OTProvider::RegisterReceiveACOT(
    std::size_t num_ots, std::size_t vector_size);
template std::unique_ptr<ACOTReceiver<__uint128_t>> OTProvider::RegisterReceiveACOT(
    std::size_t num_ots, std::size_t vector_size);

[[nodiscard]] std::unique_ptr<GOT128Receiver> OTProvider::RegisterReceiveGOT128(
    std::size_t num_ots) {
  return receiver_provider_.RegisterGOT128(num_ots, Send_);
}

[[nodiscard]] std::unique_ptr<GOTBitReceiver> OTProvider::RegisterReceiveGOTBit(
    std::size_t num_ots) {
  return receiver_provider_.RegisterGOTBit(num_ots, Send_);
}

OTProviderFromOTExtension::OTProviderFromOTExtension(
    std::function<void(flatbuffers::FlatBufferBuilder &&)> Send, MOTION::OTExtensionData &data,
    const MOTION::BaseOTsData &base_ot_data,
    MOTION::Crypto::MotionBaseProvider &motion_base_provider, std::size_t party_id,
    std::shared_ptr<MOTION::Logger> logger)
    : OTProvider(Send, data, party_id, logger),
      base_ot_data_(base_ot_data),
      motion_base_provider_(motion_base_provider) {
  auto &ot_ext_rcv = data_.GetReceiverData();
  ot_ext_rcv.real_choices_ = std::make_unique<BitVector<>>();
}

void OTProviderFromOTExtension::SendSetup() {
  // security parameter
  constexpr std::size_t kappa = 128;

  // storage for sender and base OT receiver data
  const auto &base_ots_rcv = base_ot_data_.GetReceiverData();
  auto &ot_ext_snd = data_.GetSenderData();

  // number of OTs after extension
  // == width of the bit matrix
  const std::size_t bit_size = sender_provider_.GetNumOTs();
  if (bit_size == 0) return;  // no OTs needed
  ot_ext_snd.bit_size_ = bit_size;

  // XXX: index variable?
  std::size_t i;

  // bit size of the matrix rounded to bytes
  // XXX: maybe round to blocks?
  const std::size_t byte_size = MOTION::Helpers::Convert::BitsToBytes(bit_size);

  // bit size rounded to blocks
  const auto bit_size_padded = bit_size + kappa - (bit_size % kappa);

  // vector containing the matrix rows
  // XXX: note that rows/columns are swapped compared to the ALSZ paper
  std::vector<AlignedBitVector> v(kappa);

  // PRG which is used to expand the keys we got from the base OTs
  PRG prgs_var_key;
  //// fill the rows of the matrix
  for (i = 0; i < kappa; ++i) {
    // use the key we got from the base OTs as seed
    prgs_var_key.SetKey(base_ots_rcv.messages_c_.at(i).data());
    // change the offset in the output stream since we might have already used
    // the same base OTs previously
    prgs_var_key.SetOffset(base_ots_rcv.consumed_offset_);
    // expand the seed such that it fills one row of the matrix
    auto row(prgs_var_key.Encrypt(byte_size));
    v[i] = AlignedBitVector(std::move(row), bit_size_padded);
  }

  // receive the vectors u one by one from the receiver
  // and xor them to the expanded keys if the corresponding selection bit is 1
  // transmitted one by one to prevent waiting for finishing all messages to start sending
  // the vectors can be transmitted in the wrong order

  for (auto it = ot_ext_snd.u_futures_.begin(); it < ot_ext_snd.u_futures_.end(); ++it) {
    const std::size_t u_id{it->get()};
    if (base_ots_rcv.c_[u_id]) {
      const auto &u = ot_ext_snd.u_[u_id];
      ENCRYPTO::BitSpan bs(v[u_id].GetMutableData().data(), bit_size, true);
      bs ^= u;
    }
  }

  // delete the allocated memory
  ot_ext_snd.u_ = {};

  // array with pointers to each row of the matrix
  std::array<const std::byte *, kappa> ptrs;
  for (i = 0u; i < ptrs.size(); ++i) {
    ptrs[i] = v[i].GetData().data();
  }

  motion_base_provider_.setup();
  const auto &fixed_key_aes_key = motion_base_provider_.get_aes_fixed_key();

  // for each (extended) OT i
  PRG prg_fixed_key;
  prg_fixed_key.SetKey(fixed_key_aes_key.data());

  // transpose the bit matrix
  // XXX: figure out how the result looks like
  BitMatrix::SenderTransposeAndEncrypt(ptrs, ot_ext_snd.y0_, ot_ext_snd.y1_, base_ots_rcv.c_,
                                       prg_fixed_key, bit_size_padded, ot_ext_snd.bitlengths_);
  /*
    for (i = 0; i < ot_ext_snd.bitlengths_.size(); ++i) {
      // here we want to store the sender's outputs
      // XXX: why are the y0_, y1_ vectors resized every time new ots are registered?
      auto &out0 = ot_ext_snd.y0_[i];
      auto &out1 = ot_ext_snd.y1_[i];

      // bit length of the OT
      const auto bitlen = ot_ext_snd.bitlengths_[i];

      // in which of the above "rows" can we find the block
      const auto row_i = i % kappa;
      // where in the "row" do we have to look for the block
      const auto blk_offset = ((kappa / 8) * (i / kappa));
      auto V_row = reinterpret_cast<std::byte *>(
          __builtin_assume_aligned(ptrs.at(row_i) + blk_offset, MOTION::MOTION_ALIGNMENT));

      // compute the sender outputs
      if (bitlen <= kappa) {
        // the bit length is smaller than 128 bit
        out0 = BitVector<>(prg_fixed_key.FixedKeyAES(V_row, i), bitlen);

        auto out1_in = base_ots_rcv.c_ ^ BitSpan(V_row, kappa, true);
        out1 = BitVector<>(prg_fixed_key.FixedKeyAES(out1_in.GetData().data(), i), bitlen);
      } else {
        // string OT with bit length > 128 bit
        // -> do seed compression and send later only 128 bit seeds
        auto seed0 = prg_fixed_key.FixedKeyAES(V_row, i);
        prgs_var_key.SetKey(seed0.data());
        out0 =
            BitVector<>(prgs_var_key.Encrypt(MOTION::Helpers::Convert::BitsToBytes(bitlen)),
    bitlen);

        auto out1_in = base_ots_rcv.c_ ^ BitSpan(V_row, kappa, true);
        auto seed1 = prg_fixed_key.FixedKeyAES(out1_in.GetData().data(), i);
        prgs_var_key.SetKey(seed1.data());
        out1 =
            BitVector<>(prgs_var_key.Encrypt(MOTION::Helpers::Convert::BitsToBytes(bitlen)),
    bitlen);
      }
    }*/

  // we are done with the setup for the sender side
  {
    std::scoped_lock(ot_ext_snd.setup_finished_cond_->GetMutex());
    ot_ext_snd.setup_finished_ = true;
  }
  ot_ext_snd.setup_finished_cond_->NotifyAll();
}  // namespace ENCRYPTO::ObliviousTransfer

void OTProviderFromOTExtension::ReceiveSetup() {
  // some index variables
  std::size_t i = 0, j = 0;
  // security parameter and number of base OTs
  constexpr std::size_t kappa = 128;
  // number of OTs and width of the bit matrix
  const std::size_t bit_size = receiver_provider_.GetNumOTs();
  if (bit_size == 0) return;  // nothing to do

  // rounded up to a multiple of the security parameter
  const auto bit_size_padded = bit_size + kappa - (bit_size % kappa);

  // convert to bytes
  const std::size_t byte_size = MOTION::Helpers::Convert::BitsToBytes(bit_size);
  // XXX: if byte_size is 0 then bit_size was also zero (or an overflow happened
  if (byte_size == 0) {
    return;
  }
  // storage for receiver and base OT sender data
  const auto &base_ots_snd = base_ot_data_.GetSenderData();
  auto &ot_ext_rcv = data_.GetReceiverData();

  // make random choices (this is precomputation, real inputs are not known yet)
  ot_ext_rcv.random_choices_ =
      std::make_unique<AlignedBitVector>(AlignedBitVector::Random(bit_size));

  // create matrix with kappa rows
  std::vector<AlignedBitVector> v(kappa);

  // PRG we use with the fixed-key AES function

  // PRG which is used to expand the keys we got from the base OTs
  PRG prg_fixed_key, prg_var_key;
  // fill the rows of the matrix
  for (i = 0; i < kappa; ++i) {
    // generate rows of the matrix using the corresponding 0 key
    // T[j] = PRG(s_{j,0})
    prg_var_key.SetKey(base_ots_snd.messages_0_.at(i).data());
    // change the offset in the output stream since we might have already used
    // the same base OTs previously
    prg_var_key.SetOffset(base_ots_snd.consumed_offset_);
    // expand the seed such that it fills one row of the matrix
    auto row(prg_var_key.Encrypt(byte_size));
    v.at(i) = AlignedBitVector(std::move(row), bit_size);
    // take a copy of the row and XOR it with our choices
    auto u = v.at(i);
    // u_j = T[j] XOR r
    u ^= *ot_ext_rcv.random_choices_;

    // now mask the result with random stream expanded from the 1 key
    // u_j = u_j XOR PRG(s_{j,1})
    prg_var_key.SetKey(base_ots_snd.messages_1_.at(i).data());
    prg_var_key.SetOffset(base_ots_snd.consumed_offset_);
    u ^= AlignedBitVector(prg_var_key.Encrypt(byte_size), bit_size);

    // send this row
    Send_(MOTION::Communication::BuildOTExtensionMessageReceiverMasks(u.GetData().data(),
                                                                      u.GetData().size(), i));
  }

  // transpose matrix T
  if (bit_size_padded != bit_size) {
    for (i = 0u; i < v.size(); ++i) {
      v.at(i).Resize(bit_size_padded, true);
    }
  }

  std::array<const std::byte *, kappa> ptrs;
  for (j = 0; j < ptrs.size(); ++j) {
    ptrs.at(j) = v.at(j).GetMutableData().data();
  }

  motion_base_provider_.setup();
  const auto &fixed_key_aes_key = motion_base_provider_.get_aes_fixed_key();
  prg_fixed_key.SetKey(fixed_key_aes_key.data());
  BitMatrix::ReceiverTransposeAndEncrypt(ptrs, ot_ext_rcv.outputs_, prg_fixed_key, bit_size_padded,
                                         ot_ext_rcv.bitlengths_);
  /*BitMatrix::TransposeUsingBitSlicing(ptrs, bit_size_padded);
  for (i = 0; i < ot_ext_rcv.outputs_.size(); ++i) {
    const auto row_i = i % kappa;
    const auto blk_offset = ((kappa / 8) * (i / kappa));
    const auto T_row = ptrs.at(row_i) + blk_offset;
    auto &out = ot_ext_rcv.outputs_.at(i);

    std::unique_lock lock(ot_ext_rcv.bitlengths_mutex_);
    const std::size_t bitlen = ot_ext_rcv.bitlengths_.at(i);
    lock.unlock();

    if (bitlen <= kappa) {
      out = BitVector<>(prg_fixed_key.FixedKeyAES(T_row, i), bitlen);
    } else {
      auto seed = prg_fixed_key.FixedKeyAES(T_row, i);
      prg_var_key.SetKey(seed.data());
      out = BitVector<>(prg_var_key.Encrypt(MOTION::Helpers::Convert::BitsToBytes(bitlen)), bitlen);
    }
  }*/

  {
    std::scoped_lock(ot_ext_rcv.setup_finished_cond_->GetMutex());
    ot_ext_rcv.setup_finished_ = true;
  }
  ot_ext_rcv.setup_finished_cond_->NotifyAll();
}

OTVector::OTVector(const std::size_t ot_id, const std::size_t num_ots, const std::size_t bitlen,
                   const OTProtocol p,
                   const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : ot_id_(ot_id), num_ots_(num_ots), bitlen_(bitlen), p_(p), Send_(Send) {}

const std::vector<BitVector<>> &OTVectorSender::GetOutputs() {
  WaitSetup();
  if (outputs_.empty()) {
    outputs_.reserve(num_ots_);
    for (auto i = 0ull; i < num_ots_; ++i) {
      ENCRYPTO::BitVector bv;
      bv.Reserve(MOTION::Helpers::Convert::BitsToBytes(data_.y0_.at(ot_id_ + i).GetSize() * 2));
      bv.Append(data_.y0_.at(ot_id_ + i));
      bv.Append(data_.y1_.at(ot_id_ + i));
      outputs_.emplace_back(std::move(bv));
    }
  }
  return outputs_;
}

void OTVectorSender::WaitSetup() { data_.setup_finished_cond_->Wait(); }

OTVectorSender::OTVectorSender(const std::size_t ot_id, const std::size_t num_ots,
                               const std::size_t bitlen, const OTProtocol p,
                               MOTION::OTExtensionSenderData &data,
                               const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : OTVector(ot_id, num_ots, bitlen, p, Send), data_(data) {
  Reserve(ot_id, num_ots, bitlen);
}

void OTVectorSender::Reserve(const std::size_t id, const std::size_t num_ots,
                             const std::size_t bitlen) {
  data_.y0_.resize(data_.y0_.size() + num_ots);
  data_.y1_.resize(data_.y1_.size() + num_ots);
  data_.bitlengths_.resize(data_.bitlengths_.size() + num_ots);
  data_.corrections_.Resize(data_.corrections_.GetSize() + num_ots);
  for (auto i = 0ull; i < num_ots; ++i) {
    data_.bitlengths_.at(data_.bitlengths_.size() - 1 - i) = bitlen;
  }
  data_.num_ots_in_batch_.emplace(id, num_ots);
}

GOTVectorSender::GOTVectorSender(const std::size_t ot_id, const std::size_t num_ots,
                                 const std::size_t bitlen, MOTION::OTExtensionSenderData &data,
                                 const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : OTVectorSender(ot_id, num_ots, bitlen, OTProtocol::GOT, data, Send) {
  data_.received_correction_offsets_cond_.emplace(
      ot_id_, std::make_unique<FiberCondition>([ot_id, this]() {
        std::scoped_lock lock(data_.corrections_mutex_);
        return data_.received_correction_offsets_.find(ot_id) !=
               data_.received_correction_offsets_.end();
      }));
}

void GOTVectorSender::SetInputs(std::vector<BitVector<>> &&v) {
  for ([[maybe_unused]] auto &bv : v) {
    assert(bv.GetSize() == (bitlen_ * 2));
  }
  inputs_ = std::move(v);
  outputs_ = inputs_;
}

void GOTVectorSender::SetInputs(const std::vector<BitVector<>> &v) {
  for ([[maybe_unused]] auto &bv : v) {
    assert(bv.GetSize() == (bitlen_ * 2));
  }
  inputs_ = v;
  outputs_ = inputs_;
}

// blocking wait for correction bits
void GOTVectorSender::SendMessages() {
  assert(!inputs_.empty());
  WaitSetup();
  const auto &ot_ext_snd = data_;
  ot_ext_snd.received_correction_offsets_cond_.at(ot_id_)->Wait();
  std::unique_lock lock(ot_ext_snd.corrections_mutex_);
  const auto corrections = ot_ext_snd.corrections_.Subset(ot_id_, ot_id_ + num_ots_);
  lock.unlock();
  assert(inputs_.size() == corrections.GetSize());

  BitVector<> buffer;
  for (auto i = 0ull; i < num_ots_; ++i) {
    const auto bv_0 = inputs_.at(i).Subset(0, bitlen_);
    const auto bv_1 = inputs_.at(i).Subset(bitlen_, bitlen_ * 2);
    if (corrections[i]) {
      buffer.Append(bv_1 ^ ot_ext_snd.y0_.at(ot_id_ + i));
      buffer.Append(bv_0 ^ ot_ext_snd.y1_.at(ot_id_ + i));
    } else {
      buffer.Append(bv_0 ^ ot_ext_snd.y0_.at(ot_id_ + i));
      buffer.Append(bv_1 ^ ot_ext_snd.y1_.at(ot_id_ + i));
    }
  }
  Send_(MOTION::Communication::BuildOTExtensionMessageSender(buffer.GetData().data(),
                                                             buffer.GetData().size(), ot_id_));
}

COTVectorSender::COTVectorSender(const std::size_t id, const std::size_t num_ots,
                                 const std::size_t bitlen, OTProtocol p,
                                 MOTION::OTExtensionSenderData &data,
                                 const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : OTVectorSender(id, num_ots, bitlen, p, data, Send) {
  if (p == OTProtocol::ACOT &&
      (bitlen != 8u && bitlen != 16u && bitlen != 32u && bitlen != 64u && bitlen != 128)) {
    throw std::runtime_error(fmt::format(
        "Invalid parameter bitlen={}, only 8, 16, 32, 64, or 128 are allowed in ACOT", bitlen_));
  }
  data_.received_correction_offsets_cond_.emplace(
      ot_id_, std::make_unique<FiberCondition>([this]() {
        std::scoped_lock lock(data_.corrections_mutex_);
        return data_.received_correction_offsets_.find(ot_id_) !=
               data_.received_correction_offsets_.end();
      }));
}

void COTVectorSender::SetInputs(std::vector<BitVector<>> &&v) {
  for ([[maybe_unused]] auto &bv : v) {
    assert(bv.GetSize() == (bitlen_));
  }
  inputs_ = std::move(v);
}

void COTVectorSender::SetInputs(const std::vector<BitVector<>> &v) {
  for ([[maybe_unused]] auto &bv : v) {
    assert(bv.GetSize() == (bitlen_));
  }
  inputs_ = v;
}

const std::vector<BitVector<>> &COTVectorSender::GetOutputs() {
  if (inputs_.empty()) {
    throw std::runtime_error("Inputs have to be chosen before calling GetOutputs()");
  }
  WaitSetup();
  const auto &ot_ext_snd = data_;
  ot_ext_snd.received_correction_offsets_cond_.at(ot_id_)->Wait();
  if (outputs_.empty()) {
    outputs_.reserve(num_ots_);
    std::unique_lock lock(ot_ext_snd.corrections_mutex_);
    const auto corrections = ot_ext_snd.corrections_.Subset(ot_id_, ot_id_ + num_ots_);
    lock.unlock();
    for (auto i = 0ull; i < num_ots_; ++i) {
      BitVector<> bv;
      bv.Reserve(
          MOTION::Helpers::Convert::BitsToBytes(ot_ext_snd.y1_.at(ot_id_ + i).GetSize() * 2));
      if (corrections[i]) {
        bv.Append(ot_ext_snd.y1_.at(ot_id_ + i));
      } else {
        bv.Append(ot_ext_snd.y0_.at(ot_id_ + i));
      }
      if (p_ == OTProtocol::ACOT) {
        if (corrections[i]) {
          bv.Append(ot_ext_snd.y1_.at(ot_id_ + i));
        } else {
          bv.Append(ot_ext_snd.y0_.at(ot_id_ + i));
        }
        switch (bitlen_) {
          case (8u): {
            *reinterpret_cast<uint8_t *>(bv.GetMutableData().data() + 1) +=
                *reinterpret_cast<const uint8_t *>(inputs_.at(i).GetData().data());
            break;
          }
          case (16u): {
            *reinterpret_cast<uint16_t *>(bv.GetMutableData().data() + 2) +=
                *reinterpret_cast<const uint16_t *>(inputs_.at(i).GetData().data());
            break;
          }
          case (32u): {
            *reinterpret_cast<uint32_t *>(bv.GetMutableData().data() + 4) +=
                *reinterpret_cast<const uint32_t *>(inputs_.at(i).GetData().data());
            break;
          }
          case (64u): {
            *reinterpret_cast<uint64_t *>(bv.GetMutableData().data() + 8) +=
                *reinterpret_cast<const uint64_t *>(inputs_.at(i).GetData().data());
            break;
          }
          case (128u): {
            *reinterpret_cast<__uint128_t *>(bv.GetMutableData().data() + 16) +=
                *reinterpret_cast<const __uint128_t *>(inputs_.at(i).GetData().data());
            break;
          }
        }
      } else {  // OTProtocol::XCOT
        bv.Append(inputs_.at(i) ^ bv);
      }
      outputs_.emplace_back(std::move(bv));
    }
  }
  return outputs_;
}

void COTVectorSender::SendMessages() {
  if (inputs_.empty()) {
    throw std::runtime_error("Inputs have to be chosen before calling SendMessages()");
  }
  WaitSetup();
  const auto &ot_ext_snd = data_;
  BitVector<> buffer;
  std::size_t ot_batch_bit_size = 0;
  for (auto i = 0ull; i < num_ots_; ++i)
    ot_batch_bit_size += ot_ext_snd.y0_.at(ot_id_ + i).GetSize();
  buffer.Reserve(MOTION::Helpers::Convert::BitsToBytes(ot_batch_bit_size));
  for (auto i = 0ull; i < num_ots_; ++i) {
    if (p_ == OTProtocol::ACOT) {
      BitVector bv = ot_ext_snd.y0_.at(ot_id_ + i);
      switch (bitlen_) {
        case 8u: {
          *(reinterpret_cast<std::uint8_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const std::uint8_t *>(inputs_.at(i).GetData().data()));
          *(reinterpret_cast<std::uint8_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const std::uint8_t *>(
                  ot_ext_snd.y1_.at(ot_id_ + i).GetData().data()));

          break;
        }
        case 16u: {
          *(reinterpret_cast<std::uint16_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const std::uint16_t *>(inputs_.at(i).GetData().data()));
          *(reinterpret_cast<std::uint16_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const std::uint16_t *>(
                  ot_ext_snd.y1_.at(ot_id_ + i).GetData().data()));
          break;
        }
        case 32u: {
          *(reinterpret_cast<std::uint32_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const std::uint32_t *>(inputs_.at(i).GetData().data()));
          *(reinterpret_cast<std::uint32_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const std::uint32_t *>(
                  ot_ext_snd.y1_.at(ot_id_ + i).GetData().data()));
          break;
        }
        case 64u: {
          *(reinterpret_cast<std::uint64_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const std::uint64_t *>(inputs_.at(i).GetData().data()));
          *(reinterpret_cast<std::uint64_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const std::uint64_t *>(
                  ot_ext_snd.y1_.at(ot_id_ + i).GetData().data()));
          break;
        }
        case 128u: {
          *(reinterpret_cast<__uint128_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const __uint128_t *>(inputs_.at(i).GetData().data()));
          *(reinterpret_cast<__uint128_t *>(bv.GetMutableData().data())) +=
              *(reinterpret_cast<const __uint128_t *>(
                  ot_ext_snd.y1_.at(ot_id_ + i).GetData().data()));
          break;
        }
        default: {
          throw std::runtime_error(fmt::format("Unsupported bitlength {}", bitlen_));
        }
      }
      buffer.Append(bv);
    } else if (p_ == OTProtocol::XCOT) {
      buffer.Append(inputs_.at(i) ^ ot_ext_snd.y0_.at(ot_id_ + i) ^ ot_ext_snd.y1_.at(ot_id_ + i));
    } else {
      throw std::runtime_error("Unknown OT protocol");
    }
  }
  Send_(MOTION::Communication::BuildOTExtensionMessageSender(buffer.GetData().data(),
                                                             buffer.GetData().size(), ot_id_));
}

ROTVectorSender::ROTVectorSender(const std::size_t ot_id, const std::size_t num_ots,
                                 const std::size_t bitlen, MOTION::OTExtensionSenderData &data,
                                 const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : OTVectorSender(ot_id, num_ots, bitlen, OTProtocol::ROT, data, Send) {}

void ROTVectorSender::SetInputs([[maybe_unused]] std::vector<BitVector<>> &&v) {
  throw std::runtime_error("Inputs are random in ROT and thus cannot be set");
}

void ROTVectorSender::SetInputs([[maybe_unused]] const std::vector<BitVector<>> &v) {
  throw std::runtime_error("Inputs are random in ROT and thus cannot be set");
}

void ROTVectorSender::SendMessages() {
  throw std::runtime_error("Inputs in ROT are available locally and thus do not need to be sent");
}

void OTVectorReceiver::WaitSetup() { data_.setup_finished_cond_->Wait(); }

OTVectorReceiver::OTVectorReceiver(const std::size_t ot_id, const std::size_t num_ots,
                                   const std::size_t bitlen, const OTProtocol p,
                                   MOTION::OTExtensionReceiverData &data,
                                   std::function<void(flatbuffers::FlatBufferBuilder &&)> Send)
    : OTVector(ot_id, num_ots, bitlen, p, Send), data_(data) {
  Reserve(ot_id, num_ots, bitlen);
}

void OTVectorReceiver::Reserve(const std::size_t id, const std::size_t num_ots,
                               const std::size_t bitlen) {
  data_.outputs_.resize(id + num_ots);
  data_.bitlengths_.resize(id + num_ots);
  for (auto i = 0ull; i < num_ots; ++i) {
    data_.bitlengths_.at(id + i) = bitlen;
  }
  data_.num_ots_in_batch_.emplace(id, num_ots);
}

GOTVectorReceiver::GOTVectorReceiver(
    const std::size_t ot_id, const std::size_t num_ots, const std::size_t bitlen,
    MOTION::OTExtensionReceiverData &data,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : OTVectorReceiver(ot_id, num_ots, bitlen, OTProtocol::GOT, data, Send) {
  std::scoped_lock lock(data_.num_messages_mutex_);
  data_.num_messages_.emplace(ot_id_, 2);
}

void GOTVectorReceiver::SetChoices(BitVector<> &&v) {
  assert(v.GetSize() == num_ots_);
  choices_ = std::move(v);
  {
    std::scoped_lock lock(data_.real_choices_mutex_,
                          data_.real_choices_cond_.at(ot_id_)->GetMutex());
    data_.real_choices_->Copy(ot_id_, choices_);
    data_.set_real_choices_.emplace(ot_id_);
  }
  data_.real_choices_cond_.at(ot_id_)->NotifyOne();
  choices_flag_ = true;
}

void GOTVectorReceiver::SetChoices(const BitVector<> &v) {
  assert(v.GetSize() == num_ots_);
  choices_ = v;
  {
    std::scoped_lock lock(data_.real_choices_mutex_,
                          data_.real_choices_cond_.at(ot_id_)->GetMutex());
    data_.real_choices_->Copy(ot_id_, choices_);
    data_.set_real_choices_.emplace(ot_id_);
  }
  data_.real_choices_cond_.at(ot_id_)->NotifyOne();
  choices_flag_ = true;
}

void GOTVectorReceiver::SendCorrections() {
  if (choices_.Empty()) {
    throw std::runtime_error("Choices in GOT must be set before calling SendCorrections()");
  }

  auto corrections = choices_ ^ data_.random_choices_->Subset(ot_id_, ot_id_ + num_ots_);
  Send_(MOTION::Communication::BuildOTExtensionMessageReceiverCorrections(
      corrections.GetData().data(), corrections.GetData().size(), ot_id_));
  corrections_sent_ = true;
}

const std::vector<BitVector<>> &GOTVectorReceiver::GetOutputs() {
  if (!corrections_sent_) {
    throw std::runtime_error("In GOT, corrections must be set before calling GetOutputs()");
  }
  WaitSetup();
  const auto &ot_ext_rcv = data_;
  ot_ext_rcv.output_conds_.at(ot_id_)->Wait();
  if (messages_.empty()) {
    for (auto i = 0ull; i < num_ots_; ++i) {
      if (ot_ext_rcv.outputs_.at(ot_id_ + i).GetSize() > 0) {
        messages_.emplace_back(std::move(ot_ext_rcv.outputs_.at(ot_id_ + i)));
      }
    }
  }
  return messages_;
}

COTVectorReceiver::COTVectorReceiver(
    const std::size_t ot_id, const std::size_t num_ots, const std::size_t bitlen, OTProtocol p,
    MOTION::OTExtensionReceiverData &data,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : OTVectorReceiver(ot_id, num_ots, bitlen, p, data, Send) {
  if (p == OTProtocol::ACOT &&
      (bitlen != 8u && bitlen != 16u && bitlen != 32u && bitlen != 64u && bitlen != 128u)) {
    throw std::runtime_error(fmt::format(
        "Invalid parameter bitlen={}, only 8, 16, 32, 64, or 128 are allowed in ACOT", bitlen_));
  }
  {
    std::scoped_lock lock(data_.num_messages_mutex_);
    data_.num_messages_.emplace(ot_id_, 1);
  }
  if (p == OTProtocol::XCOT) {
    data_.xor_correlation_.emplace(ot_id_);
  }
}

void COTVectorReceiver::SendCorrections() {
  if (choices_.Empty()) {
    throw std::runtime_error("Choices in COT must be set before calling SendCorrections()");
  }
  auto corrections = choices_ ^ data_.random_choices_->Subset(ot_id_, ot_id_ + num_ots_);
  Send_(MOTION::Communication::BuildOTExtensionMessageReceiverCorrections(
      corrections.GetData().data(), corrections.GetData().size(), ot_id_));
  corrections_sent_ = true;
}

void COTVectorReceiver::SetChoices(BitVector<> &&v) {
  choices_ = std::move(v);
  {
    std::scoped_lock lock(data_.real_choices_mutex_,
                          data_.real_choices_cond_.at(ot_id_)->GetMutex());
    data_.real_choices_->Copy(ot_id_, choices_);
    data_.set_real_choices_.emplace(ot_id_);
  }
  data_.real_choices_cond_.at(ot_id_)->NotifyOne();
  choices_flag_ = true;
}

void COTVectorReceiver::SetChoices(const BitVector<> &v) {
  choices_ = v;
  {
    std::scoped_lock lock(data_.real_choices_mutex_,
                          data_.real_choices_cond_.at(ot_id_)->GetMutex());
    data_.real_choices_->Copy(ot_id_, choices_);
    data_.set_real_choices_.emplace(ot_id_);
  }
  data_.real_choices_cond_.at(ot_id_)->NotifyOne();
  choices_flag_ = true;
}

const std::vector<BitVector<>> &COTVectorReceiver::GetOutputs() {
  if (!corrections_sent_) {
    throw std::runtime_error("In COT, corrections must be set before calling GetOutputs()");
  }
  WaitSetup();
  data_.output_conds_.at(ot_id_)->Wait();

  if (messages_.empty()) {
    messages_.reserve(num_ots_);
    for (auto i = 0ull; i < num_ots_; ++i) {
      if (data_.outputs_.at(ot_id_ + i).GetSize() > 0) {
        messages_.emplace_back(std::move(data_.outputs_.at(ot_id_ + i)));
      }
    }
  }
  return messages_;
}

ROTVectorReceiver::ROTVectorReceiver(
    const std::size_t ot_id, const std::size_t num_ots, const std::size_t bitlen,
    MOTION::OTExtensionReceiverData &data,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : OTVectorReceiver(ot_id, num_ots, bitlen, OTProtocol::ROT, data, Send) {
  Reserve(ot_id, num_ots, bitlen);
}

void ROTVectorReceiver::SetChoices([[maybe_unused]] const BitVector<> &v) {
  throw std::runtime_error("Choices are random in ROT and thus cannot be set");
}

void ROTVectorReceiver::SetChoices([[maybe_unused]] BitVector<> &&v) {
  throw std::runtime_error("Choices are random in ROT and thus cannot be set");
}

void ROTVectorReceiver::SendCorrections() {
  throw std::runtime_error(
      "Choices are random in ROT and thus there is no need for correction bits");
}

const BitVector<> &ROTVectorReceiver::GetChoices() {
  WaitSetup();
  if (choices_.Empty()) {
    const auto a_bv = data_.random_choices_->Subset(ot_id_, ot_id_ + num_ots_);
    choices_ = BitVector<>(a_bv.GetData().data(), a_bv.GetSize());
  }
  return choices_;
}

const std::vector<BitVector<>> &ROTVectorReceiver::GetOutputs() {
  WaitSetup();
  if (messages_.empty()) {
    const auto data = data_.outputs_.begin();
    messages_.assign(data + ot_id_, data + ot_id_ + num_ots_);
  }
  return messages_;
}

std::shared_ptr<OTVectorSender> &OTProviderSender::GetOTs(std::size_t offset) {
  auto iterator = sender_data_.find(offset);
  if (iterator == sender_data_.end()) {
    throw std::runtime_error(fmt::format("Could not find an OTVector with offset {}", offset));
  }
  return iterator->second;
}

std::shared_ptr<OTVectorSender> &OTProviderSender::RegisterOTs(
    const std::size_t bitlen, const std::size_t num_ots, const OTProtocol p,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_;
  total_ots_count_ += num_ots;
  std::shared_ptr<OTVectorSender> ot;
  switch (p) {
    case OTProtocol::GOT: {
      ot = std::make_shared<GOTVectorSender>(i, num_ots, bitlen, data_, Send);
      if constexpr (MOTION::MOTION_DEBUG) {
        if (logger_) {
          logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender GOTs",
                                        party_id_, num_ots, bitlen));
        }
      }
      break;
    }
    case OTProtocol::ACOT: {
      ot = std::make_shared<COTVectorSender>(i, num_ots, bitlen, p, data_, Send);
      if constexpr (MOTION::MOTION_DEBUG) {
        if (logger_) {
          logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender ACOTs",
                                        party_id_, num_ots, bitlen));
        }
      }
      break;
    }
    case OTProtocol::XCOT: {
      ot = std::make_shared<COTVectorSender>(i, num_ots, bitlen, p, data_, Send);
      if constexpr (MOTION::MOTION_DEBUG) {
        if (logger_) {
          logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender XCOTs",
                                        party_id_, num_ots, bitlen));
        }
      }
      break;
    }
    case OTProtocol::ROT: {
      ot = std::make_shared<ROTVectorSender>(i, num_ots, bitlen, data_, Send);
      if constexpr (MOTION::MOTION_DEBUG) {
        if (logger_) {
          logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender ROTs",
                                        party_id_, num_ots, bitlen));
        }
      }
      break;
    }
    default:
      throw std::logic_error("Unknown OT protocol");
  }
  return sender_data_.insert(std::pair(i, ot)).first->second;
}

std::unique_ptr<FixedXCOT128Sender> OTProviderSender::RegisterFixedXCOT128s(
    const std::size_t num_ots, const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_;
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<FixedXCOT128Sender>(i, num_ots, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender FixedXCOT128s",
                                    party_id_, num_ots, 128));
    }
  }
  return ot;
}

std::unique_ptr<XCOTBitSender> OTProviderSender::RegisterXCOTBits(
    const std::size_t num_ots, const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_;
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<XCOTBitSender>(i, num_ots, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender XCOTBits",
                                    party_id_, num_ots, 1));
    }
  }
  return ot;
}

template <typename T>
std::unique_ptr<ACOTSender<T>> OTProviderSender::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_;
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<ACOTSender<T>>(i, num_ots, vector_size, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender ACOTs",
                                    party_id_, num_ots, 8 * sizeof(T)));
    }
  }
  return ot;
}

template std::unique_ptr<ACOTSender<std::uint8_t>> OTProviderSender::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);
template std::unique_ptr<ACOTSender<std::uint16_t>> OTProviderSender::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);
template std::unique_ptr<ACOTSender<std::uint32_t>> OTProviderSender::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);
template std::unique_ptr<ACOTSender<std::uint64_t>> OTProviderSender::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);
template std::unique_ptr<ACOTSender<__uint128_t>> OTProviderSender::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);

std::unique_ptr<GOT128Sender> OTProviderSender::RegisterGOT128(
    const std::size_t num_ots, const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_;
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<GOT128Sender>(i, num_ots, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender GOT128s",
                                    party_id_, num_ots, 128));
    }
  }
  return ot;
}

std::unique_ptr<GOTBitSender> OTProviderSender::RegisterGOTBit(
    const std::size_t num_ots, const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_;
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<GOTBitSender>(i, num_ots, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit sender GOTBits",
                                    party_id_, num_ots, 1));
    }
  }
  return ot;
}

void OTProviderSender::Clear() {
  // TODO: move this
  // data_storage_->GetBaseOTsData()->GetSenderData().consumed_offset_ += total_ots_count_;

  total_ots_count_ = 0;

  {
    std::scoped_lock lock(data_.setup_finished_cond_->GetMutex());
    data_.setup_finished_ = false;
  }
  {
    std::scoped_lock lock(data_.corrections_mutex_);
    data_.received_correction_offsets_.clear();
  }
}

void OTProviderSender::Reset() {
  Clear();
  // TODO
}

std::shared_ptr<OTVectorReceiver> &OTProviderReceiver::GetOTs(std::size_t offset) {
  auto iterator = receiver_data_.find(offset);
  if (iterator == receiver_data_.end()) {
    throw std::runtime_error(fmt::format("Could not find an OTVector with offset {}", offset));
  }
  return iterator->second;
}

std::shared_ptr<OTVectorReceiver> &OTProviderReceiver::RegisterOTs(
    const std::size_t bitlen, const std::size_t num_ots, const OTProtocol p,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const std::size_t i = total_ots_count_;
  total_ots_count_ += num_ots;

  if (p != OTProtocol::ROT) {
    {
      auto &&e =
          std::pair(i, std::make_unique<FiberCondition>([i, this]() {
                      std::scoped_lock lock(data_.received_outputs_mutex_);
                      return data_.received_outputs_.find(i) != data_.received_outputs_.end();
                    }));
      data_.output_conds_.insert(std::move(e));
    }
    {
      auto &&e =
          std::pair(i, std::make_unique<FiberCondition>([i, this]() {
                      std::scoped_lock lock(data_.real_choices_mutex_);
                      return data_.set_real_choices_.find(i) != data_.set_real_choices_.end();
                    }));
      std::scoped_lock lock(data_.real_choices_mutex_);
      data_.real_choices_cond_.insert(std::move(e));
    }
  }

  std::shared_ptr<OTVectorReceiver> ot;

  switch (p) {
    case OTProtocol::GOT: {
      ot = std::make_shared<GOTVectorReceiver>(i, num_ots, bitlen, data_, Send);
      if constexpr (MOTION::MOTION_DEBUG) {
        if (logger_) {
          logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit receiver GOTs",
                                        party_id_, num_ots, bitlen));
        }
      }
      break;
    }
    case OTProtocol::XCOT: {
      ot = std::make_shared<COTVectorReceiver>(i, num_ots, bitlen, p, data_, Send);
      if constexpr (MOTION::MOTION_DEBUG) {
        if (logger_) {
          logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit receiver XCOTs",
                                        party_id_, num_ots, bitlen));
        }
      }
      break;
    }
    case OTProtocol::ACOT: {
      ot = std::make_shared<COTVectorReceiver>(i, num_ots, bitlen, p, data_, Send);
      if constexpr (MOTION::MOTION_DEBUG) {
        if (logger_) {
          logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit receiver ACOTs",
                                        party_id_, num_ots, bitlen));
        }
      }
      break;
    }
    case OTProtocol::ROT: {
      ot = std::make_shared<ROTVectorReceiver>(i, num_ots, bitlen, data_, Send);
      if constexpr (MOTION::MOTION_DEBUG) {
        if (logger_) {
          logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit receiver ROTs",
                                        party_id_, num_ots, bitlen));
        }
      }
      break;
    }
    default:
      throw std::runtime_error("Unknown OT protocol");
  }

  auto &&e = std::pair(i, ot);
  data_.real_choices_->Resize(total_ots_count_, false);
  return receiver_data_.insert(std::move(e)).first->second;
}

std::unique_ptr<FixedXCOT128Receiver> OTProviderReceiver::RegisterFixedXCOT128s(
    const std::size_t num_ots, const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_.load();
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<FixedXCOT128Receiver>(i, num_ots, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(
          fmt::format("Party#{}: registered {} parallel {}-bit receiver FixedXCOT128s", party_id_,
                      num_ots, 128));
    }
  }
  return ot;
}

std::unique_ptr<XCOTBitReceiver> OTProviderReceiver::RegisterXCOTBits(
    const std::size_t num_ots, const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_.load();
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<XCOTBitReceiver>(i, num_ots, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit receiver XCOTBits",
                                    party_id_, num_ots, 1));
    }
  }
  return ot;
}

template <typename T>
std::unique_ptr<ACOTReceiver<T>> OTProviderReceiver::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_.load();
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<ACOTReceiver<T>>(i, num_ots, vector_size, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit receiver ACOTs",
                                    party_id_, num_ots, 8 * sizeof(T)));
    }
  }
  return ot;
}

template std::unique_ptr<ACOTReceiver<std::uint8_t>> OTProviderReceiver::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);
template std::unique_ptr<ACOTReceiver<std::uint16_t>> OTProviderReceiver::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);
template std::unique_ptr<ACOTReceiver<std::uint32_t>> OTProviderReceiver::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);
template std::unique_ptr<ACOTReceiver<std::uint64_t>> OTProviderReceiver::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);
template std::unique_ptr<ACOTReceiver<__uint128_t>> OTProviderReceiver::RegisterACOT(
    std::size_t num_ots, std::size_t vector_size,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send);

std::unique_ptr<GOT128Receiver> OTProviderReceiver::RegisterGOT128(
    const std::size_t num_ots, const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_.load();
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<GOT128Receiver>(i, num_ots, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit receiver GOT128s",
                                    party_id_, num_ots, 128));
    }
  }
  return ot;
}

std::unique_ptr<GOTBitReceiver> OTProviderReceiver::RegisterGOTBit(
    const std::size_t num_ots, const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send) {
  const auto i = total_ots_count_.load();
  total_ots_count_ += num_ots;
  auto ot = std::make_unique<GOTBitReceiver>(i, num_ots, data_, Send);
  if constexpr (MOTION::MOTION_DEBUG) {
    if (logger_) {
      logger_->LogDebug(fmt::format("Party#{}: registered {} parallel {}-bit receiver GOTBits",
                                    party_id_, num_ots, 1));
    }
  }
  return ot;
}

void OTProviderReceiver::Clear() {
  // TODO: move this
  // data_storage_->GetBaseOTsData()->GetReceiverData().consumed_offset_ += total_ots_count_;
  //
  total_ots_count_ = 0;

  {
    std::scoped_lock lock(data_.setup_finished_cond_->GetMutex());
    data_.setup_finished_ = false;
  }

  {
    std::scoped_lock lock(data_.real_choices_mutex_);
    data_.set_real_choices_.clear();
  }

  {
    std::scoped_lock lock(data_.received_outputs_mutex_);
    data_.received_outputs_.clear();
  }
}
void OTProviderReceiver::Reset() { Clear(); }

class OTExtensionMessageHandler : public MOTION::Communication::MessageHandler {
 public:
  OTExtensionMessageHandler(MOTION::OTExtensionData &data) : data_(data) {}
  void received_message(std::size_t, std::vector<std::uint8_t> &&message) override;

 private:
  MOTION::OTExtensionData &data_;
};

void OTExtensionMessageHandler::received_message(std::size_t,
                                                 std::vector<std::uint8_t> &&raw_message) {
  assert(!raw_message.empty());
  auto message = MOTION::Communication::GetMessage(raw_message.data());
  auto message_type = message->message_type();
  auto index_i = MOTION::Communication::GetOTExtensionMessage(message->payload()->data())->i();
  auto ot_data =
      MOTION::Communication::GetOTExtensionMessage(message->payload()->data())->buffer()->data();
  auto ot_data_size =
      MOTION::Communication::GetOTExtensionMessage(message->payload()->data())->buffer()->size();
  switch (message_type) {
    case MOTION::Communication::MessageType::OTExtensionReceiverMasks: {
      data_.MessageReceived(ot_data, ot_data_size, MOTION::OTExtensionDataType::rcv_masks, index_i);
      break;
    }
    case MOTION::Communication::MessageType::OTExtensionReceiverCorrections: {
      data_.MessageReceived(ot_data, ot_data_size, MOTION::OTExtensionDataType::rcv_corrections, index_i);
      break;
    }
    case MOTION::Communication::MessageType::OTExtensionSender: {
      data_.MessageReceived(ot_data, ot_data_size, MOTION::OTExtensionDataType::snd_messages, index_i);
      break;
    }
    default: {
      assert(false);
      break;
    }
  }
}

OTProviderManager::OTProviderManager(MOTION::Communication::CommunicationLayer &communication_layer,
                                     const MOTION::BaseOTProvider &base_ot_provider,
                                     MOTION::Crypto::MotionBaseProvider &motion_base_provider,
                                     std::shared_ptr<MOTION::Logger> logger)
    : communication_layer_(communication_layer),
      base_ot_provider_(base_ot_provider),
      motion_base_provider_(motion_base_provider),
      logger_(logger),
      num_parties_(communication_layer_.get_num_parties()),
      providers_(num_parties_),
      data_(num_parties_) {
  auto my_id = communication_layer.get_my_id();
  for (std::size_t party_id = 0; party_id < num_parties_; ++party_id) {
    if (party_id == my_id) {
      continue;
    }
    auto send_func = [this, party_id](flatbuffers::FlatBufferBuilder &&message_builder) {
      communication_layer_.send_message(party_id, std::move(message_builder));
    };
    data_.at(party_id) = std::make_unique<MOTION::OTExtensionData>();
    providers_.at(party_id) = std::make_unique<OTProviderFromOTExtension>(
        send_func, *data_.at(party_id), base_ot_provider.get_base_ots_data(party_id),
        motion_base_provider, party_id, logger);
  }

  communication_layer_.register_message_handler(
      [this](std::size_t party_id) {
        return std::make_shared<OTExtensionMessageHandler>(*data_.at(party_id));
      },
      {MOTION::Communication::MessageType::OTExtensionReceiverMasks,
       MOTION::Communication::MessageType::OTExtensionReceiverCorrections,
       MOTION::Communication::MessageType::OTExtensionSender});
}

OTProviderManager::~OTProviderManager() {
  communication_layer_.deregister_message_handler(
      {MOTION::Communication::MessageType::OTExtensionReceiverMasks,
       MOTION::Communication::MessageType::OTExtensionReceiverCorrections,
       MOTION::Communication::MessageType::OTExtensionSender});
}

void OTProviderManager::run_setup() {
  motion_base_provider_.wait_setup();
  base_ot_provider_.wait_setup();

  if constexpr (MOTION::MOTION_DEBUG) {
    logger_->LogDebug("Start computing setup for OTExtensions");
  }

  // run_time_stats_.back().record_start<Statistics::RunTimeStats::StatID::ot_extension_setup>();

  std::vector<std::future<void>> task_futures;
  task_futures.reserve(2 * (communication_layer_.get_num_parties() - 1));

  for (auto party_i = 0ull; party_i < communication_layer_.get_num_parties(); ++party_i) {
    if (party_i == communication_layer_.get_my_id()) {
      continue;
    }
    task_futures.emplace_back(std::async(
        std::launch::async, [this, party_i] { providers_.at(party_i)->SendSetup(); }));
    task_futures.emplace_back(std::async(
        std::launch::async, [this, party_i] { providers_.at(party_i)->ReceiveSetup(); }));
  }

  std::for_each(task_futures.begin(), task_futures.end(), [](auto &f) { f.get(); });
  set_setup_ready();

  // run_time_stats_.back().record_end<Statistics::RunTimeStats::StatID::ot_extension_setup>();

  if constexpr (MOTION::MOTION_DEBUG) {
    logger_->LogDebug("Finished setup for OTExtensions");
  }
}

}  // namespace ENCRYPTO::ObliviousTransfer
