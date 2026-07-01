#include <doctest/doctest.h>

#include "test_common.hpp"

TEST_CASE("full PURPOSE.md example, both methods") {
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    json out = eval_with(purpose_example(), m);
    CHECK(out["a"] == 1);
    CHECK(out["b"] == 2);
    CHECK(out["c"] == 3);
    CHECK(out["c"].is_number_integer());
    CHECK(out["d"] == "This must remain a string");
    CHECK(out["e"] == "while strings beginning with $ must be evaluated");
    CHECK(out["f"].get<double>() == doctest::Approx(1.0471975511965976));
    CHECK(out["g"].get<double>() == doctest::Approx(1.7320508075688772));
    CHECK(out["h"] == true);
    CHECK(out["i"] == false);
    CHECK(out["j"] == false);
  }
}

TEST_CASE("constants and mathematical functions") {
  CHECK(eval_expr("$pi").get<double>() == doctest::Approx(3.141592653589793));
  CHECK(eval_expr("$e").get<double>() == doctest::Approx(2.718281828459045));
  CHECK(eval_expr("$sin(0)").get<double>() == doctest::Approx(0.0));
  CHECK(eval_expr("$cos(0)").get<double>() == doctest::Approx(1.0));
  CHECK(eval_expr("$sqrt(16)").get<double>() == doctest::Approx(4.0));
  CHECK(eval_expr("$log(e)").get<double>() == doctest::Approx(1.0));
  CHECK(eval_expr("$pow(2, 8)").get<double>() == doctest::Approx(256.0));
  CHECK(eval_expr("$max(3, 7)").get<double>() == doctest::Approx(7.0));
  CHECK(eval_expr("$abs(-5)").get<double>() == doctest::Approx(5.0));
  CHECK(eval_expr("$hypot(3, 4)").get<double>() == doctest::Approx(5.0));
}

TEST_CASE("user-defined constants and functions") {
  json o = json{{"x", "$twice(g) + k"}, {"g", 4}};
  Engine ex(std::move(o));
  ex.addConstant("k", 10.0);
  ex.addUnaryFunction("twice", [](double v) { return 2.0 * v; });
  ex.evaluate();
  CHECK(ex.object()["x"].get<double>() == doctest::Approx(18.0));
}
