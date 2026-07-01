#include <doctest/doctest.h>

#include "test_common.hpp"

using ExprError = Expressionist::ExpressionistException;

TEST_CASE("circular dependency throws in both methods") {
  json o = json{{"a", "$b + 1"}, {"b", "$a + 1"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    Engine ex(o, m);
    CHECK_THROWS_AS(ex.evaluate(), ExprError);
  }
}

TEST_CASE("self reference is circular") {
  json o = json{{"a", "$a + 1"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    Engine ex(o, m);
    CHECK_THROWS_AS(ex.evaluate(), ExprError);
  }
}

TEST_CASE("undefined variable throws") {
  json o = json{{"a", "$missing + 1"}};
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    Engine ex(o, m);
    CHECK_THROWS_AS(ex.evaluate(), ExprError);
  }
}

TEST_CASE("unknown function throws") {
  json o = json{{"a", "$nope(1)"}};
  Engine ex(o);
  CHECK_THROWS_AS(ex.evaluate(), ExprError);
}

TEST_CASE("wrong argument count throws") {
  json o = json{{"a", "$sin(1, 2)"}};
  Engine ex(o);
  CHECK_THROWS_AS(ex.evaluate(), ExprError);
}

TEST_CASE("non-numeric variable throws") {
  json o = json{{"a", "$b + 1"}, {"b", "plain string"}};
  Engine ex(o);
  CHECK_THROWS_AS(ex.evaluate(), ExprError);
}

TEST_CASE("parse errors throw with context") {
  for (const char *bad : {"$1 +", "$", "$sin(", "$2 @ 3"}) {
    json o = json{{"a", bad}};
    Engine ex(o);
    CHECK_THROWS_AS(ex.evaluate(), ExprError);
  }
}

TEST_CASE("error message carries the offending field") {
  json o = json{{"a", "$missing + 1"}};
  Engine ex(o);
  try {
    ex.evaluate();
    FAIL("expected an exception");
  } catch (const ExprError &e) {
    std::string msg = e.what();
    CHECK(msg.find("/a") != std::string::npos);
    CHECK(msg.find("missing") != std::string::npos);
  }
}
