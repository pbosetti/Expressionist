# Expressionist

[![CI](https://github.com/pbosetti/Expressionist/actions/workflows/ci.yml/badge.svg)](https://github.com/pbosetti/Expressionist/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)](src/expressionist.hpp)

![Expressionist](https://raw.githubusercontent.com/pbosetti/Expressionist/main/docs/Expressionist.png)

A small, header-only **C++20** library that evaluates algebraic expressions
embedded in [`nlohmann::json`](https://github.com/nlohmann/json) fields,
replacing them in place with their computed values. Variables in an expression
are simply other keys of the same JSON tree, so an object can describe a little
spreadsheet of interdependent values.

```jsonc
// input                             // output (after evaluate)
{                                    {
  "a": 1,                              "a": 1,
  "b": 2,                              "b": 2,
  "c": "$a + b",                       "c": 3,
  "d": "plain string",                 "d": "plain string",
  "f": "$pi/3",                        "f": 1.0471975511965976,
  "g": "$sin(f) * b",                  "g": 1.7320508075688772,
  "h": true,                           "h": true,
  "i": false,                          "i": false,
  "j": "$h && i"                       "j": false
}                                    }
```

A string is treated as an expression only when it starts with the **tag**
(default `"$"`); every other string is left untouched. The definition order of
the variables does not matter — dependencies are resolved automatically, and
circular dependencies or undefined variables are reported as errors *with
context*.

## Features

- **Header-only**, zero runtime dependencies beyond `nlohmann::json`.
- Hand-written tokenizer + Pratt parser — no third-party expression engine.
- Integer/double/boolean results, preserving integer-ness
  (`$a + b` stays `3`, `$pi/3` becomes a double).
- Arithmetic (`+ - * / ^`), comparisons (`< <= > >= == !=`) and logical
  operators (`&& || !`), with C-style precedence and short-circuiting.
- A library of mathematical functions and constants, extensible at runtime.
- Two interchangeable resolution strategies (topological **graph** and
  memoized **recursive**) selectable per instance.
- Recurses into nested objects and arrays with **lexical scoping**.
- Clear `ExpressionistException` messages carrying the offending field and
  expression.
- Consumable via CMake `FetchContent`.

## Requirements

- A C++20 compiler (tested with Clang / AppleClang).
- CMake ≥ 3.20 with Ninja recommended.
- `nlohmann::json` (fetched automatically when building this project).

## Integration

### CMake FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(
  Expressionist
  GIT_REPOSITORY https://github.com/pbosetti/Expressionist.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(Expressionist)

target_link_libraries(your_target PRIVATE Expressionist::Expressionist)
```

Linking the target transitively pulls in `nlohmann::json` and the include path,
so `#include <expressionist.hpp>` just works.

### Manual

Copy `src/expressionist.hpp` into your include path and make sure
`nlohmann/json.hpp` is reachable.

## Quick start

```cpp
#include <expressionist.hpp>
#include <iostream>

int main() {
  std::string json_text = R"({
    "a": 1,
    "b": 2,
    "c": "$a + b",
    "f": "$pi / 3",
    "g": "$sin(f) * b"
  })";

  // Instantiate with a json object
  nlohmann::json data = nlohmann::json::parse(json_text);
  Expressionist::Expressionist ex1(data);
  ex1.evaluate();            // mutate the stored object in place
  std::cout << ex1.object().dump(2) << '\n';
  
  // or, equivalently, from a string (a bare string literal works too — it is
  // parsed as JSON rather than stored as a string value):
  Expressionist::Expressionist ex2(json_text);
  ex2.evaluate();            // mutate the stored object in place
  std::cout << ex2.object().dump(2) << '\n';
}
```

Use `produce()` when you want a new object and need to keep the original intact:

```cpp
Expressionist::Expressionist ex(data);
nlohmann::json result = ex.produce();     // `data` is left unchanged
```

### Evaluating a separate object

An engine can also evaluate an object supplied at call time instead of the one
it stores. Construct it with just a strategy — the stored object stays empty —
and pass the target to `evaluate(json&)` (mutates it in place) or `produce(json)`
(returns an evaluated copy). Both reuse the engine's configured tag and symbol
table, so one engine can be applied to many objects:

```cpp
Expressionist::Expressionist ex(EvalMethod::RECURSIVE);
ex.addConstant("g0", 9.80665);

nlohmann::json a = nlohmann::json::parse(R"({"m": 2, "w": "$m * g0"})");
ex.evaluate(a);                           // mutates `a` in place

nlohmann::json b = ex.produce(            // returns a new object
    nlohmann::json::parse(R"({"x": 3, "y": "$x ^ 2"})"));
```

### Choosing a strategy

```cpp
using Expressionist::EvalMethod;

Expressionist::Expressionist ex(data, EvalMethod::RECURSIVE);
ex.setEvalMethod(EvalMethod::GRAPH);      // switch at any time
```

Both strategies produce identical results:

- `EvalMethod::GRAPH` builds a dependency graph and evaluates it in
  topological order (Kahn's algorithm).
- `EvalMethod::RECURSIVE` (default) evaluates lazily with memoization and a visiting-set
  for cycle detection.

### Changing the tag

```cpp
ex.setTag("@");     // now "@a + 1" is an expression and "$a + 1" is a literal
```

### Extending the symbol table

```cpp
ex.addConstant("g0", 9.80665);
ex.addUnaryFunction("double", [](double x) { return 2.0 * x; });
ex.addBinaryFunction("clampmax", [](double x, double m) { return std::min(x, m); });
```

### Error handling

```cpp
try {
  ex.evaluate();
} catch (const Expressionist::ExpressionistException &e) {
  std::cerr << e.what() << '\n';
  // e.g. In '/a' ("$missing + 1"): Undefined variable or constant: 'missing'
}
```

## Command-line tool

An optional `expressionist` executable is provided (target
`expressionist_cli`). It is **not built by default**; enable it with the
`EXPRESSIONIST_BUILD_TOOL` option, which also pulls in
[cxxopts](https://github.com/jarro2783/cxxopts) via FetchContent:

```sh
cmake -Bbuild -GNinja -DEXPRESSIONIST_BUILD_TOOL=ON
cmake --build build
```

The tool reads an input JSON, evaluates it, and prints the result to stdout.
The input comes either from a positional argument or, if none is given, from
stdin.

```sh
# JSON passed as an argument
expressionist '{"a":1,"b":2,"c":"$a + b","f":"$pi/3"}'

# JSON piped in on stdin
echo '{"a":1,"c":"$a + 10"}' | expressionist

# read from a file via the shell
expressionist < data.json
```

Output for the first command:

```json
{
  "a": 1,
  "b": 2,
  "c": 3,
  "f": 1.0471975511965976
}
```

Every class option is exposed as a flag:

| Option              | Description                                             | Default   |
|---------------------|---------------------------------------------------------|-----------|
| `-m, --method arg`  | Evaluation strategy: `graph` or `recursive`             | `graph`   |
| `-t, --tag arg`     | Prefix that marks a string as an expression             | `$`       |
| `--indent arg`      | Spaces used when pretty-printing the output             | `2`       |
| `-c, --compact`     | Emit compact, single-line JSON (overrides `--indent`)   | off       |
| `-h, --help`        | Print usage and exit                                    |           |
| `-V, --version`     | Print version and exit                                  |           |

```sh
expressionist --method recursive --tag @ --compact '{"a":1,"b":"@a + 1"}'
# -> {"a":1,"b":2}
```

**Exit codes:** `0` on success, `1` on a JSON or evaluation error (e.g. a
circular dependency or undefined variable, reported with context on stderr),
and `2` on a usage error (bad option, unknown method, or no input).

## Expression syntax

| Category      | Supported                                                                 |
|---------------|---------------------------------------------------------------------------|
| Literals      | integers, floats (`1.5`, `1.5e-3`), `true`, `false`                        |
| Arithmetic    | `+  -  *  /  ^` (power, right-associative), unary `-`                      |
| Comparison    | `<  <=  >  >=  ==  !=`                                                     |
| Logical       | `&&  \|\|  !` (short-circuiting, C-style truthiness)                       |
| Grouping      | `( … )`                                                                    |
| Constants     | `pi`, `e`, `tau`                                                           |
| Unary funcs   | `sin cos tan asin acos atan sinh cosh tanh exp log ln log10 log2 sqrt cbrt abs floor ceil round trunc sign` |
| Binary funcs  | `pow atan2 hypot min max mod`                                             |

Precedence (lowest → highest): `||`, `&&`, `== !=`, `< <= > >=`, `+ -`, `* /`,
unary `- !`, `^`, primary. Power binds tighter than unary minus, so `-2^2`
evaluates to `-4`, and `2^3^2` is `2^(3^2) = 512`.

**Result types.** `+ - *` keep integer operands integral; `/` always yields a
double; `^` stays integral for non-negative integer exponents; comparisons and
logical operators yield booleans; mathematical functions yield doubles.

### Nesting and scoping

Evaluation recurses into nested objects and arrays. Identifiers resolve
**lexically**: the nearest enclosing object's keys are searched first, then its
ancestors. Array elements resolve against their nearest enclosing object. A key
shadows a same-named constant.

```jsonc
{
  "base": 10,
  "inner": {
    "x": "$base + 5",   // -> 15  (sees the outer `base`)
    "y": "$x * 2"       // -> 30  (sees the sibling `x`)
  }
}
```

## API summary

| Member                                             | Purpose                                   |
|----------------------------------------------------|-------------------------------------------|
| `Expressionist(json, EvalMethod = RECURSIVE)`      | Construct from a JSON object.             |
| `Expressionist(std::string, EvalMethod = RECURSIVE)` | Construct by parsing a JSON string (a `const char*` literal binds here too). |
| `Expressionist(EvalMethod = RECURSIVE)`            | Construct with an empty object, for the by-argument overloads below. |
| `void evaluate()`                                  | Evaluate the stored object in place.      |
| `void evaluate(json&) const`                       | Evaluate the given object in place.       |
| `json produce() const`                             | Evaluate a copy of the stored object and return it. |
| `json produce(json) const`                         | Evaluate a copy of the given object and return it. |
| `void setEvalMethod(EvalMethod)` / `getEvalMethod()` | Select / query the strategy.            |
| `void setTag(const std::string&)` / `tag()`        | Set / query the expression tag.           |
| `const json& object() const`                       | Access the (possibly evaluated) object.   |
| `addConstant(name, value)`                         | Register a constant.                      |
| `addUnaryFunction(name, fn)`                       | Register a `double(double)` function.     |
| `addBinaryFunction(name, fn)`                      | Register a `double(double,double)` function. |

All failures throw `Expressionist::ExpressionistException`, whose `what()`
includes the JSON path and expression text.

## Building and testing

```sh
cmake -Bbuild -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

This also builds `example/main.cpp` (`build/expressionist_example`), which
prints the PURPOSE.md object evaluated with both strategies.

## Benchmark

The test suite includes a benchmark comparing the two strategies on large
interdependent objects and asserting that they return identical results. The
`GRAPH` method does a bit of graph bookkeeping up front, while `RECURSIVE`
resolves lazily; for these workloads they are close, with `RECURSIVE` slightly
ahead.

Indicative figures (Release build, AppleClang, Apple Silicon):

| Workload                              | GRAPH   | RECURSIVE |
|---------------------------------------|---------|-----------|
| Dependency chain, 1000 variables      | ~1.5 ms | ~1.1 ms   |
| Wide fan-in, 500 leaves → 1 sum       | ~0.6 ms | ~0.4 ms   |

Run it yourself:

```sh
./build/test/expressionist_tests --test-case="*benchmark*" --success
```

## License

Licensed under the [Apache License, Version 2.0](LICENSE).
