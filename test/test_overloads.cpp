#include <doctest/doctest.h>

#include "test_common.hpp"

using ExprError = Expressionist::ExpressionistException;

// ---------------------------------------------------------------------------
// Method-only (default) constructor
// ---------------------------------------------------------------------------

TEST_CASE("default constructor yields an empty object with the given method") {
  Engine ex;
  CHECK(ex.object().is_object());
  CHECK(ex.object().empty());
  CHECK(ex.getEvalMethod() == EvalMethod::RECURSIVE);

  Engine graph(EvalMethod::GRAPH);
  CHECK(graph.object().empty());
  CHECK(graph.getEvalMethod() == EvalMethod::GRAPH);
}

TEST_CASE("default-constructed engine evaluates external objects") {
  Engine ex(EvalMethod::GRAPH);
  json o = json{{"a", 1}, {"b", 2}, {"c", "$a + b"}};
  ex.evaluate(o);
  CHECK(o["c"] == 3);
  CHECK(ex.object().empty()); // stored object never populated
}

// ---------------------------------------------------------------------------
// String constructor
// ---------------------------------------------------------------------------

TEST_CASE("string constructor parses and evaluates JSON") {
  Engine ex(std::string(R"({"a": 1, "b": 2, "c": "$a + b"})"));
  ex.evaluate();
  CHECK(ex.object()["c"] == 3);
}

TEST_CASE("a string literal is parsed as JSON, not stored as a json string") {
  // The const char* overload disambiguates the literal towards JSON parsing.
  Engine ex(R"({"a": 1, "b": "$a + 1"})");
  ex.evaluate();
  CHECK(ex.object()["b"] == 2);
}

TEST_CASE("string constructor honours the eval method argument") {
  Engine ex(std::string(R"({"a": 1})"), EvalMethod::GRAPH);
  CHECK(ex.getEvalMethod() == EvalMethod::GRAPH);
}

TEST_CASE("string constructor rejects malformed JSON") {
  CHECK_THROWS_AS(Engine(std::string("{ not valid json")), ExprError);
}

// ---------------------------------------------------------------------------
// evaluate(json &) overload
// ---------------------------------------------------------------------------

TEST_CASE("evaluate(json&) mutates the argument, leaving the stored object "
          "intact") {
  Engine ex(json{{"a", 10}, {"b", "$a + 5"}});
  json other = json{{"x", 1}, {"y", "$x + 1"}};
  ex.evaluate(other);
  CHECK(other["y"] == 2);              // argument mutated in place
  CHECK(ex.object()["b"] == "$a + 5"); // stored object untouched
}

TEST_CASE("evaluate(json&) matches evaluate() on the same data") {
  json data = purpose_example();
  for (auto m : {EvalMethod::GRAPH, EvalMethod::RECURSIVE}) {
    Engine via_member(data, m);
    via_member.evaluate();

    Engine via_arg(m);
    json copy = data;
    via_arg.evaluate(copy);

    CHECK(copy == via_member.object());
  }
}

TEST_CASE("evaluate(json&) uses the engine's custom tag and symbols") {
  Engine ex(EvalMethod::RECURSIVE);
  ex.setTag("@");
  ex.addConstant("k", 100.0);
  json o = json{{"a", "@k + 1"}, {"b", "$k + 1"}};
  ex.evaluate(o);
  CHECK(o["a"] == 101.0);    // evaluated with tag '@' and custom constant k
  CHECK(o["b"] == "$k + 1"); // '$' is no longer the active tag
}

TEST_CASE("evaluate(json&) propagates evaluation errors") {
  Engine ex;
  json o = json{{"a", "$missing + 1"}};
  CHECK_THROWS_AS(ex.evaluate(o), ExprError);
}

// ---------------------------------------------------------------------------
// produce(json) overload
// ---------------------------------------------------------------------------

TEST_CASE("produce(json) returns an evaluated copy, leaving inputs intact") {
  Engine ex;
  json in = json{{"a", 2}, {"b", 3}, {"c", "$a * b"}};
  json out = ex.produce(in);
  CHECK(out["c"] == 6);
  CHECK(in["c"] == "$a * b"); // caller's object untouched (passed by value)
  CHECK(ex.object().empty()); // stored object untouched
}

TEST_CASE("produce(json) matches produce() for the same data") {
  json data = json{{"a", 1}, {"b", 2}, {"c", "$a + b"}};
  Engine stored(data);
  Engine empty;
  CHECK(empty.produce(data) == stored.produce());
}

TEST_CASE("produce(json) honours a custom tag and eval method") {
  Engine ex(EvalMethod::GRAPH);
  ex.setTag("#");
  json in = json{{"a", 16}, {"r", "#sqrt(a)"}};
  json out = ex.produce(in);
  CHECK(out["r"].get<double>() == doctest::Approx(4.0));
  CHECK(out["a"] == 16);
}

TEST_CASE("produce(json) propagates evaluation errors") {
  Engine ex;
  json in = json{{"a", "$b + 1"}, {"b", "$a + 1"}}; // circular
  CHECK_THROWS_AS(ex.produce(in), ExprError);
}
