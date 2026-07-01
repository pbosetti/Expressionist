#include <doctest/doctest.h>

#include "test_common.hpp"

#include <chrono>
#include <cstdio>
#include <string>

namespace {

// A long chain v0=0, v1="$v0 + 1", ..., so v_i == i. Order is intentionally
// shuffled-friendly (both methods must reconstruct the dependency order).
json make_chain(int n) {
  json o = json::object();
  o["v0"] = 0;
  for (int i = 1; i < n; ++i)
    o["v" + std::to_string(i)] = "$v" + std::to_string(i - 1) + " + 1";
  return o;
}

// A wide fan-in: many leaves feeding one summing expression.
json make_fan(int n) {
  json o = json::object();
  std::string sum = "$l0";
  o["l0"] = 0;
  for (int i = 1; i < n; ++i) {
    o["l" + std::to_string(i)] = i;
    sum += " + l" + std::to_string(i);
  }
  o["total"] = sum;
  return o;
}

double ms(std::chrono::steady_clock::time_point a,
          std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

} // namespace

TEST_CASE("GRAPH and RECURSIVE agree on a large chain and are timed") {
  const int n = 1000;
  json input = make_chain(n);

  auto t0 = std::chrono::steady_clock::now();
  json g = eval_with(input, EvalMethod::GRAPH);
  auto t1 = std::chrono::steady_clock::now();
  json r = eval_with(input, EvalMethod::RECURSIVE);
  auto t2 = std::chrono::steady_clock::now();

  CHECK(g == r);
  CHECK(g["v" + std::to_string(n - 1)] == n - 1);

  std::printf("[benchmark chain n=%d] GRAPH=%.3f ms  RECURSIVE=%.3f ms\n", n,
              ms(t0, t1), ms(t1, t2));
  std::fflush(stdout);
}

TEST_CASE("GRAPH and RECURSIVE agree on a wide fan-in") {
  const int n = 500;
  json input = make_fan(n);

  auto t0 = std::chrono::steady_clock::now();
  json g = eval_with(input, EvalMethod::GRAPH);
  auto t1 = std::chrono::steady_clock::now();
  json r = eval_with(input, EvalMethod::RECURSIVE);
  auto t2 = std::chrono::steady_clock::now();

  CHECK(g == r);
  CHECK(g["total"] == (n - 1) * n / 2); // sum 0..n-1

  std::printf("[benchmark fan  n=%d] GRAPH=%.3f ms  RECURSIVE=%.3f ms\n", n,
              ms(t0, t1), ms(t1, t2));
  std::fflush(stdout);
}
