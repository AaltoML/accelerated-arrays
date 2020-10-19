#pragma once

#include <sstream>

#include "image.hpp"
#include "adapters.hpp"

namespace accelerated {
namespace opengl {
namespace {

namespace glsl {

template <class T> std::string wrapToVec(const std::vector<T> &values, const ImageTypeSpec &spec) {
    aa_assert(!values.empty() && values.size() <= 4);
    std::ostringstream oss;
    if (values.size() == 1) {
        oss << values.at(0);
        return oss.str();
    }
    oss << getGlslVecType(spec);
    oss << "(";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ",";
        oss << values.at(i);
    }
    oss << ")";
    return oss.str();
}

template <class T> std::string wrapToFloatVec(const std::vector<T> &values) {
    return wrapToVec(values, Image::getSpec(int(values.size()), ImageTypeSpec::DataType::FLOAT32));
}

std::string swizzleSubset(std::size_t n) {
    aa_assert(n > 0 && n <= 4);
    return std::string("rgba").substr(0, n);
}

std::string floatVecType(int channels) {
    if (channels == 1) return "float";
    std::ostringstream oss;
    oss << "vec" << channels;
    return oss.str();
}

/*std::string convertToFloatOutputValue(const std::string &value, ImageTypeSpec::DataType dtype) {
    double maxValue = maxDataTypeValue(dtype);
    double minValue = minDataTypeValue(dtype);
    std::ostringstream oss;
    oss << "((" << value << ") / float(" << (maxValue - minValue) << ")";
    if (minValue != 0.0) {
        oss << " - float(" << (minValue/(maxValue - minValue)) << ")";
    }
    oss << ")";
    return oss.str();
}

std::string convertFromFloatInputValue(const std::string &value, ImageTypeSpec::DataType dtype) {
    double maxValue = maxDataTypeValue(dtype);
    double minValue = minDataTypeValue(dtype);
    std::ostringstream oss;
    oss << "(float(" << (maxValue - minValue) << ") * ((" << value << ")";
    if (minValue != 0.0) oss << " + (" << (minValue/(maxValue - minValue)) << ")";
    oss << "))";
    return oss.str();
}*/

}

}
}
}
