#pragma once

/*
 * This file contains compiler-specific workarounds. With the latest
 * compilers, there are no workarounds necessary.
 */

// GCC-specific builtin functions to get source information about the location
// of an invocation. There are proposals to add similar functionality to the
// C++ standard.
#define WORKAROUND_SOURCE_FILE() __builtin_FILE()
#define WORKAROUND_SOURCE_LINE() __builtin_LINE()
