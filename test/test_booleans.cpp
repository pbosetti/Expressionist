#include <doctest/doctest.h>

#include "test_common.hpp"

TEST_CASE("logical operators over boolean fields") {
  json o = json{{"h", true},   {"i", false},   {"j", "$h && i"},
                {"k", "$h || i"}, {"l", "$!i"}, {"m", "$h && !i"}};
  for (auto met : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, met);
    CHECK(out["j"] == false);
    CHECK(out["k"] == true);
    CHECK(out["l"] == true);
    CHECK(out["m"] == true);
  }
}

TEST_CASE("comparisons produce booleans") {
  CHECK(eval_expr("$3 < 5") == true);
  CHECK(eval_expr("$5 <= 5") == true);
  CHECK(eval_expr("$3 == 3") == true);
  CHECK(eval_expr("$3 != 4") == true);
  CHECK(eval_expr("$2 > 9") == false);
  CHECK(eval_expr("$2 >= 9") == false);
}

TEST_CASE("numeric truthiness is C-style") {
  CHECK(eval_expr("$1 && 2") == true); // nonzero is true
  CHECK(eval_expr("$0 || 0") == false);
  CHECK(eval_expr("$0 && 1") == false);
}
