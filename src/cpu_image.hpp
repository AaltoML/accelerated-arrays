#pragma once

#include "image.hpp"
#include <array>
#include <cassert>
#include <cstring>

namespace accelerated {

// CpuImage "reference implementation"
class CpuImage : public Image {
public:
    template<class T, int N> std::array<T, N> get(int x, int y) const {
        checkType<T>();
        assert(channels == N);
        std::array<T, N> result;
        get(x, y, reinterpret_cast<std::uint8_t*>(&result));
        return result;
    }

    template<class T, int N> void set(int x, int y, const std::array<T, N> &array) {
        checkType<T>();
        assert(channels == N);
        assert(x > 0 && y > 0 && x < width && y < height);
        set(x, y, reinterpret_cast<const std::uint8_t*>(&array));
    }

    template<class T> T get(int x, int y) const {
        return get<T, 1>(x, y).at(0);
    }

    template<class T> void set(int x, int y, T value) {
        set<T, 1>(x, y, { value });
    }

    static CpuImage &castFrom(Image &image) {
        assert(image.type == StorageType::CPU);
        return reinterpret_cast<CpuImage&>(image);
    }

    static std::unique_ptr<Image::Factory> createFactory();

protected:
    CpuImage(int w, int h, int channels, DataType dtype);

    virtual void get(int x, int y, std::uint8_t *targetArray) const = 0;
    virtual void set(int x, int y, const std::uint8_t *srcArray) = 0;
};
}
