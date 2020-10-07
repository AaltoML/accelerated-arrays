#include <cassert>

#include "operations.hpp"
#include "image.hpp"

namespace accelerated {
namespace opengl {
namespace operations {
namespace {
typedef ::accelerated::Image BaseImage;
typedef ::accelerated::operations::fill::Spec FillSpec;
typedef ::accelerated::operations::fixedConvolution2D::Spec FixedConvolution2DSpec;
using ::accelerated::operations::Function;
using ::accelerated::operations::convert;

void checkSpec(const ImageTypeSpec &spec) {
    assert(Image::isCompatible(spec.storageType));
}

template <class T> SyncUnary fill(const FillSpec &spec) {
    assert(!spec.value.empty());
    return [spec](Image &input, Image &output) {

        // TODO: call GLSL kernel here

        (void)input;
        (void)output;
        assert(false);
    };
}

template <class T> SyncUnary fixedConvolution2D(const FixedConvolution2DSpec &spec) {
    assert(!spec.kernel.empty());
    return [spec](Image &input, Image &output) {

        // TODO: call GLSL kernel here

        (void)input;
        (void)output;
        assert(false);
    };
}

class GpuFactory : public Factory {
private:
    Processor &processor;

    // could be DRYed w.r.t. CpuFactory

    Future processUnary(const SyncUnary &f, BaseImage &input, BaseImage &output) {
        auto &gpuInput = Image::castFrom(input);
        auto &gpuOutput = Image::castFrom(output);
        return processor.enqueue([f, &gpuInput, &gpuOutput] {
            f(gpuInput, gpuOutput);
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
    GpuFactory(Processor &processor) : processor(processor) {}

    Function wrap(const SyncUnary &f) final {
        return convert([f, this](BaseImage &input, BaseImage &output) -> Future {
            return processUnary(f, input, output);
        });
    }

    Function create(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec) final {
        // TODO: enqueue the compilation of the GLSL kernel here

        #define X(dtype) \
            if (imageSpec.dataType == ImageTypeSpec::getType<dtype>()) \
                return wrapChecked(fixedConvolution2D<dtype>(spec), imageSpec);
        ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
        #undef X
        assert(false);
    }

    Function create(const FillSpec &spec, const ImageTypeSpec &imageSpec) final {

        // TODO: enqueue the compilation of the GLSL kernel here

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
    return std::unique_ptr<Factory>(new GpuFactory(processor));
}

}
}
}
