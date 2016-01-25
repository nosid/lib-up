#pragma once

/**
 * @file
 *
 * @brief Header file to include frequently used headers from the standard
 * library.
 *
 * They are added once, to reduce the list of includes in the other. In the
 * future, it might also be used to improve compiler performance with
 * pre-compiled headers.
 *
 * Only standard headers should be added to this file. Only headers should be
 * included, if they are typically included anyway (indirectly), and don't
 * cause any harm.
 */

#include <cstddef>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

/**
 * @brief Designated namespace for the @c up library.
 *
 * This namespace should be used by all internal and external consumers of
 * library components, even if the entities are actually declared in different
 * namespaces.
 */
namespace up { }
