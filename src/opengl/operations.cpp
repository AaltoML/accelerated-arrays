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

template <class T> Nullary fill(const FillSpec &spec) {
    assert(!spec.value.empty());
    return [spec](Image &output) {

        // TODO: call GLSL kernel here

        (void)output;
        assert(false);
    };
}

template <class T> Unary fixedConvolution2D(const FixedConvolution2DSpec &spec) {
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

    Function wrapNAryChecked(const NAry &f, const ImageTypeSpec &spec) {
        checkSpec(spec);
        return ::accelerated::operations::sync::wrapChecked(f, processor, spec);
    }

    template <class T>
    Function wrapChecked(const T &f, const ImageTypeSpec &spec) {
        checkSpec(spec);
        return wrapNAryChecked(::accelerated::operations::sync::convert(f), spec);
    }

public:
    GpuFactory(Processor &processor) : processor(processor) {}

    Function wrapNAry(const NAry &f) final {
        return ::accelerated::operations::sync::wrap(f, processor);
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
