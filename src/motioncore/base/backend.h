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

#pragma once

#include <memory>

#include <flatbuffers/flatbuffers.h>

#include "gate/arithmetic_gmw_gate.h"
#include "gate/constant_gate.h"

static_assert(FLATBUFFERS_LITTLEENDIAN);

namespace ENCRYPTO::ObliviousTransfer {
class OTProvider;
class OTProviderManager;
}  // namespace ENCRYPTO::ObliviousTransfer

namespace MOTION {
namespace Crypto {
class MotionBaseProvider;
class BMRProvider;
}
class MTProvider;
class SPProvider;
class SBProvider;

class BaseOTProvider;
struct SenderMsgs;
struct ReceiverMsgs;

class Logger;
using LoggerPtr = std::shared_ptr<Logger>;

class Configuration;
using ConfigurationPtr = std::shared_ptr<Configuration>;

class Register;
using RegisterPtr = std::shared_ptr<Register>;

class GateExecutor;

namespace Statistics {
struct RunTimeStats;
}

namespace Shares {
class GMWShare;
using GMWSharePtr = std::shared_ptr<GMWShare>;
}  // namespace Shares

namespace Gates::Interfaces {
class Gate;
using GatePtr = std::shared_ptr<Gate>;

class InputGate;
using InputGatePtr = std::shared_ptr<InputGate>;
}  // namespace Gates::Interfaces

namespace Communication {
class CommunicationLayer;
}  // namespace Communication

class Backend : public std::enable_shared_from_this<Backend> {
 public:
  Backend() = delete;

  Backend(Communication::CommunicationLayer &communication_layer, ConfigurationPtr &config, std::shared_ptr<Logger> logger);

  ~Backend();

  const ConfigurationPtr &GetConfig() const noexcept { return config_; }

  const LoggerPtr &GetLogger() const noexcept;

  const RegisterPtr &GetRegister() const noexcept { return register_; }

  std::size_t NextGateId() const;

  void Send(std::size_t party_id, flatbuffers::FlatBufferBuilder &&message);

  void RegisterInputGate(const Gates::Interfaces::InputGatePtr &input_gate);

  void RegisterGate(const Gates::Interfaces::GatePtr &gate);

  void RunPreprocessing();

  void EvaluateSequential();

  void EvaluateParallel();

  const Gates::Interfaces::GatePtr &GetGate(std::size_t gate_id) const;

  const std::vector<Gates::Interfaces::GatePtr> &GetInputGates() const;

  void Reset();

  void Clear();

  Shares::SharePtr BooleanGMWInput(std::size_t party_id, bool input = false);

  Shares::SharePtr BooleanGMWInput(std::size_t party_id, const ENCRYPTO::BitVector<> &input);

  Shares::SharePtr BooleanGMWInput(std::size_t party_id, ENCRYPTO::BitVector<> &&input);

  Shares::SharePtr BooleanGMWInput(std::size_t party_id,
                                   const std::vector<ENCRYPTO::BitVector<>> &input);

  Shares::SharePtr BooleanGMWInput(std::size_t party_id,
                                   std::vector<ENCRYPTO::BitVector<>> &&input);

  Shares::SharePtr BooleanGMWXOR(const Shares::GMWSharePtr &a, const Shares::GMWSharePtr &b);

  Shares::SharePtr BooleanGMWXOR(const Shares::SharePtr &a, const Shares::SharePtr &b);

  Shares::SharePtr BooleanGMWAND(const Shares::GMWSharePtr &a, const Shares::GMWSharePtr &b);

  Shares::SharePtr BooleanGMWAND(const Shares::SharePtr &a, const Shares::SharePtr &b);

  Shares::SharePtr BooleanGMWMUX(const Shares::GMWSharePtr &a, const Shares::GMWSharePtr &b,
                                 const Shares::GMWSharePtr &sel);

  Shares::SharePtr BooleanGMWMUX(const Shares::SharePtr &a, const Shares::SharePtr &b,
                                 const Shares::SharePtr &sel);

  Shares::SharePtr BooleanGMWOutput(const Shares::SharePtr &parent, std::size_t output_owner);

  Shares::SharePtr BMRInput(std::size_t party_id, bool input = false);

  Shares::SharePtr BMRInput(std::size_t party_id, const ENCRYPTO::BitVector<> &input);

  Shares::SharePtr BMRInput(std::size_t party_id, ENCRYPTO::BitVector<> &&input);

  Shares::SharePtr BMRInput(std::size_t party_id, const std::vector<ENCRYPTO::BitVector<>> &input);

  Shares::SharePtr BMRInput(std::size_t party_id, std::vector<ENCRYPTO::BitVector<>> &&input);

  Shares::SharePtr BMROutput(const Shares::SharePtr &parent, std::size_t output_owner);

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ConstantArithmeticGMWInput(T input = 0) {
    return ConstantArithmeticGMWInput({input});
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ConstantArithmeticGMWInput(const std::vector<T> &input_vector) {
    auto in_gate =
        std::make_shared<Gates::Arithmetic::ConstantArithmeticInputGate<T>>(input_vector, *this);
    RegisterGate(in_gate);
    return std::static_pointer_cast<Shares::Share>(in_gate->GetOutputAsShare());
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ConstantArithmeticGMWInput(std::vector<T> &&input_vector) {
    auto in_gate = std::make_shared<Gates::Arithmetic::ConstantArithmeticInputGate<T>>(
        std::move(input_vector), *this);
    RegisterGate(in_gate);
    return std::static_pointer_cast<Shares::Share>(in_gate->GetOutputAsShare());
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWInput(std::size_t party_id, T input = 0) {
    std::vector<T> input_vector{input};
    return ArithmeticGMWInput(party_id, std::move(input_vector));
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWInput(std::size_t party_id, const std::vector<T> &input_vector) {
    auto in_gate =
        std::make_shared<Gates::Arithmetic::ArithmeticInputGate<T>>(input_vector, party_id, *this);
    auto in_gate_cast = std::static_pointer_cast<Gates::Interfaces::InputGate>(in_gate);
    RegisterInputGate(in_gate_cast);
    return std::static_pointer_cast<Shares::Share>(in_gate->GetOutputAsArithmeticShare());
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWInput(std::size_t party_id, std::vector<T> &&input_vector) {
    auto in_gate = std::make_shared<Gates::Arithmetic::ArithmeticInputGate<T>>(
        std::move(input_vector), party_id, *this);
    auto in_gate_cast = std::static_pointer_cast<Gates::Interfaces::InputGate>(in_gate);
    RegisterInputGate(in_gate_cast);
    return std::static_pointer_cast<Shares::Share>(in_gate->GetOutputAsArithmeticShare());
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWOutput(const Shares::ArithmeticSharePtr<T> &parent,
                                       std::size_t output_owner) {
    assert(parent);
    auto out_gate =
        std::make_shared<Gates::Arithmetic::ArithmeticOutputGate<T>>(parent, output_owner);
    auto out_gate_cast = std::static_pointer_cast<Gates::Interfaces::Gate>(out_gate);
    RegisterGate(out_gate_cast);
    return std::static_pointer_cast<Shares::Share>(out_gate->GetOutputAsArithmeticShare());
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWOutput(const Shares::SharePtr &parent, std::size_t output_owner) {
    assert(parent);
    auto casted_parent_ptr = std::dynamic_pointer_cast<Shares::ArithmeticShare<T>>(parent);
    assert(casted_parent_ptr);
    return ArithmeticGMWOutput(casted_parent_ptr, output_owner);
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWAddition(const Shares::ArithmeticSharePtr<T> &a,
                                         const Shares::ArithmeticSharePtr<T> &b) {
    assert(a);
    assert(b);
    auto wire_a = a->GetArithmeticWire();
    auto wire_b = b->GetArithmeticWire();
    auto addition_gate =
        std::make_shared<Gates::Arithmetic::ArithmeticAdditionGate<T>>(wire_a, wire_b);
    auto addition_gate_cast = std::static_pointer_cast<Gates::Interfaces::Gate>(addition_gate);
    RegisterGate(addition_gate_cast);
    return std::static_pointer_cast<Shares::Share>(addition_gate->GetOutputAsArithmeticShare());
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWAddition(const Shares::SharePtr &a, const Shares::SharePtr &b) {
    assert(a);
    assert(b);
    auto casted_parent_a_ptr = std::dynamic_pointer_cast<Shares::ArithmeticShare<T>>(a);
    auto casted_parent_b_ptr = std::dynamic_pointer_cast<Shares::ArithmeticShare<T>>(b);
    assert(casted_parent_a_ptr);
    assert(casted_parent_b_ptr);
    return ArithmeticGMWAddition(casted_parent_a_ptr, casted_parent_b_ptr);
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWSubtraction(const Shares::ArithmeticSharePtr<T> &a,
                                            const Shares::ArithmeticSharePtr<T> &b) {
    assert(a);
    assert(b);
    auto wire_a = a->GetArithmeticWire();
    auto wire_b = b->GetArithmeticWire();
    auto sub_gate =
        std::make_shared<Gates::Arithmetic::ArithmeticSubtractionGate<T>>(wire_a, wire_b);
    auto sub_gate_cast = std::static_pointer_cast<Gates::Interfaces::Gate>(sub_gate);
    RegisterGate(sub_gate_cast);
    return std::static_pointer_cast<Shares::Share>(sub_gate->GetOutputAsArithmeticShare());
  }

  template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  Shares::SharePtr ArithmeticGMWSubtraction(const Shares::SharePtr &a, const Shares::SharePtr &b) {
    assert(a);
    assert(b);
    auto casted_parent_a_ptr = std::dynamic_pointer_cast<Shares::ArithmeticShare<T>>(a);
    auto casted_parent_b_ptr = std::dynamic_pointer_cast<Shares::ArithmeticShare<T>>(b);
    assert(casted_parent_a_ptr);
    assert(casted_parent_b_ptr);
    return ArithmeticGMWSubtraction(casted_parent_a_ptr, casted_parent_b_ptr);
  }

  /// \brief Blocking wait for synchronizing between parties. Called in Clear() and Reset()
  void Sync();

  void ComputeBaseOTs();

  void ImportBaseOTs(std::size_t i, const ReceiverMsgs &msgs);

  void ImportBaseOTs(std::size_t i, const SenderMsgs &msgs);

  std::pair<ReceiverMsgs, SenderMsgs> ExportBaseOTs(std::size_t i);

  void OTExtensionSetup();

  Communication::CommunicationLayer &get_communication_layer() { return communication_layer_; };

  Crypto::MotionBaseProvider &get_motion_base_provider() { return *motion_base_provider_; };

  Crypto::BMRProvider &get_bmr_provider() { return *bmr_provider_; };

  auto &GetBaseOTProvider() { return base_ot_provider_; };

  ENCRYPTO::ObliviousTransfer::OTProvider &GetOTProvider(std::size_t party_id);

  auto &GetMTProvider() { return mt_provider_; };

  auto &GetSPProvider() { return sp_provider_; };

  auto &GetSBProvider() { return sb_provider_; };

  const auto &GetRunTimeStats() const { return run_time_stats_; }

  auto &GetMutableRunTimeStats() { return run_time_stats_; }

 private:
  std::list<Statistics::RunTimeStats> run_time_stats_;

  Communication::CommunicationLayer &communication_layer_;
  std::shared_ptr<Logger> logger_;
  ConfigurationPtr config_;
  RegisterPtr register_;
  std::unique_ptr<GateExecutor> gate_executor_;

  std::unique_ptr<Crypto::MotionBaseProvider> motion_base_provider_;
  std::unique_ptr<BaseOTProvider> base_ot_provider_;
  std::unique_ptr<ENCRYPTO::ObliviousTransfer::OTProviderManager> ot_provider_manager_;
  std::unique_ptr<ArithmeticProviderManager> arithmetic_provider_manager_;
  std::shared_ptr<MTProvider> mt_provider_;
  std::shared_ptr<SPProvider> sp_provider_;
  std::shared_ptr<SBProvider> sb_provider_;
  std::unique_ptr<Crypto::BMRProvider> bmr_provider_;

  bool share_inputs_{true};
  bool require_base_ots_{false};
  bool base_ots_finished_{false};
  bool ot_extension_finished_{false};

  bool NeedOTs();
};

using BackendPtr = std::shared_ptr<Backend>;
}  // namespace MOTION
