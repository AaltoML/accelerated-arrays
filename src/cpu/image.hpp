#pragma once

#include "../image.hpp"

#include <array>
#include <cstring>
#include <cassert>

// use normal, disableable assert for per-pixel checks
#define ACCELERATED_ARRAYS_PIXEL_ASSERT(...) assert(__VA_ARGS__)

namespace accelerated {
namespace cpu {
class Image : public ::accelerated::Image {
public:
    template<class T, std::size_t N> std::array<T, N> get(int x, int y) const {
        checkType<T>();
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channels == N);
        std::array<T, N> result;
        get(x, y, reinterpret_cast<std::uint8_t*>(&result));
        return result;
    }

    template<class T, std::size_t N> void set(int x, int y, const std::array<T, N> &array) {
        checkType<T>();
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channels == N);
        ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height);
        set(x, y, reinterpret_cast<const std::uint8_t*>(&array));
    }

    template<class T> T get(int x, int y, int channel) const;
    template<class T> void set(int x, int y, int channel, T value);

    template<class T> inline T get(int x, int y, int c, Border border) const {
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
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channels == 1);
        return get<T>(x, y, 0);
    }

    template<class T> void set(int x, int y, T value) {
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channels == 1);
        return set<T>(x, y, 0, value);
    }

    template<class T> T get(int x, int y, Border border) const {
        if (!applyBorder(x, y, border)) return T(double(0));
        return get<T>(x, y);
    }

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

    static ImageTypeSpec getSpec(int channels, DataType dtype);

protected:
    bool applyBorder(int &x, int &y, Border border) const;

    Image(int w, int h, int channels, DataType dtype);

    virtual void get(int x, int y, std::uint8_t *targetArray) const = 0;
    virtual void set(int x, int y, const std::uint8_t *srcArray) = 0;
    virtual void get(int x, int y, int channel, std::uint8_t *targetArray) const = 0;
    virtual void set(int x, int y, int channel, const std::uint8_t *srcArray) = 0;
};

#define X(dtype) \
    template<> dtype Image::get<dtype>(int x, int y, int channel) const; \
    template<> void Image::set<dtype>(int x, int y, int channel, dtype value);
ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
#undef X
}
}
