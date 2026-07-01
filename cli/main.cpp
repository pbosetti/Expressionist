// expressionist -- command-line front-end for the Expressionist library.
//
// Reads an input JSON (from a positional argument or from stdin), evaluates the
// expression fields and writes the resulting JSON to stdout.
//
// Copyright 2026 Paolo Bosetti
// SPDX-License-Identifier: Apache-2.0

#include <expressionist.hpp>

#include <cxxopts.hpp>

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

#ifndef EXPRESSIONIST_VERSION
#define EXPRESSIONIST_VERSION "unknown"
#endif

namespace {

std::string to_lower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string read_stream(std::istream &in) {
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

} // namespace

int main(int argc, char **argv) {
  cxxopts::Options options(
      "expressionist",
      "Evaluate algebraic expressions embedded in JSON fields.\n"
      "Input JSON is read from the positional argument, or from stdin if none "
      "is given.");

  // clang-format off
  options.add_options()
      ("m,method", "Evaluation strategy: graph or recursive",
          cxxopts::value<std::string>()->default_value("graph"))
      ("t,tag", "Prefix that marks a string as an expression",
          cxxopts::value<std::string>()->default_value("$"))
      ("indent", "Number of spaces for pretty-printing the output",
          cxxopts::value<int>()->default_value("2"))
      ("c,compact", "Emit compact, single-line JSON (overrides --indent)",
          cxxopts::value<bool>()->default_value("false"))
      ("input", "Input JSON (positional; read from stdin if omitted)",
          cxxopts::value<std::string>())
      ("h,help", "Print usage and exit")
      ("V,version", "Print version and exit");
  // clang-format on
  options.parse_positional({"input"});
  options.positional_help("[JSON]");

  cxxopts::ParseResult args;
  try {
    args = options.parse(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "expressionist: " << e.what() << "\n";
    return 2;
  }

  if (args.count("help")) {
    std::cout << options.help() << "\n";
    return 0;
  }
  if (args.count("version")) {
    std::cout << "expressionist " << EXPRESSIONIST_VERSION << "\n";
    return 0;
  }

  // --- Evaluation method ---------------------------------------------------
  Expressionist::EvalMethod method = Expressionist::EvalMethod::GRAPH;
  {
    const std::string requested = args["method"].as<std::string>();
    const std::string m = to_lower(requested);
    if (m == "graph")
      method = Expressionist::EvalMethod::GRAPH;
    else if (m == "recursive")
      method = Expressionist::EvalMethod::RECURSIVE;
    else {
      std::cerr << "expressionist: unknown method '" << requested
                << "' (use 'graph' or 'recursive')\n";
      return 2;
    }
  }

  // --- Read input ----------------------------------------------------------
  std::string input_text;
  if (args.count("input"))
    input_text = args["input"].as<std::string>();
  else
    input_text = read_stream(std::cin);

  if (input_text.find_first_not_of(" \t\r\n") == std::string::npos) {
    std::cerr << "expressionist: no input provided (pass JSON as an argument "
                 "or via stdin)\n";
    return 2;
  }

  // --- Parse JSON ----------------------------------------------------------
  nlohmann::json input;
  try {
    input = nlohmann::json::parse(input_text);
  } catch (const std::exception &e) {
    std::cerr << "expressionist: invalid JSON: " << e.what() << "\n";
    return 1;
  }

  // --- Evaluate ------------------------------------------------------------
  nlohmann::json output;
  try {
    Expressionist::Expressionist ex(std::move(input), method);
    ex.setTag(args["tag"].as<std::string>());
    output = ex.produce();
  } catch (const Expressionist::ExpressionistException &e) {
    std::cerr << "expressionist: " << e.what() << "\n";
    return 1;
  }

  const int indent = args["compact"].as<bool>() ? -1 : args["indent"].as<int>();
  std::cout << output.dump(indent) << "\n";
  return 0;
}
