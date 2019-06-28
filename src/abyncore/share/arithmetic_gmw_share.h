#pragma once

#include "share.h"

#include "wire/arithmetic_gmw_wire.h"

namespace ABYN::Shares {
/*
 * Allow only unsigned integers for Arithmetic shares.
 */
template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
class ArithmeticShare : public Share {
 public:
  ArithmeticShare(const Wires::WirePtr &wire) {
    wires_ = {std::dynamic_pointer_cast<ArithmeticShare<T>>(wire)};
    if (!wires_.at(0)) {
      throw(std::runtime_error("Something went wrong with creating an arithmetic share"));
    }
    register_ = wires_.at(0)->GetRegister();
  }

  ArithmeticShare(Wires::ArithmeticWirePtr<T> &wire) : wires_({wire}) {
    wires_ = {wire};
    assert(wire);
    register_ = wires_.at(0)->GetRegister();
  }

  ArithmeticShare(std::vector<Wires::ArithmeticWirePtr<T>> &wires) : wires_(wires) {
    if (wires.size() == 0) {
      throw(std::runtime_error("Trying to create an arithmetic share without wires"));
    }
    if (wires.size() > 1) {
      throw(
          std::runtime_error(fmt::format("Cannot create an arithmetic share "
                                         "from more than 1 wire; got {} wires",
                                         wires.size())));
    }
    register_ = wires_.at(0)->GetRegister();
  }

  ArithmeticShare(std::vector<Wires::WirePtr> &wires) {
    if (wires.size() == 0) {
      throw(std::runtime_error("Trying to create an arithmetic share without wires"));
    }
    if (wires.size() > 1) {
      throw(
          std::runtime_error(fmt::format("Cannot create an arithmetic share "
                                         "from more than 1 wire; got {} wires",
                                         wires.size())));
    }
    wires_ = {std::dynamic_pointer_cast<ArithmeticShare<T>>(wires.at(0))};
    if (!wires_.at(0)) {
      throw(std::runtime_error("Something went wrong with creating an arithmetic share"));
    }
    register_ = wires_.at(0)->GetRegister();
  }

  ArithmeticShare(std::vector<T> &input, const RegisterPtr &reg) {
    register_ = reg;
    wires_ = {std::make_shared<Wires::ArithmeticWire<T>>(input, reg)};
  }

  ArithmeticShare(T input, const RegisterPtr &reg) {
    register_ = reg;
    wires_ = {std::make_shared<Wires::ArithmeticWire<T>>(input, reg)};
  }

  ~ArithmeticShare() override = default;

  std::size_t GetNumOfParallelValues() final { return wires_.at(0)->GetNumOfParallelValues(); };

  Protocol GetSharingType() final { return wires_.at(0)->GetProtocol(); }

  const Wires::ArithmeticWirePtr<T> &GetArithmeticWire() { return wires_.at(0); }

  const std::vector<Wires::WirePtr> GetWires() const final {
    std::vector<Wires::WirePtr> result{std::static_pointer_cast<Wires::Wire>(wires_.at(0))};
    return std::move(result);
  }

  const bool &Finished() { return wires_.at(0)->IsReady(); }

  const std::vector<T> &GetValue() const { return wires_->GetRawSharedValues(); }

  std::size_t GetBitLength() final { return sizeof(T) * 8; }

  std::shared_ptr<Share> Clone() final {
    // TODO
    return std::static_pointer_cast<Share>(std::make_shared<ArithmeticShare<T>>(wires_));
  }

  std::shared_ptr<ArithmeticShare> NonVirtualClone() {
    return std::make_shared<ArithmeticShare>(wires_);
  }

  ArithmeticShare(ArithmeticShare &) = delete;

 protected:
  std::vector<Wires::ArithmeticWirePtr<T>> wires_;

 private:
  ArithmeticShare() = default;
};

template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
using ArithmeticSharePtr = std::shared_ptr<ArithmeticShare<T>>;

/*
 * Allow only unsigned integers for Arithmetic shares.
 */
template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
class ArithmeticConstantShare : public Share {
 public:
  ArithmeticConstantShare(T input, const RegisterPtr &reg) : values_({input}) { register_ = reg; }

  ArithmeticConstantShare(std::vector<T> &input, const RegisterPtr &reg) : values_(input) {
    register_ = reg;
  }

  ArithmeticConstantShare(std::vector<T> &&input, const RegisterPtr &reg)
      : values_(std::move(input)) {
    register_ = reg;
  }

  ~ArithmeticConstantShare() override = default;

  std::size_t GetNumOfParallelValues() final { return values_.size(); };

  Protocol GetSharingType() final { return ArithmeticGMW; }

  const std::vector<T> &GetValue() const { return values_; }

  std::shared_ptr<Share> Clone() final {
    // TODO
    return std::static_pointer_cast<Share>(
        std::make_shared<ArithmeticConstantShare>(values_, register_));
  };

  ArithmeticConstantShare() = delete;

  ArithmeticConstantShare(ArithmeticConstantShare &) = delete;

  std::size_t GetBitLength() final { return sizeof(T) * 8; }

 protected:
  std::vector<T> values_;

 private:
};

template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
using ArithmeticConstantSharePtr = std::shared_ptr<ArithmeticConstantShare<T>>;
}