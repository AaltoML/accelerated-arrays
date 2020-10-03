#include <cassert>
#include <future>
// #include <iostream>

#include "cpu_ops.hpp"
#include "cpu_image.hpp"

namespace accelerated {
namespace operations {
namespace {

void checkSpec(const ImageTypeSpec &spec) {
    assert(spec.storageType == ImageTypeSpec::StorageType::CPU);
}

/*void forEachPixel(CpuImage &img, const std::function<void(CpuImage &img, int x, int y)> &f) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            f(img, x, y);
        }
    }
}*/

void forEachPixelAndChannel(CpuImage &img, const std::function<void(CpuImage &img, int x, int y, int c)> &f) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            for (int c = 0; c < img.channels; ++c) {
                f(img, x, y, c);
            }
        }
    }
}

template <class T> Cpu::SyncUnary fixedConvolution2D(const FixedConvolution2D::Spec &spec) {
    assert(!spec.kernel.empty());
    return [spec](const CpuImage &input, CpuImage &output) {
        const int kernelYOffset = -(spec.kernel.size() / 2) + spec.yOffset;
        const int kernelXOffset = -(spec.kernel.at(0).size() / 2) + spec.xOffset;
        // std::cout << spec.kernel.size() << " " << spec.kernel.at(0).size() << std::endl;
        forEachPixelAndChannel(output, [kernelYOffset, kernelXOffset, &spec, &input](CpuImage &output, int x, int y, int c) {
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

Cpu::SyncUnary fixedConvolution2D(const FixedConvolution2D::Spec &spec, ImageTypeSpec::DataType dataType) {
    #define X(dtype) if (dataType == ImageTypeSpec::getType<dtype>()) return fixedConvolution2D<dtype>(spec);
    ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
    #undef X
    assert(false);
}

Function convertChecked(const Cpu::SyncUnary &f, const ImageTypeSpec &imageSpec, std::promise<void> &p) {
    checkSpec(imageSpec);
    return convert([f, &p, imageSpec](Image &input, Image &output) -> std::future<void> {
        p = {};
        p.set_value();
        assert(input == imageSpec);
        assert(output == imageSpec);
        f(CpuImage::castFrom(input), CpuImage::castFrom(output));
        return p.get_future();
    });
}

class SyncCpuImplementation : public Cpu {
private:
    // not optimal
    std::vector< std::promise<void> > callPromises;
    std::vector< std::promise<Function> > funcPromises;
public:
    std::future<Function> create(const FixedConvolution2D::Spec &spec, const ImageTypeSpec &imageSpec) final {
        callPromises.push_back({});
        funcPromises.push_back({});
        funcPromises.back().set_value(convertChecked(fixedConvolution2D(spec, imageSpec.dataType), imageSpec, callPromises.back()));
        return funcPromises.back().get_future();
    }
};
}

std::unique_ptr<Cpu> Cpu::createFactory() {
    return std::unique_ptr<Cpu>(new SyncCpuImplementation);
}

}
}
