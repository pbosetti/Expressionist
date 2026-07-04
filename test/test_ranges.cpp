#include <doctest/doctest.h>

#include "test_common.hpp"

using ExprError = Expressionist::ExpressionistException;

// ---------------------------------------------------------------------------
// Binary range "start:stop" (integer bounds, implicit step 1)
// ---------------------------------------------------------------------------

TEST_CASE("binary range produces an inclusive integer sequence") {
  CHECK(eval_expr("$1:5") == json::array({1, 2, 3, 4, 5}));
}

TEST_CASE("binary range elements are integers, not floats") {
  json r = eval_expr("$1:3");
  REQUIRE(r.is_array());
  for (const auto &el : r)
    CHECK(el.is_number_integer());
}

TEST_CASE("binary range with equal bounds is a single element") {
  CHECK(eval_expr("$3:3") == json::array({3}));
}

TEST_CASE("binary range going the wrong way is empty") {
  json r = eval_expr("$5:1");
  CHECK(r.is_array());
  CHECK(r.empty());
}

TEST_CASE("binary range accepts negative and variable bounds") {
  CHECK(eval_expr("$-2:2") == json::array({-2, -1, 0, 1, 2}));

  json o = json{{"n", 4}, {"r", "$1:n"}};
  Engine ex(o);
  ex.evaluate();
  CHECK(ex.object()["r"] == json::array({1, 2, 3, 4}));
}

// ---------------------------------------------------------------------------
// Ternary range "start:stop:step" (floating)
// ---------------------------------------------------------------------------

TEST_CASE("ternary range produces an inclusive floating sequence") {
  json r = eval_expr("$0:1:0.25");
  REQUIRE(r.is_array());
  REQUIRE(r.size() == 5);
  const double expected[] = {0.0, 0.25, 0.5, 0.75, 1.0};
  for (std::size_t i = 0; i < r.size(); ++i)
    CHECK(r[i].get<double>() == doctest::Approx(expected[i]));
}

TEST_CASE("ternary range is always floating, even with integer parts") {
  json r = eval_expr("$1:5:2");
  REQUIRE(r.is_array());
  REQUIRE(r.size() == 3);
  for (const auto &el : r)
    CHECK(el.is_number_float());
  CHECK(r[0].get<double>() == doctest::Approx(1.0));
  CHECK(r[2].get<double>() == doctest::Approx(5.0));
}

TEST_CASE("ternary range keeps the endpoint inclusive despite rounding") {
  json r = eval_expr("$0:1:0.1");
  REQUIRE(r.is_array());
  CHECK(r.size() == 11);
  CHECK(r.back().get<double>() == doctest::Approx(1.0));
}

TEST_CASE("ternary range supports a negative step") {
  json r = eval_expr("$1:0:-0.5");
  REQUIRE(r.size() == 3);
  CHECK(r[0].get<double>() == doctest::Approx(1.0));
  CHECK(r[1].get<double>() == doctest::Approx(0.5));
  CHECK(r[2].get<double>() == doctest::Approx(0.0));
}

TEST_CASE("ternary range with a step in the wrong direction is empty") {
  json r = eval_expr("$0:5:-1");
  CHECK(r.is_array());
  CHECK(r.empty());
}

TEST_CASE("ternary range parts may be arbitrary expressions") {
  json r = eval_expr("$0:2*pi:pi/2");
  REQUIRE(r.size() == 5);
  CHECK(r.front().get<double>() == doctest::Approx(0.0));
  CHECK(r.back().get<double>() == doctest::Approx(6.283185307179586));
}

// ---------------------------------------------------------------------------
// Both resolution strategies, and use inside a larger object
// ---------------------------------------------------------------------------

TEST_CASE("ranges evaluate identically under both strategies") {
  json o = json{{"a", 2}, {"seq", "$1:a + 1"}}; // ':' binds looser than '+'
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(o, m);
    CHECK(out["seq"] == json::array({1, 2, 3}));
  }
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

TEST_CASE("a zero step is rejected") {
  CHECK_THROWS_AS(eval_expr("$0:5:0"), ExprError);
}

TEST_CASE("more than three parts is a parse error") {
  CHECK_THROWS_AS(eval_expr("$1:2:3:4"), ExprError);
}

TEST_CASE("binary range with non-integer bounds is rejected") {
  CHECK_THROWS_AS(eval_expr("$0:1.5"), ExprError);
}

TEST_CASE("a sequence cannot be used in scalar arithmetic") {
  json o = json{{"r", "$1:3"}, {"s", "$r + 1"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    Engine ex(o, m);
    CHECK_THROWS_AS(ex.evaluate(), ExprError);
  }
}
