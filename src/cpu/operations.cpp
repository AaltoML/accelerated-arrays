#include <cassert>
// #include <iostream>

#include "operations.hpp"
#include "image.hpp"

namespace accelerated {
namespace cpu {
namespace operations {
namespace {
typedef ::accelerated::Image BaseImage;
typedef ::accelerated::operations::fixedConvolution2D::Spec FixedConvolution2DSpec;
typedef ::accelerated::operations::fill::Spec FillSpec;
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

template <class T> SyncUnary fill(const FillSpec &spec) {
    return [spec](const Image &input, Image &output) {
        forEachPixelAndChannel(output, [&spec, &input](Image &output, int x, int y, int c) {
            output.set<T>(x, y, c, static_cast<T>(spec.value.at(c)));
        });
    };
}

template <class T> SyncUnary fixedConvolution2D(const FixedConvolution2DSpec &spec) {
    assert(!spec.kernel.empty());
    return [spec](const Image &input, Image &output) {
        const int kernelXOffset = spec.getKernelXOffset();
        const int kernelYOffset = spec.getKernelYOffset();
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

class CpuFactory : public Factory {
private:
    Processor &processor;

    Future processUnary(const SyncUnary &f, BaseImage &input, BaseImage &output) {
        auto &cpuInput = Image::castFrom(input);
        auto &cpuOutput = Image::castFrom(output);
        return processor.enqueue([f, &cpuInput, &cpuOutput] {
            f(cpuInput, cpuOutput);
        });
    }

    Function wrapChecked(const SyncUnary &f, const ImageTypeSpec &imageSpec) {
        checkSpec(imageSpec);
        return convert([f, imageSpec, this](BaseImage &input, BaseImage &output) -> Future {
            assert(input == imageSpec);
            assert(output == imageSpec);
            return processUnary(f, input, output);
        });
    }

public:
    CpuFactory(Processor &processor) : processor(processor) {}

    Function wrap(const SyncUnary &f) final {
        return convert([f, this](BaseImage &input, BaseImage &output) -> Future {
            return processUnary(f, input, output);
        });
    }

    Function create(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec) final {
        #define X(dtype) \
            if (imageSpec.dataType == ImageTypeSpec::getType<dtype>()) \
                return wrapChecked(fixedConvolution2D<dtype>(spec), imageSpec);
        ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
        #undef X
        assert(false);
    }

    Function create(const FillSpec &spec, const ImageTypeSpec &imageSpec) final {
        assert(int(spec.value.size()) == imageSpec.channels);
        #define X(dtype) \
            if (imageSpec.dataType == ImageTypeSpec::getType<dtype>()) \
                return wrapChecked(fill<dtype>(spec), imageSpec);
        ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
        #undef X
        assert(false);
    }
};
}

std::unique_ptr<Factory> createFactory(Processor &processor) {
    return std::unique_ptr<Factory>(new CpuFactory(processor));
}

}
}
}
