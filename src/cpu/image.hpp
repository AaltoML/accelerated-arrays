#pragma once

#include "../image.hpp"

#include <array>
#include <cstring>

namespace accelerated {
namespace cpu {
class Image : public ::accelerated::Image {
public:
    template<class T, std::size_t N> std::array<T, N> get(int x, int y) const {
        checkType<T>();
        aa_assert(channels == N);
        std::array<T, N> result;
        get(x, y, reinterpret_cast<std::uint8_t*>(&result));
        return result;
    }

    template<class T, std::size_t N> void set(int x, int y, const std::array<T, N> &array) {
        checkType<T>();
        aa_assert(channels == N);
        aa_assert(x >= 0 && y >= 0 && x < width && y < height);
        set(x, y, reinterpret_cast<const std::uint8_t*>(&array));
    }

    template<class T> T get(int x, int y, int channel) const {
        checkType<T>();
        T result;
        get(x, y, channel, reinterpret_cast<std::uint8_t*>(&result));
        return result;
    }

    template<class T> void set(int x, int y, int channel, T value) {
        checkType<T>();
        aa_assert(x >= 0 && y >= 0 && x < width && y < height);
        set(x, y, channel, reinterpret_cast<const std::uint8_t*>(&value));
    }

    template<class T> T get(int x, int y, int c, Border border) const {
        if (!applyBorder(x, y, border)) return T(double(0));
        return get<T>(x, y, c);
    }

    template<class T, std::size_t N> std::array<T, N> get(int x, int y, Border border) const {
        if (!applyBorder(x, y, border)) {
            std::array<T, N> result;
            for (std::size_t i = 0; i < N; ++i) result.at(i) = T(double(0));
            return result;
        }
        return get<T, N>(x, y);
    }

    // single-channel shorthands
    template<class T> T get(int x, int y) const {
        return get<T, 1>(x, y).at(0);
    }

    template<class T> void set(int x, int y, T value) {
        set<T, 1>(x, y, { value });
    }

    template<class T> T get(int x, int y, Border border) const {
        if (!applyBorder(x, y, border)) return T(double(0));
        return get<T>(x, y);
    }

    // convert-to/from-double API: useful for avoiding template bloat
    // on the user side

    void setFloat(int x, int y, int channel, double value);
    double getFloat(int x, int y, int channel) const;
    double getFloat(int x, int y, int channel, Border border) const;

    // single channel shorthands (double API)
    void setFloat(int x, int y, double value);
    double getFloat(int x, int y) const;
    double getFloat(int x, int y, Border border) const;

    // adapters for copying to/from possibly non-CPU images
    virtual Future copyFrom(::accelerated::Image &other) = 0;
    virtual Future copyTo(::accelerated::Image &other) const = 0;

    /** Get pointer to raw data, use sparingly */
    virtual std::uint8_t *getDataRaw() = 0;

    /** Same as getRawData but checks that the type is what you expect */
    template <class T> T *getData() {
        checkType<T>();
        return reinterpret_cast<T*>(getDataRaw());
    }

    static Image &castFrom(::accelerated::Image &image) {
        aa_assert(image.storageType == StorageType::CPU);
        return reinterpret_cast<Image&>(image);
    }

    static std::unique_ptr<Factory> createFactory();

    /** Create a cpu::Image which as a reference to existing data */
    static std::unique_ptr<Image> createReference(
        int w, int h, int channels, DataType dtype, std::uint8_t *data);

    template <class T, int Chan> static std::unique_ptr<Image> createReference(int w, int h, T* data) {
        return createReference(w, h, Chan, getType<T>(), reinterpret_cast<std::uint8_t*>(data));
    }

protected:
    bool applyBorder(int &x, int &y, Border border) const;

    static ImageTypeSpec getSpec(int channels, DataType dtype);

    Image(int w, int h, int channels, DataType dtype);

    virtual void get(int x, int y, std::uint8_t *targetArray) const = 0;
    virtual void set(int x, int y, const std::uint8_t *srcArray) = 0;
    virtual void get(int x, int y, int channel, std::uint8_t *targetArray) const = 0;
    virtual void set(int x, int y, int channel, const std::uint8_t *srcArray) = 0;
};
}
}
