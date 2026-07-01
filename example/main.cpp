// Minimal demonstration: evaluate the PURPOSE.md example with both strategies.

#include <expressionist.hpp>

#include <iostream>

int main() {
  const nlohmann::json input = nlohmann::json::parse(R"({
    "a": 1,
    "b": 2,
    "c": "$a + b",
    "d": "This must remain a string",
    "e": "while strings beginning with $ must be evaluated",
    "f": "$pi/3",
    "g": "$sin(f ) * b",
    "h": true,
    "i": false,
    "j": "$h && i"
  })");

  try {
    Expressionist::Expressionist ex(input, Expressionist::EvalMethod::GRAPH);
    std::cout << "GRAPH:\n" << ex.produce().dump(2) << "\n\n";

    ex.setEvalMethod(Expressionist::EvalMethod::RECURSIVE);
    std::cout << "RECURSIVE:\n" << ex.produce().dump(2) << "\n";
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
