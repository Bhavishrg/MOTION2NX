// MIT License
//
// Copyright (c) 2021 Lennart Braun
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

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/json/serialize.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include "algorithm/circuit_loader.h"
#include "base/gate_factory.h"
#include "base/two_party_backend.h"
#include "communication/communication_layer.h"
#include "communication/tcp_transport.h"
#include "statistics/analysis.h"
#include "utility/logger.h"
#include "utility/bit_vector.h"
#include "protocols/beavy/beavy_provider.cpp"
#include "wire/new_wire.h"
#include "utility/helpers.h"

#include "protocols/gmw/wire.h"
#include "protocols/gmw/gate.h"
#include "protocols/gmw/gmw_provider.h"
#include "utility/bit_vector.h"
#include "protocols/gmw/gmw_provider.cpp"

namespace po = boost::program_options;
using namespace MOTION::proto::gmw;
using NewWire = MOTION::NewWire;
using WireVector = std::vector<std::shared_ptr<NewWire>>;

struct Options {
  std::size_t threads;
  bool json;
  std::size_t num_repetitions;
  std::size_t num_simd;
  bool sync_between_setup_and_online;
  MOTION::MPCProtocol arithmetic_protocol;
  MOTION::MPCProtocol boolean_protocol;
  std::uint64_t pattern_size;
  std::uint64_t text_size;
  // std::uint64_t ring_size;
  std::size_t my_id;
  MOTION::Communication::tcp_parties_config tcp_config;
  bool no_run = false;
};

std::optional<Options> parse_program_options(int argc, char* argv[]) {
  Options options;
  boost::program_options::options_description desc("Allowed options");
  // clang-format off
  desc.add_options()
    ("help,h", po::bool_switch()->default_value(false),"produce help message")
    ("config-file", po::value<std::string>(), "config file containing options")
    ("my-id", po::value<std::size_t>()->required(), "my party id")
    ("party", po::value<std::vector<std::string>>()->multitoken(),
     "(party id, IP, port), e.g., --party 1,127.0.0.1,7777")
    ("threads", po::value<std::size_t>()->default_value(0), "number of threads to use for gate evaluation")
    ("json", po::bool_switch()->default_value(false), "output data in JSON format")
    ("pattern-size", po::value<std::uint64_t>()->required(), "size of pattern")
    ("text-size", po::value<std::uint64_t>()->required(), "size of text")
    // ("ring-size", po::value<std::size_t>()->default_value(16), "size of the ring")
    ("repetitions", po::value<std::size_t>()->default_value(1), "number of repetitions")
    ("num-simd", po::value<std::size_t>()->default_value(1), "number of SIMD values")
    ("sync-between-setup-and-online", po::bool_switch()->default_value(false),
     "run a synchronization protocol before the online phase starts")
    ("no-run", po::bool_switch()->default_value(false), "just build the circuit, but not execute it")
    ;
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  bool help = vm["help"].as<bool>();
  if (help) {
    std::cerr << desc << "\n";
    return std::nullopt;
  }
  if (vm.count("config-file")) {
    std::ifstream ifs(vm["config-file"].as<std::string>().c_str());
    po::store(po::parse_config_file(ifs, desc), vm);
  }
  try {
    po::notify(vm);
  } catch (std::exception& e) {
    std::cerr << "error:" << e.what() << "\n\n";
    std::cerr << desc << "\n";
    return std::nullopt;
  }

  options.my_id = vm["my-id"].as<std::size_t>();
  options.threads = vm["threads"].as<std::size_t>();
  options.json = vm["json"].as<bool>();
  options.num_repetitions = vm["repetitions"].as<std::size_t>();
  options.num_simd = vm["num-simd"].as<std::size_t>();
  options.sync_between_setup_and_online = vm["sync-between-setup-and-online"].as<bool>();
  options.no_run = vm["no-run"].as<bool>();

  options.arithmetic_protocol = MOTION::MPCProtocol::ArithmeticGMW;
  options.boolean_protocol = MOTION::MPCProtocol::BooleanGMW;
  
  
  auto pattern_size = vm["pattern-size"].as<std::uint64_t>();
  options.pattern_size = vm["pattern-size"].as<std::uint64_t>();
  // options.ring_size = vm["ring-size"].as<std::uint64_t>();

  auto text_size = vm["text-size"].as<std::uint64_t>();
  options.text_size = text_size;
  assert(pattern_size < text_size);

  const auto parse_party_argument =
      [](const auto& s) -> std::pair<std::size_t, MOTION::Communication::tcp_connection_config> {
    const static std::regex party_argument_re("([012]),([^,]+),(\\d{1,5})");
    std::smatch match;
    if (!std::regex_match(s, match, party_argument_re)) {
      throw std::invalid_argument("invalid party argument");
    }
    auto id = boost::lexical_cast<std::size_t>(match[1]);
    auto host = match[2];
    auto port = boost::lexical_cast<std::uint16_t>(match[3]);
    return {id, {host, port}};
  };

  const std::vector<std::string> party_infos = vm["party"].as<std::vector<std::string>>();
  if (party_infos.size() != 2) {
    std::cerr << "expecting two --party options\n";
    return std::nullopt;
  }

  options.tcp_config.resize(2);
  std::size_t other_id = 2;

  const auto [id0, conn_info0] = parse_party_argument(party_infos[0]);
  const auto [id1, conn_info1] = parse_party_argument(party_infos[1]);
  if (id0 == id1) {
    std::cerr << "need party arguments for party 0 and 1\n";
    return std::nullopt;
  }
  options.tcp_config[id0] = conn_info0;
  options.tcp_config[id1] = conn_info1;

  return options;
}




std::unique_ptr<MOTION::Communication::CommunicationLayer> setup_communication(
    const Options& options) {
  MOTION::Communication::TCPSetupHelper helper(options.my_id, options.tcp_config);
  return std::make_unique<MOTION::Communication::CommunicationLayer>(options.my_id,
                                                                     helper.setup_connections());
}

std::vector<uint64_t> convert_to_binary(uint64_t x) {
    std::vector<uint64_t> res;
    for (uint64_t i = 0; i < 64; ++i) {
        if (x%2 == 1) res.push_back(1);
        else res.push_back(0);
        x /= 2;
    }
    return res;
}


auto make_boolean_wires(const Options& options){
  auto num_simd = (options.text_size - options.pattern_size + 1);
  int num_wires = options.pattern_size;
  ENCRYPTO::BitVector<> dx = ENCRYPTO::BitVector<>::Random(num_simd);

  auto x = std::make_shared<MOTION::proto::gmw::BooleanGMWWire>(num_simd);
  auto& x_sec = x->get_share();
  x_sec = dx;
  x->set_online_ready();

  auto xx = std::dynamic_pointer_cast<MOTION::NewWire>(x);
  // Cast to wire vectors.
  MOTION::WireVector X;
  
  for (uint64_t j = 0; j < num_wires; ++j) {
      X.push_back(xx);
}
  return X;
}

auto make_dpf_in_wire(const Options& options) {
  
  auto num_simd = (options.text_size - options.pattern_size + 1)*options.pattern_size;
  auto num_wires = options.pattern_size;

  auto wire = std::make_shared<ArithmeticGMWWire<uint8_t>>(num_simd);
  std::vector<MOTION::NewWireP> in;
  std::vector<uint8_t> x(num_simd, 2*num_wires);

  wire->get_share() = x;
  wire->set_online_ready();

  in.push_back(wire);
  return in;
}

auto make_ham_in_wire(const Options& options) {
  
  auto num_simd = (options.text_size - options.pattern_size + 1)*options.pattern_size;

  auto wire = std::make_shared<ArithmeticGMWWire<uint8_t>>(num_simd);
  std::vector<MOTION::NewWireP> in;
  std::vector<uint8_t> x(num_simd, 1);

  wire->get_share() = x;
  wire->set_online_ready();

  in.push_back(wire);
  return in;
}


void run_circuit(const Options& options, MOTION::TwoPartyBackend& backend, WireVector in1, WireVector in2, WireVector in3) {

  if (options.no_run) {
    return;
  }

  MOTION::MPCProtocol arithmetic_protocol = options.arithmetic_protocol;
  MOTION::MPCProtocol boolean_protocol = options.boolean_protocol;
  auto& gate_factory_arith = backend.get_gate_factory(arithmetic_protocol);
  auto& gate_factory_bool = backend.get_gate_factory(boolean_protocol);

  auto output = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::MUL, in3, in3);
  auto output0 = gate_factory_bool.make_unary_gate(ENCRYPTO::PrimitiveOperationType::HAM, in3);
  auto output = gate_factory_arith.make_unary_gate(
    ENCRYPTO::PrimitiveOperationType::DPF, in2);

  auto output1 = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::AND, in1, in1);
  auto output2 = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::AND, output1, output1);
  auto output3 = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::AND, output2, output2);
  auto output4 = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::AND, output3, output3);
  auto output5 = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::AND, output4, output4);
  auto output6 = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::AND, output5, output5);
  auto output7 = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::AND, output6, output5);
  auto output8 = gate_factory_bool.make_binary_gate(ENCRYPTO::PrimitiveOperationType::AND, output7, output7);

  
  
  backend.run();

}

void print_stats(const Options& options,
                 const MOTION::Statistics::AccumulatedRunTimeStats& run_time_stats,
                 const MOTION::Statistics::AccumulatedCommunicationStats& comm_stats) {
  if (options.json) {
    auto obj = MOTION::Statistics::to_json("exact_pm", run_time_stats, comm_stats);
    obj.emplace("party_id", options.my_id);
    obj.emplace("threads", options.threads);
    obj.emplace("sync_between_setup_and_online", options.sync_between_setup_and_online);
    std::cout << obj << "\n";
  } else {
    std::cout << MOTION::Statistics::print_stats("Exact Pattern Matching", run_time_stats,
                                                 comm_stats);
  }
}

int main(int argc, char* argv[]) {
  auto options = parse_program_options(argc, argv);
  
  if (!options.has_value()) {
    return EXIT_FAILURE;
  }
  
  try {

    auto in2 = make_dpf_in_wire(*options);
    auto in3 = make_ham_in_wire(*options);
    auto in1 = make_boolean_wires(*options);

    auto comm_layer = setup_communication(*options);
    auto logger = std::make_shared<MOTION::Logger>(options->my_id,
                                                   boost::log::trivial::severity_level::trace);
    comm_layer->set_logger(logger);
    MOTION::Statistics::AccumulatedRunTimeStats run_time_stats;
    MOTION::Statistics::AccumulatedCommunicationStats comm_stats;
    for (std::size_t i = 0; i < options->num_repetitions; ++i) {
      MOTION::TwoPartyBackend backend(*comm_layer, options->threads,
                                      options->sync_between_setup_and_online, logger);
      run_circuit(*options, backend, in1, in2, in3);
      comm_layer->sync();
      comm_stats.add(comm_layer->get_transport_statistics());
      comm_layer->reset_transport_statistics();
      run_time_stats.add(backend.get_run_time_stats());
    }
    comm_layer->shutdown();
    print_stats(*options, run_time_stats, comm_stats);
  } catch (std::runtime_error& e) {
    std::cerr << "ERROR OCCURRED: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}