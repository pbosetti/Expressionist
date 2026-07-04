// Expressionist -- header-only evaluator of algebraic expressions embedded in
// nlohmann::json fields. See PURPOSE.md for the design rationale.
//
// Copyright 2026 Paolo Bosetti
// SPDX-License-Identifier: Apache-2.0
//
// A JSON string value that begins with the tag (default "$") is treated as an
// algebraic expression whose variables are other keys of the JSON tree. The
// class resolves inter-variable dependencies (order independent), detects
// circular dependencies and undefined variables, and supports the usual
// arithmetic, comparison and logical operators plus a library of mathematical
// functions and constants.

#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace Expressionist {

using json = nlohmann::json;

enum class EvalMethod { GRAPH, RECURSIVE };

class ExpressionistException : public std::exception {
public:
  explicit ExpressionistException(std::string message)
      : _message(std::move(message)) {}
  const char *what() const noexcept override { return _message.c_str(); }

private:
  std::string _message;
}; // class ExpressionistException

namespace detail {

// A computed value is an integer, a double, a boolean or a sequence. Preserving
// the integer/double distinction lets "$a + b" stay integral while "$pi/3" is a
// float, matching the behaviour documented in PURPOSE.md. A sequence (produced
// by the range operator "start:stop[:step]") is carried as a ready-made JSON
// array: it may be written into a field but not used in scalar arithmetic.
using Value = std::variant<std::int64_t, double, bool, json>;

inline bool is_int(const Value &v) {
  return std::holds_alternative<std::int64_t>(v);
}

inline bool is_bool_value(const Value &v) {
  return std::holds_alternative<bool>(v);
}

inline bool is_seq(const Value &v) { return std::holds_alternative<json>(v); }

inline double to_double(const Value &v) {
  if (std::holds_alternative<std::int64_t>(v))
    return static_cast<double>(std::get<std::int64_t>(v));
  if (std::holds_alternative<double>(v))
    return std::get<double>(v);
  if (std::holds_alternative<bool>(v))
    return std::get<bool>(v) ? 1.0 : 0.0;
  throw ExpressionistException("a sequence cannot be used as a number");
}

inline bool to_bool(const Value &v) {
  if (std::holds_alternative<bool>(v))
    return std::get<bool>(v);
  if (std::holds_alternative<std::int64_t>(v))
    return std::get<std::int64_t>(v) != 0;
  if (std::holds_alternative<double>(v))
    return std::get<double>(v) != 0.0;
  throw ExpressionistException("a sequence cannot be used as a boolean");
}

inline json to_json(const Value &v) {
  if (std::holds_alternative<std::int64_t>(v))
    return json(std::get<std::int64_t>(v));
  if (std::holds_alternative<bool>(v))
    return json(std::get<bool>(v));
  if (std::holds_alternative<double>(v))
    return json(std::get<double>(v));
  return std::get<json>(v);
}

// ---------------------------------------------------------------------------
// Symbol tables: constants and mathematical functions.
// ---------------------------------------------------------------------------

using UnaryFn = std::function<double(double)>;
using BinaryFn = std::function<double(double, double)>;

struct Symbols {
  std::map<std::string, double> constants;
  std::map<std::string, UnaryFn> unary;
  std::map<std::string, BinaryFn> binary;
}; // struct Symbols

inline Symbols default_symbols() {
  Symbols s;
  // Literals are used instead of M_PI/M_E for portability (MSVC does not define
  // them without _USE_MATH_DEFINES).
  s.constants["pi"] = 3.14159265358979323846;
  s.constants["e"] = 2.71828182845904523536;
  s.constants["tau"] = 6.28318530717958647692;

  s.unary["sin"] = [](double x) { return std::sin(x); };
  s.unary["cos"] = [](double x) { return std::cos(x); };
  s.unary["tan"] = [](double x) { return std::tan(x); };
  s.unary["asin"] = [](double x) { return std::asin(x); };
  s.unary["acos"] = [](double x) { return std::acos(x); };
  s.unary["atan"] = [](double x) { return std::atan(x); };
  s.unary["sinh"] = [](double x) { return std::sinh(x); };
  s.unary["cosh"] = [](double x) { return std::cosh(x); };
  s.unary["tanh"] = [](double x) { return std::tanh(x); };
  s.unary["exp"] = [](double x) { return std::exp(x); };
  s.unary["log"] = [](double x) { return std::log(x); }; // natural log
  s.unary["ln"] = [](double x) { return std::log(x); };
  s.unary["log10"] = [](double x) { return std::log10(x); };
  s.unary["log2"] = [](double x) { return std::log2(x); };
  s.unary["sqrt"] = [](double x) { return std::sqrt(x); };
  s.unary["cbrt"] = [](double x) { return std::cbrt(x); };
  s.unary["abs"] = [](double x) { return std::fabs(x); };
  s.unary["floor"] = [](double x) { return std::floor(x); };
  s.unary["ceil"] = [](double x) { return std::ceil(x); };
  s.unary["round"] = [](double x) { return std::round(x); };
  s.unary["trunc"] = [](double x) { return std::trunc(x); };
  s.unary["sign"] = [](double x) {
    return x > 0.0 ? 1.0 : (x < 0.0 ? -1.0 : 0.0);
  };

  s.binary["pow"] = [](double a, double b) { return std::pow(a, b); };
  s.binary["atan2"] = [](double a, double b) { return std::atan2(a, b); };
  s.binary["hypot"] = [](double a, double b) { return std::hypot(a, b); };
  s.binary["min"] = [](double a, double b) { return std::min(a, b); };
  s.binary["max"] = [](double a, double b) { return std::max(a, b); };
  s.binary["mod"] = [](double a, double b) { return std::fmod(a, b); };
  return s;
}

// ---------------------------------------------------------------------------
// Tokenizer
// ---------------------------------------------------------------------------

enum class TokType {
  Number,
  Ident,
  Plus,
  Minus,
  Star,
  Slash,
  Caret,
  LParen,
  RParen,
  Comma,
  Lt,
  Le,
  Gt,
  Ge,
  EqEq,
  NotEq,
  And,
  Or,
  Not,
  Colon,
  End
}; // enum class TokType

struct Token {
  TokType type;
  std::string text;
  Value value;     // valid when type == Number
  std::size_t pos; // position in the source expression
};                 // struct Token

class Tokenizer {
public:
  explicit Tokenizer(std::string src) : _src(std::move(src)) {}

  std::vector<Token> tokenize() {
    std::vector<Token> tokens;
    for (;;) {
      skip_ws();
      if (_i >= _src.size()) {
        tokens.push_back({TokType::End, "", Value{}, _i});
        break;
      }
      char c = _src[_i];
      if (is_digit(c) ||
          (c == '.' && _i + 1 < _src.size() && is_digit(_src[_i + 1])))
        tokens.push_back(read_number());
      else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
        tokens.push_back(read_ident());
      else
        tokens.push_back(read_op());
    }
    return tokens;
  }

private:
  static bool is_digit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
  }

  void skip_ws() {
    while (_i < _src.size() &&
           std::isspace(static_cast<unsigned char>(_src[_i])))
      ++_i;
  }

  Token read_number() {
    std::size_t start = _i;
    bool is_float = false;
    while (_i < _src.size() && is_digit(_src[_i]))
      ++_i;
    if (_i < _src.size() && _src[_i] == '.') {
      is_float = true;
      ++_i;
      while (_i < _src.size() && is_digit(_src[_i]))
        ++_i;
    }
    if (_i < _src.size() && (_src[_i] == 'e' || _src[_i] == 'E')) {
      is_float = true;
      ++_i;
      if (_i < _src.size() && (_src[_i] == '+' || _src[_i] == '-'))
        ++_i;
      while (_i < _src.size() && is_digit(_src[_i]))
        ++_i;
    }
    std::string text = _src.substr(start, _i - start);
    Value v;
    if (is_float) {
      v = std::stod(text);
    } else {
      try {
        v = static_cast<std::int64_t>(std::stoll(text));
      } catch (const std::exception &) {
        v = std::stod(text); // out of int64 range -> keep as double
      }
    }
    return {TokType::Number, text, v, start};
  }

  Token read_ident() {
    std::size_t start = _i;
    while (
        _i < _src.size() &&
        (std::isalnum(static_cast<unsigned char>(_src[_i])) || _src[_i] == '_'))
      ++_i;
    return {TokType::Ident, _src.substr(start, _i - start), Value{}, start};
  }

  Token read_op() {
    std::size_t start = _i;
    char c = _src[_i];
    auto two = [&](char a, char b) {
      return _i + 1 < _src.size() && _src[_i] == a && _src[_i + 1] == b;
    };
    if (two('<', '='))
      return _i += 2, Token{TokType::Le, "<=", {}, start};
    if (two('>', '='))
      return _i += 2, Token{TokType::Ge, ">=", {}, start};
    if (two('=', '='))
      return _i += 2, Token{TokType::EqEq, "==", {}, start};
    if (two('!', '='))
      return _i += 2, Token{TokType::NotEq, "!=", {}, start};
    if (two('&', '&'))
      return _i += 2, Token{TokType::And, "&&", {}, start};
    if (two('|', '|'))
      return _i += 2, Token{TokType::Or, "||", {}, start};
    switch (c) {
    case '+':
      return ++_i, Token{TokType::Plus, "+", {}, start};
    case '-':
      return ++_i, Token{TokType::Minus, "-", {}, start};
    case '*':
      return ++_i, Token{TokType::Star, "*", {}, start};
    case '/':
      return ++_i, Token{TokType::Slash, "/", {}, start};
    case '^':
      return ++_i, Token{TokType::Caret, "^", {}, start};
    case '(':
      return ++_i, Token{TokType::LParen, "(", {}, start};
    case ')':
      return ++_i, Token{TokType::RParen, ")", {}, start};
    case ',':
      return ++_i, Token{TokType::Comma, ",", {}, start};
    case ':':
      return ++_i, Token{TokType::Colon, ":", {}, start};
    case '<':
      return ++_i, Token{TokType::Lt, "<", {}, start};
    case '>':
      return ++_i, Token{TokType::Gt, ">", {}, start};
    case '!':
      return ++_i, Token{TokType::Not, "!", {}, start};
    default:
      throw ExpressionistException("Unexpected character '" +
                                   std::string(1, c) + "' at position " +
                                   std::to_string(start));
    }
  }

  std::string _src;
  std::size_t _i = 0;
}; // class Tokenizer

// ---------------------------------------------------------------------------
// Abstract syntax tree
// ---------------------------------------------------------------------------

enum class NodeKind {
  IntLit,
  FloatLit,
  BoolLit,
  Ident,
  Unary,
  Binary,
  Call,
  Range
};

struct Node {
  NodeKind kind;
  Value value;                                 // literals
  std::string name;                            // Ident / Call name
  TokType op = TokType::End;                   // Unary / Binary operator
  std::vector<std::shared_ptr<Node>> children; // operands / call arguments
};                                             // struct Node

using NodePtr = std::shared_ptr<Node>;

class Parser {
public:
  Parser(std::vector<Token> tokens, std::string src)
      : _tokens(std::move(tokens)), _src(std::move(src)) {}

  NodePtr parse() {
    NodePtr n = parse_range();
    if (!check(TokType::End))
      throw error("unexpected trailing token");
    return n;
  }

private:
  const Token &peek() const { return _tokens[_pos]; }
  const Token &advance() { return _tokens[_pos++]; }
  bool check(TokType t) const { return peek().type == t; }
  bool match(TokType t) {
    if (check(t)) {
      ++_pos;
      return true;
    }
    return false;
  }

  ExpressionistException error(const std::string &why) const {
    return ExpressionistException("Parse error in \"" + _src + "\": " + why +
                                  " ('" + peek().text + "' at position " +
                                  std::to_string(peek().pos) + ")");
  }

  static NodePtr make_binary(TokType op, NodePtr l, NodePtr r) {
    auto n = std::make_shared<Node>();
    n->kind = NodeKind::Binary;
    n->op = op;
    n->children = {std::move(l), std::move(r)};
    return n;
  }

  // A range is the lowest-precedence construct and appears only at the top of
  // an expression: "start:stop" (integer bounds, implicit step 1) or
  // "start:stop:step" (floating). Its parts are ordinary scalar expressions.
  NodePtr parse_range() {
    NodePtr first = parse_or();
    if (!check(TokType::Colon))
      return first;
    auto node = std::make_shared<Node>();
    node->kind = NodeKind::Range;
    node->children.push_back(first);
    while (match(TokType::Colon))
      node->children.push_back(parse_or());
    if (node->children.size() > 3)
      throw error("a range has at most three parts (start:stop:step)");
    return node;
  }

  NodePtr parse_or() {
    NodePtr n = parse_and();
    while (check(TokType::Or)) {
      advance();
      n = make_binary(TokType::Or, n, parse_and());
    }
    return n;
  }

  NodePtr parse_and() {
    NodePtr n = parse_equality();
    while (check(TokType::And)) {
      advance();
      n = make_binary(TokType::And, n, parse_equality());
    }
    return n;
  }

  NodePtr parse_equality() {
    NodePtr n = parse_comparison();
    while (check(TokType::EqEq) || check(TokType::NotEq)) {
      TokType op = advance().type;
      n = make_binary(op, n, parse_comparison());
    }
    return n;
  }

  NodePtr parse_comparison() {
    NodePtr n = parse_additive();
    while (check(TokType::Lt) || check(TokType::Le) || check(TokType::Gt) ||
           check(TokType::Ge)) {
      TokType op = advance().type;
      n = make_binary(op, n, parse_additive());
    }
    return n;
  }

  NodePtr parse_additive() {
    NodePtr n = parse_multiplicative();
    while (check(TokType::Plus) || check(TokType::Minus)) {
      TokType op = advance().type;
      n = make_binary(op, n, parse_multiplicative());
    }
    return n;
  }

  NodePtr parse_multiplicative() {
    NodePtr n = parse_unary();
    while (check(TokType::Star) || check(TokType::Slash)) {
      TokType op = advance().type;
      n = make_binary(op, n, parse_unary());
    }
    return n;
  }

  NodePtr parse_unary() {
    if (check(TokType::Minus) || check(TokType::Not)) {
      TokType op = advance().type;
      auto n = std::make_shared<Node>();
      n->kind = NodeKind::Unary;
      n->op = op;
      n->children = {parse_unary()};
      return n;
    }
    return parse_power();
  }

  // Power binds tighter than unary minus (so -2^2 == -(2^2)) and is
  // right-associative (2^3^2 == 2^(3^2)); its exponent may itself be unary.
  NodePtr parse_power() {
    NodePtr base = parse_primary();
    if (check(TokType::Caret)) {
      advance();
      return make_binary(TokType::Caret, base, parse_unary());
    }
    return base;
  }

  NodePtr parse_primary() {
    const Token &t = peek();
    if (t.type == TokType::Number) {
      advance();
      auto n = std::make_shared<Node>();
      n->kind = is_int(t.value) ? NodeKind::IntLit : NodeKind::FloatLit;
      n->value = t.value;
      return n;
    }
    if (t.type == TokType::LParen) {
      advance();
      NodePtr n = parse_or();
      if (!match(TokType::RParen))
        throw error("expected ')'");
      return n;
    }
    if (t.type == TokType::Ident) {
      advance();
      if (t.text == "true" || t.text == "false") {
        auto n = std::make_shared<Node>();
        n->kind = NodeKind::BoolLit;
        n->value = (t.text == "true");
        return n;
      }
      if (check(TokType::LParen)) {
        advance();
        auto n = std::make_shared<Node>();
        n->kind = NodeKind::Call;
        n->name = t.text;
        if (!check(TokType::RParen)) {
          n->children.push_back(parse_or());
          while (match(TokType::Comma))
            n->children.push_back(parse_or());
        }
        if (!match(TokType::RParen))
          throw error("expected ')' in call to '" + t.text + "'");
        return n;
      }
      auto n = std::make_shared<Node>();
      n->kind = NodeKind::Ident;
      n->name = t.text;
      return n;
    }
    throw error("unexpected token");
  }

  std::vector<Token> _tokens;
  std::string _src;
  std::size_t _pos = 0;
}; // class Parser

// Names appearing in variable position (constants included; the caller decides
// which are keys vs. constants). Function names are not collected.
inline void collect_idents(const NodePtr &n, std::vector<std::string> &out) {
  if (!n)
    return;
  if (n->kind == NodeKind::Ident)
    out.push_back(n->name);
  for (const auto &c : n->children)
    collect_idents(c, out);
}

using Resolver = std::function<std::optional<Value>(const std::string &)>;

inline Value num_binary(const Value &l, const Value &r, TokType op) {
  if (op == TokType::Slash)
    return to_double(l) / to_double(r); // division is always floating point
  if (op == TokType::Caret) {
    if (is_int(l) && is_int(r) && std::get<std::int64_t>(r) >= 0) {
      std::int64_t base = std::get<std::int64_t>(l);
      std::int64_t exp = std::get<std::int64_t>(r);
      std::int64_t result = 1;
      for (std::int64_t k = 0; k < exp; ++k)
        result *= base;
      return result;
    }
    return std::pow(to_double(l), to_double(r));
  }
  if (is_int(l) && is_int(r)) {
    std::int64_t a = std::get<std::int64_t>(l);
    std::int64_t b = std::get<std::int64_t>(r);
    switch (op) {
    case TokType::Plus:
      return a + b;
    case TokType::Minus:
      return a - b;
    case TokType::Star:
      return a * b;
    default:
      break;
    }
  }
  double a = to_double(l);
  double b = to_double(r);
  switch (op) {
  case TokType::Plus:
    return a + b;
  case TokType::Minus:
    return a - b;
  case TokType::Star:
    return a * b;
  default:
    return 0.0; // unreachable
  }
}

inline Value eval_node(const NodePtr &n, const Symbols &sym,
                       const Resolver &resolve) {
  switch (n->kind) {
  case NodeKind::IntLit:
  case NodeKind::FloatLit:
  case NodeKind::BoolLit:
    return n->value;
  case NodeKind::Ident: {
    if (auto v = resolve(n->name)) // a key shadows a same-named constant
      return *v;
    auto it = sym.constants.find(n->name);
    if (it != sym.constants.end())
      return it->second;
    throw ExpressionistException("Undefined variable or constant: '" + n->name +
                                 "'");
  }
  case NodeKind::Unary: {
    if (n->op == TokType::Not)
      return !to_bool(eval_node(n->children[0], sym, resolve));
    Value c = eval_node(n->children[0], sym, resolve);
    if (is_int(c))
      return -std::get<std::int64_t>(c);
    return -to_double(c);
  }
  case NodeKind::Binary: {
    if (n->op == TokType::And) {
      if (!to_bool(eval_node(n->children[0], sym, resolve)))
        return false;
      return to_bool(eval_node(n->children[1], sym, resolve));
    }
    if (n->op == TokType::Or) {
      if (to_bool(eval_node(n->children[0], sym, resolve)))
        return true;
      return to_bool(eval_node(n->children[1], sym, resolve));
    }
    Value l = eval_node(n->children[0], sym, resolve);
    Value r = eval_node(n->children[1], sym, resolve);
    switch (n->op) {
    case TokType::Plus:
    case TokType::Minus:
    case TokType::Star:
    case TokType::Slash:
    case TokType::Caret:
      return num_binary(l, r, n->op);
    case TokType::Lt:
      return to_double(l) < to_double(r);
    case TokType::Le:
      return to_double(l) <= to_double(r);
    case TokType::Gt:
      return to_double(l) > to_double(r);
    case TokType::Ge:
      return to_double(l) >= to_double(r);
    case TokType::EqEq:
      if (is_bool_value(l) && is_bool_value(r))
        return std::get<bool>(l) == std::get<bool>(r);
      return to_double(l) == to_double(r);
    case TokType::NotEq:
      if (is_bool_value(l) && is_bool_value(r))
        return std::get<bool>(l) != std::get<bool>(r);
      return to_double(l) != to_double(r);
    default:
      throw ExpressionistException("Internal error: bad binary operator");
    }
  }
  case NodeKind::Range: {
    if (n->children.size() == 2) {
      Value a = eval_node(n->children[0], sym, resolve);
      Value b = eval_node(n->children[1], sym, resolve);
      if (!is_int(a) || !is_int(b))
        throw ExpressionistException(
            "Binary range 'start:stop' requires integer bounds");
      std::int64_t start = std::get<std::int64_t>(a);
      std::int64_t stop = std::get<std::int64_t>(b);
      json arr = json::array();
      for (std::int64_t v = start; v <= stop; ++v)
        arr.push_back(v);
      return Value(std::in_place_type<json>, std::move(arr));
    }
    if (n->children.size() == 3) {
      double start = to_double(eval_node(n->children[0], sym, resolve));
      double stop = to_double(eval_node(n->children[1], sym, resolve));
      double step = to_double(eval_node(n->children[2], sym, resolve));
      if (step == 0.0)
        throw ExpressionistException("Range step must be non-zero");
      json arr = json::array();
      double span = stop - start;
      // Generate only when the step points from start towards stop; a step in
      // the wrong direction yields an empty sequence rather than diverging. The
      // small epsilon keeps the endpoint inclusive despite rounding, so
      // "0:1:0.1" ends exactly at 1.
      if (span == 0.0 || (span > 0.0) == (step > 0.0)) {
        std::int64_t count =
            static_cast<std::int64_t>(std::floor(span / step + 1e-9));
        for (std::int64_t i = 0; i <= count; ++i)
          arr.push_back(start + static_cast<double>(i) * step);
      }
      return Value(std::in_place_type<json>, std::move(arr));
    }
    throw ExpressionistException(
        "A range must have two (start:stop) or three (start:stop:step) parts");
  }
  case NodeKind::Call: {
    const std::string &fn = n->name;
    auto u = sym.unary.find(fn);
    if (u != sym.unary.end()) {
      if (n->children.size() != 1)
        throw ExpressionistException("Function '" + fn +
                                     "' expects 1 argument, got " +
                                     std::to_string(n->children.size()));
      return u->second(to_double(eval_node(n->children[0], sym, resolve)));
    }
    auto b = sym.binary.find(fn);
    if (b != sym.binary.end()) {
      if (n->children.size() != 2)
        throw ExpressionistException("Function '" + fn +
                                     "' expects 2 arguments, got " +
                                     std::to_string(n->children.size()));
      double x = to_double(eval_node(n->children[0], sym, resolve));
      double y = to_double(eval_node(n->children[1], sym, resolve));
      return b->second(x, y);
    }
    throw ExpressionistException("Unknown function: '" + fn + "'");
  }
  }
  throw ExpressionistException("Internal error: bad node");
}

// ---------------------------------------------------------------------------
// Evaluator: walks a JSON tree, builds scopes/cells and evaluates expressions.
// ---------------------------------------------------------------------------

class Evaluator {
public:
  Evaluator(const Symbols &sym, std::string tag, EvalMethod method)
      : _sym(sym), _tag(std::move(tag)), _method(method) {}

  void run(json &root) {
    _cells.clear();
    _scopes.clear();
    walk(root, nullptr, "");
    if (_method == EvalMethod::GRAPH)
      eval_graph();
    else
      eval_recursive();
    for (const Cell &c : _cells)
      if (c.is_expr) // all expression cells are evaluated on success
        *c.slot = to_json(c.result);
  }

private:
  struct Scope {
    Scope *parent = nullptr;
    std::map<std::string, std::size_t> keys; // key -> cell index
  };

  struct Cell {
    json *slot = nullptr; // location to overwrite with the computed value
    Scope *scope = nullptr;
    std::string name; // last path component, for diagnostics
    std::string path; // full path, for diagnostics
    bool is_expr = false;
    std::string source; // expression text (tag stripped)
    NodePtr ast;
    Value result;
    int state = 0; // 0 unvisited, 1 visiting, 2 done (recursive method)
  };

  bool starts_with_tag(const std::string &s) const {
    return !_tag.empty() && s.rfind(_tag, 0) == 0;
  }

  std::size_t add_cell(json &value, Scope *scope, const std::string &path) {
    std::size_t idx = _cells.size();
    _cells.emplace_back();
    Cell &cell = _cells.back();
    cell.slot = &value;
    cell.scope = scope;
    cell.path = path;
    cell.name = last_component(path);
    if (value.is_string()) {
      const std::string &s = value.get_ref<const std::string &>();
      if (starts_with_tag(s)) {
        cell.is_expr = true;
        cell.source = s.substr(_tag.size());
        try {
          Tokenizer tok(cell.source);
          Parser parser(tok.tokenize(), cell.source);
          cell.ast = parser.parse();
        } catch (const ExpressionistException &e) {
          throw ExpressionistException(cell_context(cell) + e.what());
        }
      }
    }
    return idx;
  }

  void walk(json &value, Scope *enclosing, const std::string &path) {
    if (value.is_object()) {
      _scopes.emplace_back();
      Scope *scope = &_scopes.back();
      scope->parent = enclosing;
      // Register every member first so sibling references resolve regardless of
      // definition order.
      for (auto it = value.begin(); it != value.end(); ++it)
        scope->keys[it.key()] =
            add_cell(it.value(), scope, join(path, it.key()));
      // Then descend into nested containers for their own scopes / inner cells.
      for (auto it = value.begin(); it != value.end(); ++it)
        if (it.value().is_object() || it.value().is_array())
          walk(it.value(), scope, join(path, it.key()));
    } else if (value.is_array()) {
      // Arrays are not scopes: their elements resolve in the enclosing object.
      for (std::size_t i = 0; i < value.size(); ++i) {
        std::string p = path + "/" + std::to_string(i);
        json &el = value[i];
        if (el.is_object() || el.is_array())
          walk(el, enclosing, p);
        else
          add_cell(el, enclosing, p);
      }
    }
  }

  // Lexical lookup: nearest enclosing object scope, then ancestors.
  std::size_t resolve_name(const std::string &name, Scope *scope,
                           bool &found) const {
    for (Scope *s = scope; s != nullptr; s = s->parent) {
      auto it = s->keys.find(name);
      if (it != s->keys.end()) {
        found = true;
        return it->second;
      }
    }
    found = false;
    return 0;
  }

  Value literal_value(const Cell &c) const {
    const json &v = *c.slot;
    if (v.is_number_integer())
      return static_cast<std::int64_t>(v.get<std::int64_t>());
    if (v.is_number_float())
      return v.get<double>();
    if (v.is_boolean())
      return v.get<bool>();
    throw ExpressionistException("variable '" + c.name + "' is not numeric");
  }

  std::string cell_context(const Cell &c) const {
    return "In '" + c.path + "' (\"" + _tag + c.source + "\"): ";
  }

  // Add the offending cell's location once, avoiding duplicate prefixes as the
  // exception unwinds through nested evaluations.
  [[noreturn]] void
  rethrow_with_context(const Cell &c, const ExpressionistException &e) const {
    std::string msg = e.what();
    if (msg.rfind("In '", 0) == 0)
      throw e;
    throw ExpressionistException(cell_context(c) + msg);
  }

  // --- Recursive strategy -------------------------------------------------

  void eval_recursive() {
    std::vector<std::size_t> stack;
    for (std::size_t i = 0; i < _cells.size(); ++i)
      if (_cells[i].is_expr)
        eval_cell(i, stack);
  }

  Value eval_cell(std::size_t idx, std::vector<std::size_t> &stack) {
    Cell &cell = _cells[idx];
    if (!cell.is_expr)
      return literal_value(cell);
    if (cell.state == 2)
      return cell.result;
    if (cell.state == 1)
      throw ExpressionistException("Circular dependency: " +
                                   cycle_path(stack, idx));
    cell.state = 1;
    stack.push_back(idx);
    Scope *scope = cell.scope;
    Resolver resolver = [&](const std::string &name) -> std::optional<Value> {
      bool found;
      std::size_t tgt = resolve_name(name, scope, found);
      if (!found)
        return std::nullopt;
      return eval_cell(tgt, stack);
    };
    Value v;
    try {
      v = eval_node(cell.ast, _sym, resolver);
    } catch (const ExpressionistException &e) {
      rethrow_with_context(cell, e);
    }
    stack.pop_back();
    cell.result = v;
    cell.state = 2;
    return v;
  }

  std::string cycle_path(const std::vector<std::size_t> &stack,
                         std::size_t idx) const {
    std::size_t start = 0;
    for (std::size_t k = 0; k < stack.size(); ++k)
      if (stack[k] == idx) {
        start = k;
        break;
      }
    std::string m;
    for (std::size_t k = start; k < stack.size(); ++k)
      m += _cells[stack[k]].name + " -> ";
    m += _cells[idx].name;
    return m;
  }

  // --- Graph strategy (topological order via Kahn's algorithm) ------------

  void eval_graph() {
    std::size_t n = _cells.size();
    std::vector<std::vector<std::size_t>> dependents(n);
    std::vector<int> indeg(n, 0);
    std::vector<std::size_t> expr_cells;
    for (std::size_t i = 0; i < n; ++i) {
      if (!_cells[i].is_expr)
        continue;
      expr_cells.push_back(i);
      std::vector<std::string> idents;
      collect_idents(_cells[i].ast, idents);
      std::unordered_set<std::size_t> seen;
      for (const std::string &name : idents) {
        bool found;
        std::size_t tgt = resolve_name(name, _cells[i].scope, found);
        if (found && _cells[tgt].is_expr && seen.insert(tgt).second) {
          dependents[tgt].push_back(i);
          ++indeg[i];
        }
      }
    }
    std::queue<std::size_t> q;
    for (std::size_t i : expr_cells)
      if (indeg[i] == 0)
        q.push(i);
    std::size_t processed = 0;
    while (!q.empty()) {
      std::size_t i = q.front();
      q.pop();
      ++processed;
      eval_expr_graph(i);
      for (std::size_t d : dependents[i])
        if (--indeg[d] == 0)
          q.push(d);
    }
    if (processed != expr_cells.size()) {
      std::string names;
      for (std::size_t i : expr_cells)
        if (_cells[i].state != 2)
          names += _cells[i].name + " ";
      throw ExpressionistException("Circular dependency detected among: " +
                                   names);
    }
  }

  void eval_expr_graph(std::size_t idx) {
    Cell &cell = _cells[idx];
    Scope *scope = cell.scope;
    Resolver resolver = [&](const std::string &name) -> std::optional<Value> {
      bool found;
      std::size_t tgt = resolve_name(name, scope, found);
      if (!found)
        return std::nullopt;
      const Cell &t = _cells[tgt];
      if (t.is_expr)
        return t.result; // already evaluated: dependencies come first
      return literal_value(t);
    };
    try {
      cell.result = eval_node(cell.ast, _sym, resolver);
    } catch (const ExpressionistException &e) {
      rethrow_with_context(cell, e);
    }
    cell.state = 2;
  }

  static std::string join(const std::string &path, const std::string &key) {
    return path + "/" + key;
  }

  static std::string last_component(const std::string &path) {
    std::size_t p = path.find_last_of('/');
    return p == std::string::npos ? path : path.substr(p + 1);
  }

  const Symbols &_sym;
  std::string _tag;
  EvalMethod _method;
  std::deque<Scope> _scopes; // stable addresses for Scope*
  std::vector<Cell> _cells;
}; // class Evaluator

} // namespace detail

class Expressionist {
public:
  Expressionist(json o, EvalMethod method = EvalMethod::RECURSIVE)
      : _object(std::move(o)), _evalMethod(method),
        _symbols(detail::default_symbols()) {}
  Expressionist(std::string s, EvalMethod method = EvalMethod::RECURSIVE)
      : _evalMethod(method), _symbols(detail::default_symbols()) {
    try {
      _object = json::parse(s);
    } catch (const std::exception &e) {
      throw ExpressionistException("Failed to parse JSON: " +
                                   std::string(e.what()));
    }
  }
  // A string literal is otherwise ambiguous between the json and std::string
  // constructors (each a single user-defined conversion from const char*).
  // This exact-match overload disambiguates in favour of JSON parsing.
  Expressionist(const char *s, EvalMethod method = EvalMethod::RECURSIVE)
      : Expressionist(std::string(s), method) {}
  Expressionist(EvalMethod method = EvalMethod::RECURSIVE)
      : _evalMethod(method), _symbols(detail::default_symbols()) {}

  ~Expressionist() = default;

  // Evaluate in place, mutating the stored object. Throws
  // ExpressionistException (with context) on any failure.
  void evaluate() {
    detail::Evaluator ev(_symbols, _tag, _evalMethod);
    ev.run(_object);
  }

  // Evaluate in place, mutating the referenced object. Throws
  // ExpressionistException (with context) on any failure.
  void evaluate(json &object) const {
    detail::Evaluator ev(_symbols, _tag, _evalMethod);
    ev.run(object);
  }

  // Like evaluate(), but returns a new object and leaves the original intact.
  json produce() const {
    json copy = _object;
    detail::Evaluator ev(_symbols, _tag, _evalMethod);
    ev.run(copy);
    return copy;
  }

  // Like evaluate(), but returns a new object and leaves the original intact.
  json produce(json object) const {
    json copy = object;
    detail::Evaluator ev(_symbols, _tag, _evalMethod);
    ev.run(copy);
    return copy;
  }

  void setEvalMethod(EvalMethod method) { _evalMethod = method; }
  EvalMethod getEvalMethod() const { return _evalMethod; }

  void setTag(const std::string &tag) { _tag = tag; }
  std::string tag() const { return _tag; }

  const json &object() const { return _object; }

  // Extend the built-in symbol tables.
  void addConstant(const std::string &name, double value) {
    _symbols.constants[name] = value;
  }
  void addUnaryFunction(const std::string &name, detail::UnaryFn fn) {
    _symbols.unary[name] = std::move(fn);
  }
  void addBinaryFunction(const std::string &name, detail::BinaryFn fn) {
    _symbols.binary[name] = std::move(fn);
  }

private:
  json _object = json::object();
  EvalMethod _evalMethod = EvalMethod::RECURSIVE;
  std::string _tag = "$";
  detail::Symbols _symbols;
}; // class Expressionist

} // namespace Expressionist
