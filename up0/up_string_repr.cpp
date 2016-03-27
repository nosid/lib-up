#include "up_string_repr.hpp"


template class up_string_repr::string_repr::storage_deleter<false>;
template class up_string_repr::string_repr::storage_deleter<true>;

template auto up_string_repr::string_repr::make_storage<false>(size_type capacity, size_type size) -> storage_ptr<false>;
template auto up_string_repr::string_repr::make_storage<true>(size_type capacity, size_type size) -> storage_ptr<true>;

template auto up_string_repr::string_repr::clone_storage<false>(const storage& storage) -> storage_ptr<false>;
template auto up_string_repr::string_repr::clone_storage<true>(const storage& storage) -> storage_ptr<true>;

template class up_string_repr::string_repr::handle<false>;
template class up_string_repr::string_repr::handle<true>;
