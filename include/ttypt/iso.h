/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef TTYPT_ISO_H
#define TTYPT_ISO_H

/**
 * @file iso.h
 * @brief Universal Isomorphic C capability export and import contract.
 *
 * Compiles symmetrically across native shared libraries (.so / .dylib)
 * and WebAssembly reactor bundles (.wasm) without requiring preprocessor
 * conditionals (#ifdef __wasm__) in application code.
 */

#if defined(__wasm__)

  /**
   * @brief Universal WebAssembly export attribute where C name == WASM export name.
   */
  #define ISO_EXPORT(ret, name, ...) \
      __attribute__((export_name(#name))) \
      ret name(__VA_ARGS__)

  /**
   * @brief Universal WebAssembly export attribute with an explicit WASM export alias.
   * Preserves existing internal C caller identifiers while exposing clean WASM export names.
   */
  #define ISO_EXPORT_AS(ret, c_name, wasm_name, ...) \
      __attribute__((export_name(#wasm_name))) \
      ret c_name(__VA_ARGS__)

  /**
   * @brief Universal WebAssembly host import attribute.
   * Binds return type, import module namespace, function name, and parameters.
   */
  #define ISO_IMPORT(ret, module_name, name, ...) \
      __attribute__((import_module(#module_name), import_name(#name))) \
      ret name(__VA_ARGS__)

#else

  /**
   * @brief Native shared object export attribute.
   * Publishes symbol with default visibility even when -fvisibility=hidden is enabled.
   */
  #define ISO_EXPORT(ret, name, ...) \
      __attribute__((visibility("default"))) \
      ret name(__VA_ARGS__)

  #define ISO_EXPORT_AS(ret, c_name, wasm_name, ...) \
      __attribute__((visibility("default"))) \
      ret c_name(__VA_ARGS__)

  #define ISO_IMPORT(ret, module_name, name, ...) \
      ret name(__VA_ARGS__)

#endif

#endif /* TTYPT_ISO_H */
