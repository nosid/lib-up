#include "up_throwable.hpp"

template class up_throwable::exception_adapter<std::exception>;
template class up_throwable::exception_adapter<std::bad_exception>;
template class up_throwable::exception_adapter<std::logic_error>;
template class up_throwable::exception_adapter<std::domain_error>;
template class up_throwable::exception_adapter<std::invalid_argument>;
template class up_throwable::exception_adapter<std::length_error>;
template class up_throwable::exception_adapter<std::out_of_range>;
template class up_throwable::exception_adapter<std::runtime_error>;
template class up_throwable::exception_adapter<std::range_error>;
template class up_throwable::exception_adapter<std::overflow_error>;
template class up_throwable::exception_adapter<std::underflow_error>;

template class up_throwable::hierarchy<std::exception>::bundle<>;
template class up_throwable::hierarchy<std::bad_exception>::bundle<>;
template class up_throwable::hierarchy<std::logic_error>::bundle<>;
template class up_throwable::hierarchy<std::domain_error>::bundle<>;
template class up_throwable::hierarchy<std::invalid_argument>::bundle<>;
template class up_throwable::hierarchy<std::length_error>::bundle<>;
template class up_throwable::hierarchy<std::out_of_range>::bundle<>;
template class up_throwable::hierarchy<std::runtime_error>::bundle<>;
template class up_throwable::hierarchy<std::range_error>::bundle<>;
template class up_throwable::hierarchy<std::overflow_error>::bundle<>;
template class up_throwable::hierarchy<std::underflow_error>::bundle<>;
