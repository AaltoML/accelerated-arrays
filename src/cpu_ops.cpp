#include <cassert>
// #include <iostream>

#include "cpu_ops.hpp"
#include "cpu_image.hpp"

namespace accelerated {
namespace cpu {
namespace operations {
namespace {
typedef ::accelerated::Image BaseImage;
typedef ::accelerated::operations::fixedConvolution2D::Spec FixedConvolution2DSpec;
using ::accelerated::operations::Function;
using ::accelerated::operations::convert;

void checkSpec(const ImageTypeSpec &spec) {
    assert(spec.storageType == ImageTypeSpec::StorageType::CPU);
}

/*void forEachPixel(Image &img, const std::function<void(Image &img, int x, int y)> &f) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            f(img, x, y);
        }
    }
}*/

void forEachPixelAndChannel(Image &img, const std::function<void(Image &img, int x, int y, int c)> &f) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            for (int c = 0; c < img.channels; ++c) {
                f(img, x, y, c);
            }
        }
    }
}

template <class T> SyncUnary fixedConvolution2D(const FixedConvolution2DSpec &spec) {
    assert(!spec.kernel.empty());
    return [spec](const Image &input, Image &output) {
        const int kernelYOffset = -(spec.kernel.size() / 2) + spec.yOffset;
        const int kernelXOffset = -(spec.kernel.at(0).size() / 2) + spec.xOffset;
        // std::cout << spec.kernel.size() << " " << spec.kernel.at(0).size() << std::endl;
        forEachPixelAndChannel(output, [kernelYOffset, kernelXOffset, &spec, &input](Image &output, int x, int y, int c) {
            double v = spec.bias;
            for (int i = 0; i < int(spec.kernel.size()); ++i) {
                const int y1 = y * spec.yStride + i + kernelYOffset;
                const auto &krow = spec.kernel.at(i);
                for (int j = 0; j < int(krow.size()); ++j) {
                    const int x1 = x * spec.xStride + j + kernelXOffset;
                    v += input.get<T>(x1, y1, c, spec.border) * krow.at(j);
                    // std::cout << " x:" << x << " y:" << y << " c:" << c << " i:" << i << " j:"  << j
                    // << " x1:" << x1 << " y1:" << y1 << " : " << input.get<T>(x1, y1, c, spec.border) << " * " <<  krow.at(j) << std::endl;
                 }
            }
            // std::cout << x << " " << y << " " << c << " = " << v << std::endl;
            output.set<T>(x, y, c, static_cast<T>(v));
        });
    };
}

SyncUnary fixedConvolution2D(const FixedConvolution2DSpec &spec, ImageTypeSpec::DataType dataType) {
    #define X(dtype) if (dataType == ImageTypeSpec::getType<dtype>()) return fixedConvolution2D<dtype>(spec);
    ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
    #undef X
    assert(false);
}

Function convertChecked(const SyncUnary &f, const ImageTypeSpec &imageSpec) {
    checkSpec(imageSpec);
    return convert([f, imageSpec](BaseImage &input, BaseImage &output) -> Future {
        assert(input == imageSpec);
        assert(output == imageSpec);
        f(Image::castFrom(input), Image::castFrom(output));
        return Future::instantlyResolved();
    });
}

class SyncCpuFactory : public Factory {
public:
    Function create(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec) final {
        return convertChecked(fixedConvolution2D(spec, imageSpec.dataType), imageSpec);
    }
};
}

std::unique_ptr<Factory> createFactory() {
    return std::unique_ptr<Factory>(new SyncCpuFactory);
}

}
}
}
