#include <cassert>
#include <mutex>

#include "adapters.hpp"
#include "operations.hpp"
#include "image.hpp"

namespace accelerated {
namespace opengl {
namespace operations {
namespace {
typedef ::accelerated::operations::fill::Spec FillSpec;
typedef ::accelerated::operations::fixedConvolution2D::Spec FixedConvolution2DSpec;
using ::accelerated::operations::Function;

void checkSpec(const ImageTypeSpec &spec) {
    assert(Image::isCompatible(spec.storageType));
}

struct Fill  {
    FillSpec spec;
    ImageTypeSpec imageSpec;

    Fill(const FillSpec &spec, const ImageTypeSpec &imageSpec)
    : spec(spec), imageSpec(imageSpec)
    {}

    std::unique_ptr<GlslPipeline> buildShader() const {
        return GlslPipeline::create(0, "foobar");
    }

    Nullary buildCaller() const {
        assert(!spec.value.empty());
        return [this](GlslPipeline &pipeline, Image &output) {
            Binder binder(pipeline);
            Binder frameBufferBinder(output.getFrameBuffer());

            // TODO: glViewport

            pipeline.call();
        };
    }
};

struct FixedConvolution2D  {
    FixedConvolution2DSpec spec;
    ImageTypeSpec imageSpec;

    FixedConvolution2D(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec)
    : spec(spec), imageSpec(imageSpec)
    {}

    std::unique_ptr<GlslPipeline> buildShader() const {
        return GlslPipeline::create(1, "foobar");
    }

    Unary buildCaller() const {
        assert(!spec.kernel.empty());
        return [this](GlslPipeline &pipeline, Image &input, Image &output) {
            Binder binder(pipeline);
            Binder inputBinder(pipeline.bindTexture(0, input.getTextureId()));
            Binder frameBufferBinder(output.getFrameBuffer());

            // TODO: glViewport

            pipeline.call();
        };
    }
};

class GpuFactory : public Factory {
private:
    Processor &processor;

    class Shader {
    private:
        Processor &processor;
        std::mutex mutex;
        std::shared_ptr<GlslPipeline> shader;

    public:
        Shader(Processor &processor) : processor(processor) {}

        void initialize(std::unique_ptr<GlslPipeline> s) {
            std::lock_guard<std::mutex> lock(mutex);
            shader = std::move(s);
        }

        ~Shader() {
            std::shared_ptr<GlslPipeline> tmp;
            {
                std::lock_guard<std::mutex> lock(mutex);
                tmp = shader;
            }

            processor.enqueue([tmp]() { tmp->destroy(); });
        }

        GlslPipeline &get() {
            // should not never called at the same time with other actions
            assert(shader);
            return *shader;
        }
    };

    Function wrapNAryChecked(const ShaderBuilder &builder, const NAry &f, const ImageTypeSpec &spec) {
        checkSpec(spec);
        std::shared_ptr<Shader> shader(new Shader(processor));
        processor.enqueue([builder, shader]() { shader->initialize(builder()); });
        return ::accelerated::operations::sync::wrapChecked<Image, ImageTypeSpec>([shader, f](Image **inputs, int nInputs, Image &output) {
            f(shader->get(), inputs, nInputs, output);
        }, processor, spec);
    }

    template <class T>
    Function wrapChecked(const ShaderBuilder &builder, const T &f, const ImageTypeSpec &spec) {
        return wrapNAryChecked(builder, convert(f), spec);
    }

public:
    GpuFactory(Processor &processor) : processor(processor) {}

    Function wrapNAry(const ShaderBuilder &builder, const NAry &f) final {
        std::shared_ptr<Shader> shader(new Shader(processor));
        processor.enqueue([builder, shader]() { shader->initialize(builder()); });
        return ::accelerated::operations::sync::wrap<Image>([shader, f](Image **inputs, int nInputs, Image &output) {
            f(shader->get(), inputs, nInputs, output);
        }, processor);
    }

    Function create(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec) final {
        FixedConvolution2D convolution(spec, imageSpec);
        return wrapChecked(
            [convolution]() { return convolution.buildShader(); },
            convolution.buildCaller(),
            imageSpec);
    }

    Function create(const FillSpec &spec, const ImageTypeSpec &imageSpec) final {
        Fill fill(spec, imageSpec);
        return wrapChecked(
            [fill]() { return fill.buildShader(); },
            fill.buildCaller(),
            imageSpec);
    }
};
}

NAry convert(const Nullary &f) {
    return [f](GlslPipeline &shader, Image** inputs, int nInputs, Image &output) {
        assert(nInputs == 0);
        (void)inputs;
        f(shader, output);
    };
}

NAry convert(const Unary &f) {
    return [f](GlslPipeline &shader, Image** inputs, int nInputs, Image &output) {
        assert(nInputs == 1);
        f(shader, **inputs, output);
    };
}

NAry convert(const Binary &f) {
    return [f](GlslPipeline &shader, Image** inputs, int nInputs, Image &output) {
        assert(nInputs == 2);
        f(shader, *inputs[0], *inputs[1], output);
    };
}

std::unique_ptr<Factory> createFactory(Processor &processor) {
    return std::unique_ptr<Factory>(new GpuFactory(processor));
}

}
}
}
