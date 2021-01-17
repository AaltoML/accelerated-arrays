#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "future.hpp"
#include "fixed_point.hpp"
#include "assert.hpp"

namespace accelerated {
struct ImageTypeSpec {
    /**
     * Number of "channels" per pixel. Possible values range from 1 to 4.
     * May be interpreted freely by the user, but often labeled as R,G,B,A
     * in a 4-channel image.
     */
    const int channels;

    /** Channel data type */
    const enum class DataType {
        UINT8,
        SINT8,
        // TODO: should specify big vs little endian
        UINT16,
        SINT16,
        UINT32,
        SINT32,
        FLOAT32,
        UFIXED8,
        SFIXED8,
        UFIXED16,
        SFIXED16,
        UFIXED32,
        SFIXED32
    } dataType;

    /**
     * Where is the image stored
     */
    const enum class StorageType {
        CPU,
        GPU_OPENGL,
        GPU_OPENGL_EXTERNAL
    } storageType;

    std::size_t bytesPerChannel() const;

    inline std::size_t bytesPerPixel() const { return bytesPerChannel() * channels; }

    template <class T> bool isType() const;
    template <class T> void checkType() const;
    template <class T> static DataType getType();

    inline bool operator==(const ImageTypeSpec &other) const {
        return channels == other.channels &&
            dataType == other.dataType &&
            storageType == other.storageType;
    }

    inline bool operator!=(const ImageTypeSpec &other) const {
        return !(*this == other);
    }

    static bool isIntegerType(DataType dtype);
    static bool isSigned(DataType dtype);
    static bool isFixedPoint(DataType dtype);
    static bool isFloat(DataType dtype);
};

/**
 * An abstraction for images that may be processed on multiple types of
 * hardware: both CPUs or GPUs. They represent image-like arrays but do not
 * offer direct pixel level access.
 */
struct Image : ImageTypeSpec {
    /**
     * Fixed 2D image dimensions whose product is the number of pixels
     */
    const int width, height;

    virtual ~Image();

    Image(int w, int h, const ImageTypeSpec &spec) :
        ImageTypeSpec(spec),
        width(w),
        height(h)
    {};

    inline std::size_t numberOfPixels() const { return width * height; }
    inline std::size_t numberOfScalars() const { return numberOfPixels() * channels; }

    inline std::size_t size() const { return numberOfPixels() * bytesPerPixel(); }

    // border behavior
    enum class Border {
        UNDEFINED, // do not allow out-of-bounds reads
        ZERO,
        REPEAT,
        MIRROR,
        CLAMP
    };

    // potentially supported interpolation types
    enum class Interpolation {
        UNDEFINED, // whatever is currently set / don't care
        NEAREST,
        LINEAR
    };

    class Factory {
    public:
        virtual ~Factory();
        template <class T, int Channels> std::unique_ptr<Image> create(int w, int h);
        template <class T, int Channels> ImageTypeSpec getSpec();
        /**
         * Create a new image with the same ImageTypeSpec as the given
         * image, except for the StorageType, which is specific to this
         * factory.
         */
        std::unique_ptr<Image> createLike(const Image &image);
        virtual std::unique_ptr<Image> create(int w, int h, int channels, DataType dtype) = 0;
        virtual ImageTypeSpec getSpec(int channels, DataType dtype) = 0;
    };

    typedef std::function< Future(std::vector<Image*> &inputs, Image &output) > Function;

    /** Asynchronous read operation */
    virtual Future readRaw(std::uint8_t *outputData) = 0;
    /** Asyncronous write operation */
    virtual Future writeRaw(const std::uint8_t *inputData) = 0;

    /**
     * Create a Region-of-Interest, a reference to a region in this image.
     * All image operations may currently not be supported for ROIs in all
     * implementations (OpenGL vs CPU).
     */
    virtual std::unique_ptr<Image> createROI(int x0, int y0, int width, int height) = 0;

    // add some type safety wrappers
    template <class T> Future read(T *outputData) {
        aa_assert(isType<T>());
        return readRaw(reinterpret_cast<std::uint8_t*>(outputData));
    }

    template <class T> Future write(const T *inputData) {
        aa_assert(isType<T>());
        return writeRaw(reinterpret_cast<const std::uint8_t*>(inputData));
    }

    template <class T> inline Future read(std::vector<T> &output) {
        output.resize(numberOfScalars());
        return read<T>(output.data());
    }

    template <class T> inline Future write(const std::vector<T> &input) {
        aa_assert(input.size() == numberOfScalars());
        return write<T>(input.data());
    }

    template <class T> Future readRawFixedPoint(std::vector<T> &output) {
        return read(reinterpret_cast<std::vector<FixedPoint<T>>&>(output));
    }

    template <class T> Future writeRawFixedPoint(const std::vector<T> &input) {
        return write(reinterpret_cast<const std::vector<FixedPoint<T>>&>(input));
    }
};

#define ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_TYPE(x) \
    x(std::uint8_t) \
    x(std::int8_t) \
    x(std::uint16_t) \
    x(std::int16_t) \
    x(std::uint32_t) \
    x(std::int32_t) \
    x(FixedPoint<std::uint8_t>) \
    x(FixedPoint<std::int8_t>) \
    x(FixedPoint<std::uint16_t>) \
    x(FixedPoint<std::int16_t>) \
    x(FixedPoint<std::uint32_t>) \
    x(FixedPoint<std::int32_t>)

#define ACCELERATED_IMAGE_FOR_EACH_TYPE(x) \
    ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_TYPE(x) \
    x(float)

#define ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, extra) \
    x(std::uint8_t, extra) \
    x(std::int8_t, extra) \
    x(std::uint16_t, extra) \
    x(std::int16_t, extra) \
    x(std::uint32_t, extra) \
    x(std::int32_t, extra) \
    x(float, extra) \
    x(FixedPoint<std::uint8_t>, extra) \
    x(FixedPoint<std::int8_t>, extra) \
    x(FixedPoint<std::uint16_t>, extra) \
    x(FixedPoint<std::int16_t>, extra) \
    x(FixedPoint<std::uint32_t>, extra) \
    x(FixedPoint<std::int32_t>, extra)

// quite heavy, use sparingly
#define ACCELERATED_IMAGE_FOR_EACH_TYPE_PAIR(x) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, std::uint8_t) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, std::int8_t) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, std::uint16_t) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, std::int16_t) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, std::uint32_t) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, std::int32_t) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, float) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, FixedPoint<std::uint8_t>) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, FixedPoint<std::int8_t>) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, FixedPoint<std::uint16_t>) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, FixedPoint<std::int16_t>) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, FixedPoint<std::uint32_t>) \
    ACCELERATED_IMAGE_FOR_EACH_TYPE_WITH_EXTRAS(x, FixedPoint<std::int32_t>)

#define ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_NAMED_TYPE(x) \
    x(std::uint8_t, ImageTypeSpec::DataType::UINT8) \
    x(std::int8_t, ImageTypeSpec::DataType::SINT8) \
    x(std::uint16_t, ImageTypeSpec::DataType::UINT16) \
    x(std::int16_t, ImageTypeSpec::DataType::SINT16) \
    x(std::uint32_t, ImageTypeSpec::DataType::UINT32) \
    x(std::int32_t, ImageTypeSpec::DataType::SINT32) \
    x(FixedPoint<std::uint8_t>, ImageTypeSpec::DataType::UFIXED8) \
    x(FixedPoint<std::int8_t>, ImageTypeSpec::DataType::SFIXED8) \
    x(FixedPoint<std::uint16_t>, ImageTypeSpec::DataType::UFIXED16) \
    x(FixedPoint<std::int16_t>, ImageTypeSpec::DataType::SFIXED16) \
    x(FixedPoint<std::uint32_t>, ImageTypeSpec::DataType::UFIXED32) \
    x(FixedPoint<std::int32_t>, ImageTypeSpec::DataType::SFIXED32)

#define ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(x) \
    ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_NAMED_TYPE(x) \
    x(float, ImageTypeSpec::DataType::FLOAT32)

#define Y(dtype, n) \
    template <> std::unique_ptr<Image> Image::Factory::create<dtype, n>(int w, int h); \
    template <> ImageTypeSpec Image::Factory::getSpec<dtype, n>();
#define X(dtype) \
    template <> void ImageTypeSpec::checkType<dtype>() const; \
    template <> bool ImageTypeSpec::isType<dtype>() const; \
    template <> ImageTypeSpec::DataType ImageTypeSpec::getType<dtype>(); \
    Y(dtype, 1) \
    Y(dtype, 2) \
    Y(dtype, 3) \
    Y(dtype, 4)
ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
#undef X
#undef Y
}
