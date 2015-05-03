#pragma once

/*
 * This file contains compiler-specific workarounds. With the latest
 * compilers, there are no workarounds necessary.
 */

// Currently, Clang does not support thread_local variables. The following
// define is used to compile code with thread_local variables, even if it can
// not be (correctly) executed.
#define UP_WORKAROUND_THREAD_LOCAL
