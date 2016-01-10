#pragma once

/*
 * This file contains compiler-specific workarounds. With the latest
 * compilers, there are no workarounds necessary.
 */

// GCC supports builtin functions to get source information about the location
// of an invocation. For Clang we fallback to __FILE__ and __LINE__. These
// replacements return the wrong information, but at least it compiles.
#define INVOCATION_FILE() __FILE__
#define INVOCATION_LINE() __LINE__
