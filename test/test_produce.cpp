#include <doctest/doctest.h>

#include "test_common.hpp"

TEST_CASE("produce() returns a new object and leaves the original intact") {
  json o = json{{"a", 1}, {"b", 2}, {"c", "$a + b"}};
  Engine ex(o);
  json result = ex.produce();
  CHECK(result["c"] == 3);
  CHECK(ex.object()["c"] == "$a + b"); // original untouched
}

TEST_CASE("evaluate() mutates in place") {
  json o = json{{"a", 1}, {"b", 2}, {"c", "$a + b"}};
  Engine ex(o);
  ex.evaluate();
  CHECK(ex.object()["c"] == 3);
}

TEST_CASE("changing the tag changes what is evaluated") {
  json o = json{{"a", 1}, {"b", "@a + 1"}, {"c", "$a + 1"}};
  Engine ex(o);
  ex.setTag("@");
  ex.evaluate();
  CHECK(ex.object()["b"] == 2);        // evaluated with tag '@'
  CHECK(ex.object()["c"] == "$a + 1"); // no longer an expression
}
