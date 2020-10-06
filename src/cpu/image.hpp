#pragma once

#include "../image.hpp"

#include <array>
#include <cassert>
#include <cstring>

namespace accelerated {
namespace cpu {
class Image : public ::accelerated::Image {
public:
    template<class T, std::size_t N> std::array<T, N> get(int x, int y) const {
        checkType<T>();
        assert(channels == N);
        std::array<T, N> result;
        get(x, y, reinterpret_cast<std::uint8_t*>(&result));
        return result;
    }

    template<class T, std::size_t N> void set(int x, int y, const std::array<T, N> &array) {
        checkType<T>();
        assert(channels == N);
        assert(x >= 0 && y >= 0 && x < width && y < height);
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
        assert(x >= 0 && y >= 0 && x < width && y < height);
        set(x, y, channel, reinterpret_cast<const std::uint8_t*>(&value));
    }

    template<class T> T get(int x, int y) const {
        return get<T, 1>(x, y).at(0);
    }

    template<class T> void set(int x, int y, T value) {
        set<T, 1>(x, y, { value });
    }

    template<class T, std::size_t N> std::array<T, N> get(int x, int y, Border border) const {
        if (!applyBorder(x, y, border)) {
            std::array<T, N> result;
            for (std::size_t i = 0; i < N; ++i) result.at(i) = 0;
            return result;
        }
        return get<T, N>(x, y);
    }

    template<class T> T get(int x, int y, int c, Border border) const {
        if (!applyBorder(x, y, border)) return 0;
        return get<T>(x, y, c);
    }

    template<class T> T get(int x, int y, Border border) const {
        if (!applyBorder(x, y, border)) return 0;
        return get<T>(x, y);
    }

    static Image &castFrom(::accelerated::Image &image) {
        assert(image.storageType == StorageType::CPU);
        return reinterpret_cast<Image&>(image);
    }

    static std::unique_ptr<Factory> createFactory(Processor &processor);

    template <class T> static ImageTypeSpec getSpec(int channels) {
        return getSpec(channels, ImageTypeSpec::getType<T>());
    }

    template <class T> static ImageTypeSpec getSpec() {
        return getSpec<T>(1);
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
