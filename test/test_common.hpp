#pragma once

#include <expressionist.hpp>

#include <string>
#include <utility>

using json = nlohmann::json;
using EvalMethod = Expressionist::EvalMethod;
// The class shares its name with its namespace; alias it to avoid awkward
// qualification in the tests.
using Engine = Expressionist::Expressionist;

// The example object from PURPOSE.md, with the corrected `j` field.
inline json purpose_example() {
  return json{
      {"a", 1},
      {"b", 2},
      {"c", "$a + b"},
      {"d", "This must remain a string"},
      {"e", "while strings beginning with $ must be evaluated"},
      {"f", "$pi/3"},
      {"g", "$sin(f ) * b"},
      {"h", true},
      {"i", false},
      {"j", "$h && i"},
  };
}

// Evaluate a whole object with the given method and return the result.
inline json eval_with(json input, EvalMethod m) {
  Engine ex(std::move(input), m);
  ex.evaluate();
  return ex.object();
}

// Evaluate a single expression (tag included) by wrapping it in an object.
inline json eval_expr(const std::string &expr,
                      EvalMethod m = EvalMethod::GRAPH) {
  json o = json::object();
  o["x"] = expr;
  Engine ex(std::move(o), m);
  ex.evaluate();
  return ex.object()["x"];
}
