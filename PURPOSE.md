# Expressionist

## Project purpose

A C++20 class that provides a way to evaluate algebraic exprerssions in JSON fields, replacing them with their values, assuming that variables are defined in the same JSON object. The class is header-only and is designed to be used in conjunction with a `nlohmann::json` library and to be consumed by CMake FetchContent.

## Details

An example of how to use the class is provided:

```json
{
  "a": 1,
  "b": 2,
  "c": "$a + b",
  "d": "This must remain a string", 
  "e": "while strings beginning with $ must be evaluated",
  "f": "$pi/3",
  "g": "$sin(f ) * b",
  "h": true,
  "i": false,
  "j": "$h && i"
}
```

which must be turned into:

```json
{
  "a": 1,
  "b": 2,
  "c": 3,
  "d": "This must remain a string", 
  "e": "while strings beginning with $ must be evaluated",
  "f": 1.0471975511965976,
  "g": 1.7320508075688774,
  "h": true,
  "i": false,
  "j": false
}
```

Note that the variables definition order is not granted, so we must cope with that. We might do that with a recursive approach or by building a graph of dependencies and evaluating it in topological order. Ideally, the strategy must be selectable by the user.

The class should also be able to handle circular dependencies and undefined variables and report them as errors (with context).

The tag `"$"` is used to identify expressions that need to be evaluated. Strings that do not start with `"$"` must remain unchanged.

The class should also be able to handle mathematical functions like `sin`, `cos`, `tan`, `log`, etc., and constants like `pi` and `e`. Power operations should also be supported, e.g., `"$a^2 + b"`.


## Testing

The `test` directory must be populated with a proper battery of tests to ensure the correctness of the class. The tests should cover various scenarios, including but not limited to:

- Simple arithmetic operations
- Use of mathematical functions and constants
- Handling of circular dependencies
- Handling of undefined variables
- Complex expressions with multiple variables and operations

There must also be a test that compares different methods benchmarking their performance and correctness, especially for large JSON objects with many variables and expressions.