/* expressionist_c.h -- plain C ABI for Expressionist, JSON string in/out.
 *
 * Copyright 2026 Paolo Bosetti
 * SPDX-License-Identifier: Apache-2.0
 *
 * A minimal, language-agnostic wrapper around the header-only C++ library:
 * every call takes/returns UTF-8 JSON text, so it can be consumed from any
 * runtime with a C FFI (ctypes/cffi in Python, but also Ruby, Node, Rust...).
 * No C++ type (std::string, nlohmann::json, exceptions) ever crosses this
 * boundary.
 */

#ifndef EXPRESSIONIST_C_H
#define EXPRESSIONIST_C_H

#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef EXPRESSIONIST_C_BUILDING
#    define EXPRESSIONIST_C_API __declspec(dllexport)
#  else
#    define EXPRESSIONIST_C_API __declspec(dllimport)
#  endif
#else
#  define EXPRESSIONIST_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle bundling the tag, disable key and evaluation strategy;
 * everything else (the JSON document being evaluated) is passed per call. */
typedef struct ExpressionistHandle ExpressionistHandle;

enum ExpressionistCMethod { EXPRESSIONIST_C_GRAPH = 0, EXPRESSIONIST_C_RECURSIVE = 1 };

/* Returns NULL only on allocation failure. */
EXPRESSIONIST_C_API ExpressionistHandle *expressionist_create(void);
EXPRESSIONIST_C_API void expressionist_destroy(ExpressionistHandle *handle);

/* Setters take effect on the next expressionist_evaluate() call; a NULL
 * handle or NULL string argument is a silent no-op. */
EXPRESSIONIST_C_API void expressionist_set_tag(ExpressionistHandle *handle,
                                               const char *tag);
EXPRESSIONIST_C_API void
expressionist_set_disable_key(ExpressionistHandle *handle, const char *key);
EXPRESSIONIST_C_API void
expressionist_set_eval_method(ExpressionistHandle *handle, int method);

/* Parses json_in, evaluates it and writes the result to *out_json (0
 * returned) or a diagnostic message to *out_error (nonzero returned) --
 * exactly one of the two is set on return, the other is left NULL. Both are
 * heap-allocated; free whichever was set with expressionist_free_string().
 * A NULL handle or json_in is reported as an error, not a crash. */
EXPRESSIONIST_C_API int expressionist_evaluate(ExpressionistHandle *handle,
                                               const char *json_in,
                                               char **out_json,
                                               char **out_error);

EXPRESSIONIST_C_API void expressionist_free_string(char *s);

EXPRESSIONIST_C_API const char *expressionist_version(void);

#ifdef __cplusplus
}
#endif

#endif /* EXPRESSIONIST_C_H */
