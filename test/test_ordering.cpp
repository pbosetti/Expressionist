#include <doctest/doctest.h>

#include "test_common.hpp"

TEST_CASE("dependency chain in reverse definition order") {
  json o = json{
      {"z", "$y + 1"}, {"y", "$x + 1"}, {"x", "$w + 1"}, {"w", 0}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["w"] == 0);
    CHECK(out["x"] == 1);
    CHECK(out["y"] == 2);
    CHECK(out["z"] == 3);
  }
}

TEST_CASE("diamond dependency graph") {
  json o = json{{"top", 1},
                {"left", "$top + 1"},
                {"right", "$top + 2"},
                {"bottom", "$left + right"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["left"] == 2);
    CHECK(out["right"] == 3);
    CHECK(out["bottom"] == 5);
  }
}
