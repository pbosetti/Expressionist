#include <doctest/doctest.h>

#include "test_common.hpp"

TEST_CASE("expressionist: false disables evaluation within that object") {
  json o = json{{"expressionist", false},
                {"a", 1},
                {"b", "$a + 1"},
                {"c", "plain string"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["b"] == "$a + 1"); // left untouched, not evaluated
    CHECK(out["c"] == "plain string");
    CHECK(out["expressionist"] == false); // the flag itself is kept as-is
  }
}

TEST_CASE("the disable flag applies to the whole nested subtree") {
  json o = json{
      {"expressionist", false},
      {"a", 1},
      {"inner", json{{"b", "$a + 1"},
                    {"deeper", json{{"c", "$a + 2"}}}}},
      {"list", json::array({"$a + 3", "literal"})}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["inner"]["b"] == "$a + 1");
    CHECK(out["inner"]["deeper"]["c"] == "$a + 2");
    CHECK(out["list"][0] == "$a + 3");
    CHECK(out["list"][1] == "literal");
  }
}

TEST_CASE("a disabled subtree does not disturb its enclosing scope") {
  json o = json{{"a", 1},
                {"skip", json{{"expressionist", false}, {"b", "$a + 1"}}},
                {"c", "$a + 10"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["skip"]["b"] == "$a + 1"); // untouched
    CHECK(out["c"] == 11);               // sibling still evaluated
  }
}

TEST_CASE("only a literal boolean false disables the subtree") {
  json o = json{{"expressionist", true}, {"a", 1}, {"b", "$a + 1"}};
  json out = eval_with(o, EvalMethod::GRAPH);
  CHECK(out["b"] == 2); // "true" does not disable

  json o2 = json{{"a", 1}, {"b", "$a + 1"}}; // key absent entirely
  json out2 = eval_with(o2, EvalMethod::GRAPH);
  CHECK(out2["b"] == 2);
}

TEST_CASE("a disabled root object makes the whole document a no-op") {
  json o = json{{"expressionist", false}, {"a", 1}, {"b", "$a + 1"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["b"] == "$a + 1");
  }
}

TEST_CASE("the disable key is configurable") {
  json o = json{{"skipEval", false}, {"a", 1}, {"b", "$a + 1"}};
  Engine ex(o);
  ex.setDisableKey("skipEval");
  CHECK(ex.disableKey() == "skipEval");
  ex.evaluate();
  CHECK(ex.object()["b"] == "$a + 1");
}

TEST_CASE("an empty disable key turns the opt-out mechanism off") {
  json o = json{{"expressionist", false}, {"a", 1}, {"b", "$a + 1"}};
  Engine ex(o);
  ex.setDisableKey("");
  ex.evaluate();
  CHECK(ex.object()["b"] == 2); // "expressionist" is now just an inert field
}
