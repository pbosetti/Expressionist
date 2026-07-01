#include <doctest/doctest.h>

#include "test_common.hpp"

TEST_CASE("lexical scoping: inner expression sees outer variable") {
  json o = json{{"base", 10},
                {"inner", json{{"x", "$base + 5"}, {"y", "$x * 2"}}}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["inner"]["x"] == 15);
    CHECK(out["inner"]["y"] == 30);
  }
}

TEST_CASE("expressions inside arrays use the enclosing object scope") {
  json o = json{{"k", 3},
                {"list", json::array({"$k + 1", "$k * k", "literal"})}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["list"][0] == 4);
    CHECK(out["list"][1] == 9);
    CHECK(out["list"][2] == "literal");
  }
}

TEST_CASE("a key shadows a same-named constant") {
  json o = json{{"pi", 100}, {"x", "$pi + 1"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["x"] == 101);
  }
}

TEST_CASE("nested scopes shadow ancestor keys") {
  json o = json{{"v", 1},
                {"outer", json{{"v", 100}, {"r", "$v + 1"}}}};
  json out = eval_with(o, EvalMethod::GRAPH);
  CHECK(out["outer"]["r"] == 101); // nearest 'v' (=100) wins
}
