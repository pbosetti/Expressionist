// expressionist_c.cpp -- implementation of the C ABI declared in
// expressionist_c.h.
//
// Copyright 2026 Paolo Bosetti
// SPDX-License-Identifier: Apache-2.0

#include "expressionist_c.h"

#include <expressionist.hpp>

#include <cstdlib>
#include <cstring>

#ifndef EXPRESSIONIST_VERSION
#define EXPRESSIONIST_VERSION "unknown"
#endif

struct ExpressionistHandle {
  Expressionist::Expressionist engine;
};

namespace {

char *dup_cstr(const std::string &s) {
  char *out = static_cast<char *>(std::malloc(s.size() + 1));
  if (!out)
    return nullptr;
  std::memcpy(out, s.c_str(), s.size() + 1);
  return out;
}

} // namespace

extern "C" {

ExpressionistHandle *expressionist_create(void) {
  try {
    return new ExpressionistHandle{Expressionist::Expressionist()};
  } catch (...) {
    return nullptr;
  }
}

void expressionist_destroy(ExpressionistHandle *handle) { delete handle; }

void expressionist_set_tag(ExpressionistHandle *handle, const char *tag) {
  if (!handle || !tag)
    return;
  handle->engine.setTag(tag);
}

void expressionist_set_disable_key(ExpressionistHandle *handle,
                                    const char *key) {
  if (!handle || !key)
    return;
  handle->engine.setDisableKey(key);
}

void expressionist_set_eval_method(ExpressionistHandle *handle, int method) {
  if (!handle)
    return;
  handle->engine.setEvalMethod(method == EXPRESSIONIST_C_RECURSIVE
                                    ? Expressionist::EvalMethod::RECURSIVE
                                    : Expressionist::EvalMethod::GRAPH);
}

int expressionist_evaluate(ExpressionistHandle *handle, const char *json_in,
                            char **out_json, char **out_error) {
  if (out_json)
    *out_json = nullptr;
  if (out_error)
    *out_error = nullptr;
  if (!handle || !json_in) {
    if (out_error)
      *out_error = dup_cstr("expressionist_evaluate: null handle or input");
    return 1;
  }
  try {
    nlohmann::json parsed = nlohmann::json::parse(json_in);
    nlohmann::json result = handle->engine.produce(std::move(parsed));
    if (out_json)
      *out_json = dup_cstr(result.dump());
    return 0;
  } catch (const std::exception &e) {
    if (out_error)
      *out_error = dup_cstr(e.what());
    return 1;
  } catch (...) {
    if (out_error)
      *out_error = dup_cstr("expressionist_evaluate: unknown error");
    return 1;
  }
}

void expressionist_free_string(char *s) { std::free(s); }

const char *expressionist_version(void) { return EXPRESSIONIST_VERSION; }

} // extern "C"
