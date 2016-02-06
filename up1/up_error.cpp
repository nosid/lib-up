#include "up_error.hpp"

template class up_error::default_constructible<std::exception>;
template class up_error::default_constructible<std::bad_exception>;
template class up_error::default_constructible<std::logic_error>;
template class up_error::default_constructible<std::domain_error>;
template class up_error::default_constructible<std::invalid_argument>;
template class up_error::default_constructible<std::length_error>;
template class up_error::default_constructible<std::out_of_range>;
template class up_error::default_constructible<std::runtime_error>;
template class up_error::default_constructible<std::range_error>;
template class up_error::default_constructible<std::overflow_error>;
template class up_error::default_constructible<std::underflow_error>;

template class up_error::unified<std::exception>;
template class up_error::unified<std::bad_exception>;
template class up_error::unified<std::logic_error>;
template class up_error::unified<std::domain_error>;
template class up_error::unified<std::invalid_argument>;
template class up_error::unified<std::length_error>;
template class up_error::unified<std::out_of_range>;
template class up_error::unified<std::runtime_error>;
template class up_error::unified<std::range_error>;
template class up_error::unified<std::overflow_error>;
template class up_error::unified<std::underflow_error>;
