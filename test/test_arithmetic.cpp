#include <doctest/doctest.h>

#include "test_common.hpp"

TEST_CASE("integer arithmetic stays integral") {
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    CHECK(eval_expr("$1 + 2", m) == 3);
    CHECK(eval_expr("$1 + 2", m).is_number_integer());
    CHECK(eval_expr("$2 * 3 + 4", m) == 10);
    CHECK(eval_expr("$(2 + 3) * 4", m) == 20);
    CHECK(eval_expr("$2 ^ 10", m) == 1024);
    CHECK(eval_expr("$2 ^ 10", m).is_number_integer());
    CHECK(eval_expr("$10 - 4 - 3", m) == 3); // left associative
  }
}

TEST_CASE("division and float literals yield doubles") {
  CHECK(eval_expr("$7 / 2").get<double>() == doctest::Approx(3.5));
  CHECK(eval_expr("$7 / 2").is_number_float());
  CHECK(eval_expr("$2 ^ -1").get<double>() == doctest::Approx(0.5));
  CHECK(eval_expr("$2.0 + 1").is_number_float());
  CHECK(eval_expr("$1.5e1").get<double>() == doctest::Approx(15.0));
}

TEST_CASE("unary minus and operator precedence") {
  CHECK(eval_expr("$-2 ^ 2") == -4);     // -(2^2)
  CHECK(eval_expr("$-(3) + 5") == 2);
  CHECK(eval_expr("$2 ^ 3 ^ 2") == 512); // right associative: 2^(3^2)
  CHECK(eval_expr("$2 + 3 * 4") == 14);
}

TEST_CASE("references resolve regardless of key order") {
  json o = json{{"c", "$a + b"}, {"a", 1}, {"b", 2}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["c"] == 3);
    CHECK(out["c"].is_number_integer());
  }
}
