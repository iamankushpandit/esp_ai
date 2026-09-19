#include "tlib_tensor.h"

#include <print>

namespace tlib {

template <>
void TensorView<int8_t>::Summary(const std::string & name) const {
    BasicSummary(name);
    std::println("|--Type:\tINT8\n");
};

template <>
void TensorView<int16_t>::Summary(const std::string & name) const {
    BasicSummary(name);
    std::println("|--Type:\tINT16\n");
};

template <>
void TensorView<int32_t>::Summary(const std::string & name) const {
    BasicSummary(name);
    std::println("|--Type:\tINT32\n");
};

template <>
void TensorView<float>::Summary(const std::string & name) const {
    BasicSummary(name);
    std::println("|--Type:\tFLOAT32\n");
};

} // tlib