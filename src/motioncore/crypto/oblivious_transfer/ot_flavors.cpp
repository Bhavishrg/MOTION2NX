// MIT License
//
// Copyright (c) 2019 Lennart Braun
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

#include "communication/ot_extension_message.h"
#include "data_storage/ot_extension_data.h"
#include "ot_flavors.h"
#include "utility/fiber_condition.h"

namespace ENCRYPTO {
namespace ObliviousTransfer {

// ---------- BasicOTSender ----------

BasicOTSender::BasicOTSender(std::size_t ot_id, std::size_t num_ots, std::size_t bitlen,
                             OTProtocol p,
                             const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send,
                             MOTION::OTExtensionSenderData &data)
    : OTVector(ot_id, num_ots, bitlen, p, Send), data_(data) {
  data_.received_correction_offsets_cond_.emplace(
      ot_id_, std::make_unique<FiberCondition>([this]() {
        std::scoped_lock lock(data_.corrections_mutex_);
        return data_.received_correction_offsets_.find(ot_id_) !=
               data_.received_correction_offsets_.end();
      }));
  data_.y0_.resize(data_.y0_.size() + num_ots);
  data_.y1_.resize(data_.y1_.size() + num_ots);
  data_.bitlengths_.resize(data_.bitlengths_.size() + num_ots, bitlen);
  data_.corrections_.Resize(data_.corrections_.GetSize() + num_ots);
  data_.num_ots_in_batch_.emplace(ot_id, num_ots);
}

void BasicOTSender::WaitSetup() const { data_.setup_finished_cond_->Wait(); }

// ---------- BasicOTReceiver ----------

BasicOTReceiver::BasicOTReceiver(std::size_t ot_id, std::size_t num_ots, std::size_t bitlen,
                                 OTProtocol p,
                                 const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send,
                                 MOTION::OTExtensionReceiverData &data)
    : OTVector(ot_id, num_ots, bitlen, p, Send), data_(data) {
  data_.outputs_.resize(ot_id + num_ots);
  data_.bitlengths_.resize(ot_id + num_ots, bitlen);
  data_.num_ots_in_batch_.emplace(ot_id, num_ots);
}

void BasicOTReceiver::WaitSetup() const { data_.setup_finished_cond_->Wait(); }

void BasicOTReceiver::SendCorrections() {
  if (choices_.Empty()) {
    throw std::runtime_error("Choices in COT must be set before calling SendCorrections()");
  }
  auto corrections = choices_ ^ data_.random_choices_->Subset(ot_id_, ot_id_ + num_ots_);
  Send_(MOTION::Communication::BuildOTExtensionMessageReceiverCorrections(
      corrections.GetData().data(), corrections.GetData().size(), ot_id_));
  corrections_sent_ = true;
}

// ---------- FixedXCOT128Sender ----------

FixedXCOT128Sender::FixedXCOT128Sender(
    std::size_t ot_id, std::size_t num_ots, MOTION::OTExtensionSenderData &data,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : BasicOTSender(ot_id, num_ots, 128, FixedXCOT128, Send, data) {}

void FixedXCOT128Sender::ComputeOutputs() {
  if (outputs_computed_) {
    // the work was already done
    return;
  }

  // setup phase needs to be finished
  WaitSetup();

  // data storage for all the sender data
  const auto &ot_ext_snd = data_;

  // wait until the receiver has sent its correction bits
  ot_ext_snd.received_correction_offsets_cond_.at(ot_id_)->Wait();

  // make space for all the OTs
  outputs_.resize(num_ots_);

  // get the corrections bits
  std::unique_lock lock(ot_ext_snd.corrections_mutex_);
  const auto corrections = ot_ext_snd.corrections_.Subset(ot_id_, ot_id_ + num_ots_);
  lock.unlock();

  // take one of the precomputed outputs
  for (std::size_t i = 0; i < num_ots_; ++i) {
    if (corrections[i]) {
      // if the correction bit is 1, we need to swap
      outputs_[i].load_from_memory(ot_ext_snd.y1_.at(ot_id_ + i).GetData().data());
    } else {
      outputs_[i].load_from_memory(ot_ext_snd.y0_.at(ot_id_ + i).GetData().data());
    }
  }

  // remember that we have done this
  outputs_computed_ = true;
}

void FixedXCOT128Sender::SendMessages() const {
  block128_vector buffer(num_ots_, correlation_);
  const auto &ot_ext_snd = data_;
  for (std::size_t i = 0; i < num_ots_; ++i) {
    buffer[i] ^= ot_ext_snd.y0_.at(ot_id_ + i).GetData().data();
    buffer[i] ^= ot_ext_snd.y1_.at(ot_id_ + i).GetData().data();
  }
  Send_(MOTION::Communication::BuildOTExtensionMessageSender(buffer.data()->data(),
                                                             buffer.byte_size(), ot_id_));
}

// ---------- FixedXCOT128Receiver ----------

FixedXCOT128Receiver::FixedXCOT128Receiver(
    const std::size_t ot_id, const std::size_t num_ots, MOTION::OTExtensionReceiverData &data,
    const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : BasicOTReceiver(ot_id, num_ots, 128, FixedXCOT128, Send, data), outputs_(num_ots) {
  data_.msg_type_.emplace(ot_id, MOTION::OTMsgType::block128);
  sender_message_future_ = data_.RegisterForBlock128SenderMessage(ot_id, num_ots);
}

void FixedXCOT128Receiver::ComputeOutputs() {
  if (outputs_computed_) {
    // already done
    return;
  }

  if (!corrections_sent_) {
    throw std::runtime_error("Choices in COT must be se(n)t before calling ComputeOutputs()");
  }
  auto sender_message = sender_message_future_.get();

  for (std::size_t i = 0; i < num_ots_; ++i) {
    outputs_[i].load_from_memory(data_.outputs_.at(ot_id_ + i).GetData().data());
    if (choices_[i]) {
      outputs_[i] ^= sender_message[i];
    }
  }
  outputs_computed_ = true;
}

// ---------- XCOTBitSender ----------

XCOTBitSender::XCOTBitSender(const std::size_t ot_id, const std::size_t num_ots,
                             const std::size_t vector_size, MOTION::OTExtensionSenderData& data,
                             const std::function<void(flatbuffers::FlatBufferBuilder&&)>& Send)
    : BasicOTSender(ot_id, num_ots, vector_size, XCOTBit, Send, data), vector_size_(vector_size) {}

void XCOTBitSender::ComputeOutputs() {
  if (outputs_computed_) {
    // the work was already done
    return;
  }

  // setup phase needs to be finished
  WaitSetup();

  // wait until the receiver has sent its correction bits
  data_.received_correction_offsets_cond_.at(ot_id_)->Wait();

  // get the corrections bits
  std::unique_lock lock(data_.corrections_mutex_);
  const auto corrections = data_.corrections_.Subset(ot_id_, ot_id_ + num_ots_);
  lock.unlock();

  if (vector_size_ == 1) {
    // make space for all the OTs
    outputs_.Resize(num_ots_);

    // take one of the precomputed outputs
    for (std::size_t i = 0; i < num_ots_; ++i) {
      if (corrections[i]) {
        // if the correction bit is 1, we need to swap
        outputs_.Set(bool(data_.y1_.at(ot_id_ + i).GetData()[0] & SET_BIT_MASK[0]), i);
      } else {
        outputs_.Set(bool(data_.y0_.at(ot_id_ + i).GetData()[0] & SET_BIT_MASK[0]), i);
      }
    }
  } else {
    // make space for all the OTs
    outputs_.Reserve(MOTION::Helpers::Convert::BitsToBytes(num_ots_ * vector_size_));

    // take one of the precomputed outputs
    for (std::size_t i = 0; i < num_ots_; ++i) {
      if (corrections[i]) {
        // if the correction bit is 1, we need to swap
        outputs_.Append(data_.y1_.at(ot_id_ + i));
      } else {
        outputs_.Append(data_.y0_.at(ot_id_ + i));
      }
    }
  }

  // remember that we have done this
  outputs_computed_ = true;
}

void XCOTBitSender::SendMessages() const {
  ENCRYPTO::BitVector<> buffer = correlations_;
  if (vector_size_ == 1) {
    for (std::size_t i = 0; i < num_ots_; ++i) {
      auto tmp = buffer[i];
      tmp ^= bool(data_.y0_.at(ot_id_ + i).GetData()[0] & SET_BIT_MASK[0]);
      tmp ^= bool(data_.y1_.at(ot_id_ + i).GetData()[0] & SET_BIT_MASK[0]);
      buffer.Set(tmp, i);
    }
  } else {
    ENCRYPTO::BitVector<> tmp;
    tmp.Reserve(MOTION::Helpers::Convert::BitsToBytes(num_ots_ * vector_size_));
    for (std::size_t ot_i = 0; ot_i < num_ots_; ++ot_i) {
      const auto& y0_p = data_.y0_.at(ot_id_ + ot_i);
      const auto& y1_p = data_.y1_.at(ot_id_ + ot_i);
      tmp.Append(y0_p ^ y1_p);
    }
    buffer ^= tmp;
  }
  assert(buffer.GetSize() == num_ots_ * vector_size_);
  Send_(MOTION::Communication::BuildOTExtensionMessageSender(buffer.GetData().data(),
                                                             buffer.GetData().size(), ot_id_));
}

// ---------- XCOTBitReceiver ----------

XCOTBitReceiver::XCOTBitReceiver(const std::size_t ot_id, const std::size_t num_ots,
                                 const std::size_t vector_size,
                                 MOTION::OTExtensionReceiverData& data,
                                 const std::function<void(flatbuffers::FlatBufferBuilder&&)>& Send)
    : BasicOTReceiver(ot_id, num_ots, vector_size, XCOTBit, Send, data), vector_size_(vector_size) {
  data_.msg_type_.emplace(ot_id, MOTION::OTMsgType::bit);
  sender_message_future_ = data_.RegisterForBitSenderMessage(ot_id, num_ots * vector_size_);
}

void XCOTBitReceiver::ComputeOutputs() {
  if (outputs_computed_) {
    // already done
    return;
  }

  if (!corrections_sent_) {
    throw std::runtime_error("Choices in COT must be se(n)t before calling ComputeOutputs()");
  }

  if (vector_size_ == 1) {
    outputs_ = sender_message_future_.get();
    outputs_ &= choices_;

    for (std::size_t i = 0; i < num_ots_; ++i) {
      auto tmp = outputs_[i];
      outputs_.Set(tmp ^ bool(data_.outputs_.at(ot_id_ + i).GetData()[0] & SET_BIT_MASK[0]), i);
    }
  } else {
    outputs_.Reserve(MOTION::Helpers::Convert::BitsToBytes(num_ots_ * vector_size_));
    const auto sender_message = sender_message_future_.get();
    for (std::size_t ot_i = 0; ot_i < num_ots_; ++ot_i) {
      auto ot_data = std::move(data_.outputs_.at(ot_id_ + ot_i));
      if (choices_[ot_i]) {
        ot_data ^= sender_message.Subset(ot_i * vector_size_, (ot_i + 1) * vector_size_);
      }
      outputs_.Append(std::move(ot_data));
    }
  }

  outputs_computed_ = true;
}

// ---------- ACOTSender ----------

template <typename T>
ACOTSender<T>::ACOTSender(const std::size_t ot_id, const std::size_t num_ots,
                          const std::size_t vector_size, MOTION::OTExtensionSenderData &data,
                          const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : BasicOTSender(ot_id, num_ots, 8 * sizeof(T) * vector_size, ACOT, Send, data),
      vector_size_(vector_size) {}

template <typename T>
void ACOTSender<T>::ComputeOutputs() {
  if (outputs_computed_) {
    // the work was already done
    return;
  }

  // setup phase needs to be finished
  WaitSetup();

  // wait until the receiver has sent its correction bits
  data_.received_correction_offsets_cond_.at(ot_id_)->Wait();

  // make space for all the OTs
  outputs_.resize(num_ots_ * vector_size_);

  // get the corrections bits
  std::unique_lock lock(data_.corrections_mutex_);
  const auto corrections = data_.corrections_.Subset(ot_id_, ot_id_ + num_ots_);
  lock.unlock();

  // take one of the precomputed outputs
  if (vector_size_ == 1) {
    for (std::size_t ot_i = 0; ot_i < num_ots_; ++ot_i) {
      if (corrections[ot_i]) {
        // if the correction bit is 1, we need to swap
        outputs_[ot_i] = *reinterpret_cast<const T *>(data_.y1_.at(ot_id_ + ot_i).GetData().data());
      } else {
        outputs_[ot_i] = *reinterpret_cast<const T *>(data_.y0_.at(ot_id_ + ot_i).GetData().data());
      }
    }
  } else {
    for (std::size_t ot_i = 0; ot_i < num_ots_; ++ot_i) {
      if (corrections[ot_i]) {
        // if the correction bit is 1, we need to swap
        auto data_p = reinterpret_cast<const T *>(data_.y1_.at(ot_id_ + ot_i).GetData().data());
        std::copy(data_p, data_p + vector_size_, &outputs_[ot_i * vector_size_]);
      } else {
        auto data_p = reinterpret_cast<const T *>(data_.y0_.at(ot_id_ + ot_i).GetData().data());
        std::copy(data_p, data_p + vector_size_, &outputs_[ot_i * vector_size_]);
      }
    }
  }

  // remember that we have done this
  outputs_computed_ = true;
}

template <typename T>
void ACOTSender<T>::SendMessages() const {
  auto buffer = correlations_;
  if (vector_size_ == 1) {
    for (std::size_t ot_i = 0; ot_i < num_ots_; ++ot_i) {
      buffer[ot_i] += *reinterpret_cast<const T *>(data_.y0_.at(ot_id_ + ot_i).GetData().data());
      buffer[ot_i] += *reinterpret_cast<const T *>(data_.y1_.at(ot_id_ + ot_i).GetData().data());
    }
  } else {
    for (std::size_t ot_i = 0; ot_i < num_ots_; ++ot_i) {
      auto y0_p = reinterpret_cast<const T *>(data_.y0_.at(ot_id_ + ot_i).GetData().data());
      auto y1_p = reinterpret_cast<const T *>(data_.y1_.at(ot_id_ + ot_i).GetData().data());
      auto b_p = &buffer[ot_i * vector_size_];
      for (std::size_t j = 0; j < vector_size_; ++j) {
        b_p[j] += y0_p[j] + y1_p[j];
      }
    }
  }
  assert(buffer.size() == num_ots_ * vector_size_);
  Send_(MOTION::Communication::BuildOTExtensionMessageSender(
      reinterpret_cast<const std::byte *>(buffer.data()), sizeof(T) * buffer.size(), ot_id_));
}

// ---------- ACOTReceiver ----------

template <typename T>
ACOTReceiver<T>::ACOTReceiver(const std::size_t ot_id, const std::size_t num_ots,
                              const std::size_t vector_size, MOTION::OTExtensionReceiverData &data,
                              const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : BasicOTReceiver(ot_id, num_ots, 8 * sizeof(T) * vector_size, ACOT, Send, data),
      vector_size_(vector_size) {
  constexpr auto int_type_to_msg_type = boost::hana::make_map(
      boost::hana::make_pair(boost::hana::type_c<std::uint8_t>, MOTION::OTMsgType::uint8),
      boost::hana::make_pair(boost::hana::type_c<std::uint16_t>, MOTION::OTMsgType::uint16),
      boost::hana::make_pair(boost::hana::type_c<std::uint32_t>, MOTION::OTMsgType::uint32),
      boost::hana::make_pair(boost::hana::type_c<std::uint64_t>, MOTION::OTMsgType::uint64),
      boost::hana::make_pair(boost::hana::type_c<__uint128_t>, MOTION::OTMsgType::uint128));
  data_.msg_type_.emplace(ot_id, int_type_to_msg_type[boost::hana::type_c<T>]);
  sender_message_future_ = data_.RegisterForIntSenderMessage<T>(ot_id, num_ots * vector_size);
}

template <typename T>
void ACOTReceiver<T>::ComputeOutputs() {
  if (outputs_computed_) {
    // already done
    return;
  }

  if (!corrections_sent_) {
    throw std::runtime_error("Choices in COT must be se(n)t before calling ComputeOutputs()");
  }

  // make space for all the OTs
  outputs_.resize(num_ots_ * vector_size_);

  auto sender_message = sender_message_future_.get();
  assert(sender_message.size() == num_ots_ * vector_size_);

  if (vector_size_ == 1) {
    for (std::size_t ot_i = 0; ot_i < num_ots_; ++ot_i) {
      auto ot_data_p =
          reinterpret_cast<const T *>(data_.outputs_.at(ot_id_ + ot_i).GetData().data());
      if (choices_[ot_i]) {
        outputs_[ot_i] = sender_message[ot_i] - *ot_data_p;
      } else {
        outputs_[ot_i] = *ot_data_p;
      }
    }
  } else {
    for (std::size_t ot_i = 0; ot_i < num_ots_; ++ot_i) {
      auto ot_data_p =
          reinterpret_cast<const T *>(data_.outputs_.at(ot_id_ + ot_i).GetData().data());
      if (choices_[ot_i]) {
        std::transform(ot_data_p, ot_data_p + vector_size_, &sender_message[ot_i * vector_size_],
                       &outputs_[ot_i * vector_size_], [](auto d, auto m) { return m - d; });
      } else {
        std::copy(ot_data_p, ot_data_p + vector_size_, &outputs_[ot_i * vector_size_]);
      }
    }
  }
  outputs_computed_ = true;
}

// ---------- ACOT template instantiations ----------

template class ACOTSender<std::uint8_t>;
template class ACOTSender<std::uint16_t>;
template class ACOTSender<std::uint32_t>;
template class ACOTSender<std::uint64_t>;
template class ACOTSender<__uint128_t>;
template class ACOTReceiver<std::uint8_t>;
template class ACOTReceiver<std::uint16_t>;
template class ACOTReceiver<std::uint32_t>;
template class ACOTReceiver<std::uint64_t>;
template class ACOTReceiver<__uint128_t>;

// ---------- GOT128Sender ----------

GOT128Sender::GOT128Sender(std::size_t ot_id, std::size_t num_ots,
                           MOTION::OTExtensionSenderData &data,
                           const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : BasicOTSender(ot_id, num_ots, 128, GOT, Send, data) {}

void GOT128Sender::SendMessages() const {
  block128_vector buffer = std::move(inputs_);

  const auto &ot_ext_snd = data_;
  ot_ext_snd.received_correction_offsets_cond_.at(ot_id_)->Wait();
  std::unique_lock lock(ot_ext_snd.corrections_mutex_);
  const auto corrections = ot_ext_snd.corrections_.Subset(ot_id_, ot_id_ + num_ots_);
  lock.unlock();

  for (std::size_t i = 0; i < num_ots_; ++i) {
    if (corrections[i]) {
      block128_t diff = buffer[2 * i] ^ buffer[2 * i + 1];
      buffer[2 * i] ^= diff ^ ot_ext_snd.y0_.at(ot_id_ + i).GetData().data();
      buffer[2 * i + 1] ^= diff ^ ot_ext_snd.y1_.at(ot_id_ + i).GetData().data();
    } else {
      buffer[2 * i] ^= ot_ext_snd.y0_.at(ot_id_ + i).GetData().data();
      buffer[2 * i + 1] ^= ot_ext_snd.y1_.at(ot_id_ + i).GetData().data();
    }
  }
  Send_(MOTION::Communication::BuildOTExtensionMessageSender(buffer.data()->data(),
                                                             buffer.byte_size(), ot_id_));
}

// ---------- GOT128Receiver ----------

GOT128Receiver::GOT128Receiver(const std::size_t ot_id, const std::size_t num_ots,
                               MOTION::OTExtensionReceiverData &data,
                               const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : BasicOTReceiver(ot_id, num_ots, 128, FixedXCOT128, Send, data), outputs_(num_ots) {
  data_.msg_type_.emplace(ot_id, MOTION::OTMsgType::block128);
  sender_message_future_ = data_.RegisterForBlock128SenderMessage(ot_id, 2 * num_ots);
}

void GOT128Receiver::ComputeOutputs() {
  if (outputs_computed_) {
    // already done
    return;
  }

  if (!corrections_sent_) {
    throw std::runtime_error("Choices in OT must be se(n)t before calling ComputeOutputs()");
  }
  auto sender_message = sender_message_future_.get();
  const auto random_choices = data_.random_choices_->Subset(ot_id_, ot_id_ + num_ots_);

  for (std::size_t i = 0; i < num_ots_; ++i) {
    block128_t diff;
    if (random_choices[i]) {
      diff = sender_message[2 * i + 1];
    } else {
      diff = sender_message[2 * i];
    }
    outputs_[i] = diff ^ data_.outputs_.at(ot_id_ + i).GetData().data();
  }
  outputs_computed_ = true;
}

// ---------- GOTBitSender ----------

GOTBitSender::GOTBitSender(std::size_t ot_id, std::size_t num_ots,
                           MOTION::OTExtensionSenderData &data,
                           const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : BasicOTSender(ot_id, num_ots, 1, GOT, Send, data) {}

void GOTBitSender::SendMessages() const {
  auto buffer = std::move(inputs_);

  const auto &ot_ext_snd = data_;
  ot_ext_snd.received_correction_offsets_cond_.at(ot_id_)->Wait();
  std::unique_lock lock(ot_ext_snd.corrections_mutex_);
  const auto corrections = ot_ext_snd.corrections_.Subset(ot_id_, ot_id_ + num_ots_);
  lock.unlock();

  for (std::size_t i = 0; i < num_ots_; ++i) {
    bool b0 = buffer.Get(2 * i);
    bool b1 = buffer.Get(2 * i + 1);
    if (corrections[i]) {
      buffer.Set(b1 ^ ot_ext_snd.y0_.at(ot_id_ + i).Get(0), 2 * i);
      buffer.Set(b0 ^ ot_ext_snd.y1_.at(ot_id_ + i).Get(0), 2 * i + 1);
    } else {
      buffer.Set(b0 ^ ot_ext_snd.y0_.at(ot_id_ + i).Get(0), 2 * i);
      buffer.Set(b1 ^ ot_ext_snd.y1_.at(ot_id_ + i).Get(0), 2 * i + 1);
    }
  }
  Send_(MOTION::Communication::BuildOTExtensionMessageSender(buffer.GetData().data(),
                                                             buffer.GetData().size(), ot_id_));
}

// ---------- GOTBitReceiver ----------

GOTBitReceiver::GOTBitReceiver(const std::size_t ot_id, const std::size_t num_ots,
                               MOTION::OTExtensionReceiverData &data,
                               const std::function<void(flatbuffers::FlatBufferBuilder &&)> &Send)
    : BasicOTReceiver(ot_id, num_ots, 1, GOT, Send, data), outputs_(num_ots) {
  data_.msg_type_.emplace(ot_id, MOTION::OTMsgType::bit);
  sender_message_future_ = data_.RegisterForBitSenderMessage(ot_id, 2 * num_ots);
}

void GOTBitReceiver::ComputeOutputs() {
  if (outputs_computed_) {
    // already done
    return;
  }

  if (!corrections_sent_) {
    throw std::runtime_error("Choices in OT must be se(n)t before calling ComputeOutputs()");
  }
  auto sender_message = sender_message_future_.get();
  const auto random_choices = data_.random_choices_->Subset(ot_id_, ot_id_ + num_ots_);

  for (std::size_t i = 0; i < num_ots_; ++i) {
    bool diff;
    if (random_choices[i]) {
      diff = sender_message.Get(2 * i + 1);
    } else {
      diff = sender_message.Get(2 * i);
    }
    outputs_.Set(diff ^ data_.outputs_.at(ot_id_ + i).Get(0), i);
  }
  outputs_computed_ = true;
}

// ---------- ROTSender ----------

ROTSender::ROTSender(const std::size_t ot_id, const std::size_t num_ots, std::size_t vector_size,
                     bool random_choice, MOTION::OTExtensionSenderData& data,
                     const std::function<void(flatbuffers::FlatBufferBuilder&&)>& Send)
    : BasicOTSender(ot_id, num_ots, vector_size, ROT, Send, data),
      vector_size_(vector_size),
      random_choice_(random_choice) {
  if (random_choice == false) {
    throw std::logic_error("ROT with choice not yet implemented");
  }
}

void ROTSender::ComputeOutputs() {
  if (outputs_computed_) {
    // the work was already done
    return;
  }

  // setup phase needs to be finished
  WaitSetup();

  // make space for all the OTs
  outputs_.Reserve(MOTION::Helpers::Convert::BitsToBytes(2 * num_ots_ * vector_size_));

  // take one of the precomputed outputs
  for (std::size_t i = 0; i < num_ots_; ++i) {
    outputs_.Append(data_.y0_.at(ot_id_ + i));
    outputs_.Append(data_.y1_.at(ot_id_ + i));
  }

  // remember that we have done this
  outputs_computed_ = true;
}

// ---------- ROTReceiver ----------

ROTReceiver::ROTReceiver(const std::size_t ot_id, const std::size_t num_ots,
                         std::size_t vector_size, bool random_choice,
                         MOTION::OTExtensionReceiverData& data,
                         const std::function<void(flatbuffers::FlatBufferBuilder&&)>& Send)
    : BasicOTReceiver(ot_id, num_ots, vector_size, ROT, Send, data),
      vector_size_(vector_size),
      random_choice_(random_choice) {
  if (random_choice == false) {
    throw std::logic_error("ROT with choice not yet implemented");
  }
}

void ROTReceiver::ComputeOutputs() {
  if (outputs_computed_) {
    // already done
    return;
  }

  if (random_choice_) {
    choices_ = data_.random_choices_->Subset(ot_id_, ot_id_ + num_ots_);
  }

  outputs_.Reserve(MOTION::Helpers::Convert::BitsToBytes(num_ots_ * vector_size_));

  for (std::size_t i = 0; i < num_ots_; ++i) {
    assert(data_.outputs_.at(ot_id_ + i).GetSize() == vector_size_);
    outputs_.Append(data_.outputs_.at(ot_id_ + i));
  }
  outputs_computed_ = true;
}

}  // namespace ObliviousTransfer
}  // namespace ENCRYPTO
