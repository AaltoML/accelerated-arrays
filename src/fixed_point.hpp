#pragma once

#include <limits>

// NOTE: these are not fast but primarily for use test compatiblity with
// the OpenGL versions, which often use these
namespace accelerated {
template <class T> struct FixedPoint {
    T value;

    FixedPoint() : value(0) {}
    FixedPoint(double f) : value(fromFloat(f)) {}
    FixedPoint(const FixedPoint &other) : value(other.value) {}
    operator double() { return toFloat(); }

    double toFloat() const {
        return clamp(value / (max() - min()) - floatMin());
    }

    static T fromFloat(double d) {
        return static_cast<T>((clamp(d) + floatMin()) * (max() - min()));
    }

    inline static double min() { return std::numeric_limits<T>::lowest(); }
    inline static double max() { return std::numeric_limits<T>::max(); }
    //inline static double floatMin() { return min() < 0 ? -1.0 : 0.0; }
    inline static double floatMin() { return 0.0; }
    inline static double floatMax() { return 1.0;  }

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
