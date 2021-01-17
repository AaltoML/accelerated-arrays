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
        for (std::size_t i = 0; i < N; ++i) result[i] = get<T>(x, y, i);
        return result;
    }

    template<class T, std::size_t N> void set(int x, int y, const std::array<T, N> &array) {
        checkType<T>();
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channels == N);
        ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height);
        for (std::size_t i = 0; i < N; ++i) set<T>(x, y, i, array[i]);
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
    template<class T> inline T get(int x, int y) const {
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channels == 1);
        return get<T>(x, y, 0);
    }

    template<class T> inline void set(int x, int y, T value) {
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channels == 1);
        return set<T>(x, y, 0, value);
    }

    template<class T> inline T get(int x, int y, Border border) const {
        if (!applyBorder(x, y, border)) return T(double(0));
        return get<T>(x, y);
    }

    // adapters for copying to/from possibly non-CPU images
    Future copyFrom(::accelerated::Image &other);
    Future copyTo(::accelerated::Image &other) const;

    Future readRaw(std::uint8_t *outputData) final;
    Future writeRaw(const std::uint8_t *inputData) final;

    /** Get pointer to raw data, use sparingly */
    std::uint8_t *getDataRaw();
    std::size_t bytesPerRow() const;

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

    /**
     * Create reference to existing data with possible padding at the end of the rows.
     * The width of the row is given in pixesls. This also guarantees the correct
     * alignment in bytes.
     */
    static std::unique_ptr<Image> createReference(
        int w, int h, int channels, DataType dtype, std::uint8_t *data, std::size_t rowWidthPixels);

    template <class T, int Chan> static std::unique_ptr<Image> createReference(int w, int h, T* data) {
        return createReference(w, h, Chan, getType<T>(), reinterpret_cast<std::uint8_t*>(data));
    }

    static ImageTypeSpec getSpec(int channels, DataType dtype);

protected:
    bool applyBorder(int &x, int &y, Border border) const;
    Image(int w, int h, int channels, DataType dtype);
};

#define X(dtype) \
    template<> dtype Image::get<dtype>(int x, int y, int channel) const; \
    template<> void Image::set<dtype>(int x, int y, int channel, dtype value);
ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
#undef X
}
}
