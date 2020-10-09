#include <cassert>
#include <mutex>
#include <sstream>

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

double maxDataTypeValue(ImageTypeSpec::DataType dtype) {
    // NOTE: using floats as-is, and not squeezing to [0, 1]
    if (dtype == ImageTypeSpec::DataType::FLOAT32) return 1.0;
    return ImageTypeSpec::maxValueOf(dtype);
}

double minDataTypeValue(ImageTypeSpec::DataType dtype) {
    if (dtype == ImageTypeSpec::DataType::FLOAT32) return 0.0;
    return ImageTypeSpec::minValueOf(dtype);
}

namespace glsl {
template <class T> std::string wrapToVec(const std::vector<T> &values) {
    assert(!values.empty() && values.size() <= 4);
    std::ostringstream oss;
    if (values.size() == 1) {
        oss << values.at(0);
        return oss.str();
    }
    oss << "vec";
    oss << values.size();
    oss << "(";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ",";
        oss << values.at(i);
    }
    oss << ")";
    return oss.str();
}

std::string sizzleSubset(std::size_t n) {
    assert(n > 0 && n <= 4);
    return std::string("rgba").substr(0, n);
}

std::string convertToFloatOutputValue(const std::string &value, ImageTypeSpec::DataType dtype) {
    double maxValue = maxDataTypeValue(dtype);
    double minValue = minDataTypeValue(dtype);
    std::ostringstream oss;
    oss << "(";
    if (minValue != 0.0) {
        oss << "(" << value << ") + (" << minValue << ")";
    } else {
        oss << value;
    }
    oss  << ") * " << (1.0 / (maxValue - minValue));
    return oss.str();
}
}

struct Fill  {
private:
    FillSpec spec;
    ImageTypeSpec imageSpec;

    std::string fragmentShaderSource() const {
        std::ostringstream oss;
        oss << "void main() {\n";
        oss << "gl_FragColor." << glsl::sizzleSubset(imageSpec.channels) << " = ";
        oss << glsl::convertToFloatOutputValue(glsl::wrapToVec(spec.value), imageSpec.dataType) << ";\n";
        oss << "}\n";
        return oss.str();
    }

public:
    Fill(const FillSpec &spec, const ImageTypeSpec &imageSpec)
    : spec(spec), imageSpec(imageSpec)
    {}

    std::unique_ptr<GlslPipeline> buildShader() const {
        return GlslPipeline::createWithoutTexCoords(fragmentShaderSource().c_str());
    }

    Nullary buildCaller() const {
        assert(!spec.value.empty());
        return [this](GlslPipeline &pipeline, Image &output) {
            Binder binder(pipeline);
            auto &fbo = output.getFrameBuffer();
            Binder frameBufferBinder(fbo);
            fbo.setViewport();
            pipeline.call();
        };
    }
};

struct FixedConvolution2D  {
    FixedConvolution2DSpec spec;
    ImageTypeSpec imageSpec;

    std::string fragmentShaderSource() const {
        std::ostringstream oss;
        oss << "void main() {\n";
        oss << "gl_FragColor." << glsl::sizzleSubset(imageSpec.channels) << " = ";
        oss << glsl::convertToFloatOutputValue(glsl::wrapToVec(spec.kernel.at(0)), imageSpec.dataType) << ";\n";
        oss << "}\n";
        return oss.str();
    }

    FixedConvolution2D(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec)
    : spec(spec), imageSpec(imageSpec)
    {}

    std::unique_ptr<GlslPipeline> buildShader() const {
        return GlslPipeline::create(1, fragmentShaderSource().c_str());
    }

    Unary buildCaller() const {
        assert(!spec.kernel.empty());
        return [this](GlslPipeline &pipeline, Image &input, Image &output) {
            Binder binder(pipeline);
            Binder inputBinder(pipeline.bindTexture(0, input.getTextureId()));
            auto &fbo = output.getFrameBuffer();
            Binder frameBufferBinder(fbo);
            fbo.setViewport();
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
