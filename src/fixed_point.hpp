#pragma once

#include <limits>

// NOTE: these are not fast but primarily for use test compatiblity with
// the OpenGL versions, which often use these
namespace accelerated {
template <class T> struct FixedPoint {
    T value;

    FixedPoint() : value(0) {}
    FixedPoint(double f) : value(fromFloat(f)) {}
    operator double() const { return toFloat(); }
    operator float() const { return toFloat(); }

    double toFloat() const {
        const double c = value;
        const double v = isSigned()
            ? (2 * c + 1) / unsignedMax()
            : c / max();
        return clamp(v);
    }

    static T fromFloat(double value) {
        // cf. https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glReadPixels.xhtml
        const double c = clamp(value);
        const double v = isSigned()
            ? (unsignedMax() * c - 1) / 2
            : max() * c;
        // This rounding to nearest was not specified in the glReadPixels manual
        // page, but with the fixed point types on the GPU, seems to work like
        // this
        return static_cast<T>(v + 0.5);
    }

    inline static constexpr double min() { return std::numeric_limits<T>::lowest(); }
    inline static constexpr double max() { return std::numeric_limits<T>::max(); }
    inline static constexpr bool isSigned() { return min() < 0.0; }
    inline static constexpr double unsignedMax() { return isSigned() ? (2 * max() + 1) : max(); }

    inline static constexpr double floatMin() { return isSigned() ? -1.0 : 0.0; }
    inline static constexpr double floatMax() { return 1.0;  }

    static double clamp(double d) {
        if (d < floatMin()) return floatMin();
        if (d > floatMax()) return floatMax();
        return d;
    }

    #define X(sym) inline FixedPoint<T> operator sym(const FixedPoint<T> &other) const \
        { return FixedPoint<T>(toFloat() sym other.toFloat()); }
    X(*)
    X(-)
    X(+)
    X(/)
    #undef X

    #define X(sym) inline FixedPoint<T> &operator sym(const FixedPoint<T> &other) \
        { double v = toFloat(); v sym other.toFloat(); value = fromFloat(v); return *this; }
    X(*=)
    X(-=)
    X(+=)
    X(/=)
    #undef X

    inline FixedPoint<T> operator -() const { return FixedPoint<T>(-toFloat()); }

    inline bool operator ==(const FixedPoint<T> &other) const { return value == other.value; }
    inline bool operator !=(const FixedPoint<T> &other) const { return value != other.value; }
};
}
