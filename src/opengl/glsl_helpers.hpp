#pragma once

#include "image.hpp"
#include "adapters.hpp"
#include <sstream>

namespace accelerated {
namespace opengl {
namespace {

double maxDataTypeValue(ImageTypeSpec::DataType dtype) {
    // NOTE: using floats as-is, and not squeezing to [0, 1]
    if (dtype == ImageTypeSpec::DataType::FLOAT32) return 1.0;
    return ImageTypeSpec::maxValueOf(dtype);
}

double minDataTypeValue(ImageTypeSpec::DataType dtype) {
    if (dtype == ImageTypeSpec::DataType::FLOAT32) return 0.0;
    return ImageTypeSpec::minValueOf(dtype);
}

namespace glsl {
template <class T> std::string wrapToVec(const std::vector<T> &values, const ImageTypeSpec &spec) {
    assert(!values.empty() && values.size() <= 4);
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

std::string swizzleSubset(std::size_t n) {
    assert(n > 0 && n <= 4);
    return std::string("rgba").substr(0, n);
}

std::string floatVecType(int channels) {
    if (channels == 0) return "float";
    std::ostringstream oss;
    oss << "vec" << channels;
    return oss.str();
}

std::string convertToFloatOutputValue(const std::string &value, ImageTypeSpec::DataType dtype) {
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
}

}

}
}
}
