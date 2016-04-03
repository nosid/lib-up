#include "up_string.hpp"


template class up_string::traits_copy_fill<std::char_traits<char>>;
template class up_string::traits_assign_fill<std::char_traits<char>>;
template class up_string::traits_value_fill<std::char_traits<char>>;
template class up_string::traits_iterator_fill_base<std::char_traits<char>>;

template class up_string::basic_string<up::string_repr::handle<false, false>, false>;
template class up_string::basic_string<up::string_repr::handle<true, false>, true>;
