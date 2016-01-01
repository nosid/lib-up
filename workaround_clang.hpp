#pragma once

/*
 * This file contains compiler-specific workarounds. With the latest
 * compilers, there are no workarounds necessary.
 */

// The following GCC-specific builtin functions are not supported by
// Clang. The replacements return the wrong information, but at least it
// compiles.
#define __builtin_FILE() __FILE__
#define __builtin_LINE() __LINE__
