#ifndef WIRE_H
#define WIRE_H

#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

#include "utility/typedefs.h"
#include "abynparty/abyncore.h"


namespace ABYN::Gates::Interfaces {
  class Gate;

  using GatePtr = std::shared_ptr<Gate>;
}

namespace ABYN::Wires {
  class Wire {
  public:
    size_t GetNumOfParallelValues() { return num_of_parallel_values_; }

    virtual enum CircuitType GetCircuitType() = 0;

    virtual enum Protocol GetProtocol() = 0;

    virtual ~Wire() {}

    void RegisterWaitingGate(size_t gate_id) {
      std::scoped_lock lock(mutex_);
      waiting_gate_ids_.insert(gate_id);
    }

    void UnregisterWaitingGate(size_t gate_id) {
      std::scoped_lock lock(mutex_);
      waiting_gate_ids_.erase(gate_id);
    }

    void SetOnlineFinished() {
      if (is_done_) {
        throw (std::runtime_error(fmt::format("Marking wire #{} as \"online phase ready\" twice", wire_id_)));
      }
      is_done_ = true;
      for (auto gate_id: waiting_gate_ids_) {
        Wire::UnregisterWireIdFromGate(gate_id, wire_id_, core_);
      }
    }

    // let the Gate class handle this to prevent cross-referencing
    const auto &GetWaitingGatesIds() const { return waiting_gate_ids_; }

    const bool &IsReady() {
      if (is_constant_) { return is_constant_; } else { return is_done_; }
    };

    bool IsConstant() { return is_constant_; }

    size_t GetWireId() {
      assert(wire_id_ >= 0);
      return static_cast<size_t>(wire_id_);
    }

    const ABYNCorePtr &GetCore() { return core_; }

  protected:
    // number of values that are _logically_ processed in parallel
    size_t num_of_parallel_values_ = 0;

    // flagging variables as constants is useful, since this allows for tricks, such as non-interactive
    // multiplication by a constant in (arithmetic) GMW
    bool is_constant_ = false;

    // is ready flag is needed for callbacks, i.e.,
    // gates will wait for wires to be evaluated to proceed with their evaluation
    bool is_done_ = false;

    ssize_t wire_id_ = -1;

    ABYNCorePtr core_;

    std::unordered_set<size_t> waiting_gate_ids_;

    Wire() {};

    static void UnregisterWireIdFromGate(size_t gate_id, size_t wire_id, ABYNCorePtr &core);

  private:

    Wire(const Wire &) = delete;

    Wire(Wire &) = delete;

    std::mutex mutex_;
  };

  using WirePtr = std::shared_ptr<Wire>;


// Allow only unsigned integers for Arithmetic wires.
  template<typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  class ArithmeticWire : public Wire {
  private:
    std::vector<T> values_;
  public:

    ArithmeticWire(std::vector<T> &&values, const ABYNCorePtr &core, bool is_constant = false) {
      is_constant_ = is_constant;
      core_ = core;
      values_ = std::move(values);
      num_of_parallel_values_ = values_.size();
      wire_id_ = core_->NextWireId();
      core_->RegisterNextWire(this);
    }

    ArithmeticWire(std::vector<T> &values, const ABYNCorePtr &core, bool is_constant = false) {
      is_constant_ = is_constant;
      core_ = core;
      values_ = values;
      num_of_parallel_values_ = values_.size();
      wire_id_ = core_->NextWireId();
      core_->RegisterNextWire(this);
    }

    ArithmeticWire(T t, const ABYNCorePtr &core, bool is_constant = false) {
      is_constant_ = is_constant;
      core_ = core;
      values_.push_back(t);
      num_of_parallel_values_ = 1;
      wire_id_ = core_->NextWireId();
      core_->RegisterNextWire(this);
    }

    virtual ~ArithmeticWire() {};

    virtual Protocol GetProtocol() final { return Protocol::ArithmeticGMW; };

    virtual CircuitType GetCircuitType() final { return CircuitType::ArithmeticType; };

    const std::vector<T> &GetValuesOnWire() { return values_; };

    std::vector<T> &GetMutableValuesOnWire() { return values_; };
  };

  template<typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
  using ArithmeticWirePtr = std::shared_ptr<ArithmeticWire<T>>;

  //TODO: implement boolean wires
  class BooleanWire : public Wire {
  public:
    virtual CircuitType GetCircuitType() final { return CircuitType::BooleanType; }

    virtual ~BooleanWire() {}

  private:
    BooleanWire() = delete;

    BooleanWire(BooleanWire &) = delete;
  };

  class GMWWire : public BooleanWire {
  public:
    virtual ~GMWWire() {}

  private:
    GMWWire() = delete;

    GMWWire(GMWWire &) = delete;
  };

  class BMRWire : BooleanWire {
  public:
    virtual ~BMRWire() {}

  private:
    BMRWire() = delete;

    BMRWire(BMRWire &) = delete;
  };

}

#endif //WIRE_H