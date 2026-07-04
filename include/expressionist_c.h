/*! \file expressionist_c.h
 *  \brief Plain C ABI for Expressionist: JSON string in, JSON string out.
 *
 * Copyright 2026 Paolo Bosetti
 * SPDX-License-Identifier: Apache-2.0
 *
 * A minimal, language-agnostic wrapper around the header-only C++ library:
 * every call takes/returns UTF-8 JSON text, so it can be consumed from any
 * runtime with a C FFI (ctypes/cffi in Python, but also Ruby, Node, Rust...).
 * No C++ type (std::string, nlohmann::json, exceptions) ever crosses this
 * boundary.
 *
 * \code{.c}
 * #include <expressionist_c.h>
 * #include <stdio.h>
 *
 * int main(void) {
 *   ExpressionistHandle *h = expressionist_create();
 *   expressionist_set_tag(h, "$");
 *
 *   char *out_json = NULL, *out_error = NULL;
 *   if (expressionist_evaluate(h, "{\"a\":1,\"b\":2,\"c\":\"$a + b\"}",
 *                               &out_json, &out_error) == 0) {
 *     printf("%s\n", out_json); // {"a":1,"b":2,"c":3}
 *     expressionist_free_string(out_json);
 *   } else {
 *     fprintf(stderr, "error: %s\n", out_error);
 *     expressionist_free_string(out_error);
 *   }
 *
 *   expressionist_destroy(h);
 *   return 0;
 * }
 * \endcode
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

/*! Opaque handle bundling the tag, disable key and evaluation strategy;
 *  everything else (the JSON document being evaluated) is passed per call.
 *  Create with expressionist_create(), release with expressionist_destroy().
 */
typedef struct ExpressionistHandle ExpressionistHandle;

/*! Mirrors Expressionist::EvalMethod for expressionist_set_eval_method(). */
enum ExpressionistCMethod {
  EXPRESSIONIST_C_GRAPH = 0,    /*!< Topological (Kahn's algorithm). */
  EXPRESSIONIST_C_RECURSIVE = 1 /*!< Lazy, memoized, with cycle detection. */
};

/*! Creates a handle configured with the library defaults (tag `"$"`,
 *  disable key `"expressionist"`, EXPRESSIONIST_C_RECURSIVE).
 *  \return a new handle, or NULL only on allocation failure. */
EXPRESSIONIST_C_API ExpressionistHandle *expressionist_create(void);

/*! Releases a handle created by expressionist_create(). Accepts NULL. */
EXPRESSIONIST_C_API void expressionist_destroy(ExpressionistHandle *handle);

/*! Changes the prefix that marks a JSON string as an expression (see
 *  Expressionist::setTag()). Takes effect on the next
 *  expressionist_evaluate() call. A NULL handle or tag is a silent no-op. */
EXPRESSIONIST_C_API void expressionist_set_tag(ExpressionistHandle *handle,
                                               const char *tag);

/*! Changes the opt-out key that disables evaluation for a subtree (see
 *  Expressionist::setDisableKey()). A NULL handle or key is a silent
 *  no-op. */
EXPRESSIONIST_C_API void
expressionist_set_disable_key(ExpressionistHandle *handle, const char *key);

/*! Selects the resolution strategy; pass EXPRESSIONIST_C_GRAPH or
 *  EXPRESSIONIST_C_RECURSIVE (see Expressionist::setEvalMethod()). A NULL
 *  handle is a silent no-op. */
EXPRESSIONIST_C_API void
expressionist_set_eval_method(ExpressionistHandle *handle, int method);

/*! Parses `json_in`, evaluates it and writes the result to `*out_json`,
 *  returning 0; on failure (malformed JSON, undefined variable, circular
 *  dependency, or a NULL `handle`/`json_in`), writes a diagnostic message to
 *  `*out_error` instead and returns nonzero. Exactly one of the two
 *  out-parameters is set on return, the other is left NULL.
 *
 *  Both are heap-allocated: free whichever was set with
 *  expressionist_free_string() once you are done with it.
 *
 *  \param handle a handle from expressionist_create()
 *  \param json_in UTF-8 JSON document to evaluate
 *  \param out_json set to the evaluated JSON text on success
 *  \param out_error set to a diagnostic message on failure
 *  \return 0 on success, nonzero on failure */
EXPRESSIONIST_C_API int expressionist_evaluate(ExpressionistHandle *handle,
                                               const char *json_in,
                                               char **out_json,
                                               char **out_error);

/*! Frees a string returned via `out_json`/`out_error` by
 *  expressionist_evaluate(). Accepts NULL. */
EXPRESSIONIST_C_API void expressionist_free_string(char *s);

/*! \return the library version, as a statically-allocated string (do not
 *  free it). */
EXPRESSIONIST_C_API const char *expressionist_version(void);

#ifdef __cplusplus
}
#endif

#endif /* EXPRESSIONIST_C_H */
