#include <atomic>
#include <cassert>
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

std::string valueType(int channels) {
    if (channels == 0) return "float";
    std::ostringstream oss;
    oss << "vec" << channels;
    return oss.str();
}

std::string swizzleSubset(std::size_t n) {
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

std::unique_ptr< Shader<sync::Nullary> > fill(const FillSpec &spec, const ImageTypeSpec &imageSpec) {
    assert(!spec.value.empty());

    std::string fragmentShaderSource;
    {
        std::ostringstream oss;
        oss << "void main() {\n";
        oss << "gl_FragColor." << glsl::swizzleSubset(imageSpec.channels) << " = ";
        oss << glsl::convertToFloatOutputValue(glsl::wrapToVec(spec.value), imageSpec.dataType) << ";\n";
        oss << "}\n";
        fragmentShaderSource = oss.str();
    }

    std::unique_ptr< Shader<sync::Nullary> > shader(new Shader<sync::Nullary>);
    shader->resources = GlslPipeline::createWithoutTexCoords(fragmentShaderSource.c_str());
    GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

    shader->function = [&pipeline](Image &output) {
        Binder binder(pipeline);
        auto &fbo = output.getFrameBuffer();
        Binder frameBufferBinder(fbo);
        fbo.setViewport();
        pipeline.call();
    };

    return shader;
}

std::unique_ptr< Shader<sync::Unary> > fixedConvolution2D(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec) {
    assert(!spec.kernel.empty());

    std::string fragmentShaderSource;
    {
        std::ostringstream oss;

        const int kernelH = spec.kernel.size();
        const int kernelW = spec.kernel.at(0).size();

        oss << "uniform vec2 pixelDelta;\n";

        oss << "#define KERNEL_H " << kernelH << "\n";
        oss << "#define KERNEL_W " << kernelW << "\n";
        oss << "#define KERNEL_SZ " << (kernelH * kernelW)  << "\n";
        oss << "const float kernel[KERNEL_SZ] = float[KERNEL_SZ](\n";
        for (int i = 0; i < kernelH; ++i) {
            if (i > 0) oss << ",\n";
            for (int j = 0; j < kernelW; ++j) {
                if (j > 0) oss << ", ";
                oss << spec.kernel.at(i).at(j);
            }
        }
        oss << "\n);\n";

        const auto vtype = glsl::valueType(imageSpec.channels);
        const auto swiz = glsl::swizzleSubset(imageSpec.channels);

        assert(spec.xStride == 1 && spec.yStride == 1); // TODO
        oss << "const vec2 beta = vec2(" << spec.getKernelXOffset() << ", " << spec.getKernelYOffset() << ");\n";

        oss << "void main() {\n";
        oss << vtype << " v = " << vtype << "(0);\n";
        oss << "for (int i = 0; i < KERNEL_H; i++) {\n";
        oss << "for (int j = 0; j < KERNEL_W; j++) {\n";
        oss << "    float k = kernel[i * KERNEL_W + j];\n";
        oss << "    vec2 coord = v_texCoord + (vec2(float(j), float(i)) + beta) * pixelDelta;\n";
        oss << "    v += k * texture2D(u_texture, coord)." << swiz << ";\n";
        oss << "}\n";
        oss << "}\n";
        oss << "gl_FragColor." << swiz << " = ";
        oss << glsl::convertToFloatOutputValue("v", imageSpec.dataType) << ";\n";
        oss << "}\n";

        fragmentShaderSource = oss.str();
    }

    std::unique_ptr< Shader<sync::Unary> > shader(new Shader<sync::Unary>);
    shader->resources = GlslPipeline::create(1, fragmentShaderSource.c_str());
    GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

    shader->function = [&pipeline](Image &input, Image &output) {
        Binder binder(pipeline);
        Binder inputBinder(pipeline.bindTexture(0, input.getTextureId()));
        auto &fbo = output.getFrameBuffer();
        Binder frameBufferBinder(fbo);

        // TODO: set uniform

        fbo.setViewport();
        pipeline.call();
    };

    return shader;
}

class GpuFactory : public Factory {
private:
    Processor &processor;

    class ShaderWrapper {
    private:
        typedef Shader<sync::NAry> S;
        Processor &processor;
        std::shared_ptr< S > shader;

    public:
        ShaderWrapper(Processor &processor) : processor(processor) {}

        void initialize(std::unique_ptr<S> s) {
            std::shared_ptr<S> tmp = std::move(s);
            std::atomic_store(&shader, tmp);
        }

        ~ShaderWrapper() {
            std::shared_ptr<S> tmp = std::atomic_load(&shader);
            if (tmp) {
                processor.enqueue([tmp]() {
                    if (tmp->resources) tmp->resources->destroy();
                    tmp->resources.reset();
                });
            }
        }

        sync::NAry &get() {
            // should never be called at the same time with other actions
            std::shared_ptr<S> tmp = std::atomic_load(&shader);
            assert(tmp && tmp->function);
            return tmp->function;
        }
    };

public:
    GpuFactory(Processor &processor) : processor(processor) {}

    Function wrapNAry(const Shader<sync::NAry>::Builder &builder) final {
        std::shared_ptr<ShaderWrapper> wrapper(new ShaderWrapper(processor));
        processor.enqueue([builder, wrapper]() { wrapper->initialize(builder()); });
        return ::accelerated::operations::sync::wrap<Image>([wrapper](Image **inputs, int nInputs, Image &output) {
            wrapper->get()(inputs, nInputs, output);
        }, processor);
    }

    Function wrapNAryChecked(const Shader<sync::NAry>::Builder &builder, const ImageTypeSpec &spec) final {
        checkSpec(spec);
        std::shared_ptr<ShaderWrapper> wrapper(new ShaderWrapper(processor));
        processor.enqueue([builder, wrapper]() { wrapper->initialize(builder()); });
        return ::accelerated::operations::sync::wrapChecked<Image, ImageTypeSpec>([wrapper](Image **inputs, int nInputs, Image &output) {
            wrapper->get()(inputs, nInputs, output);
        }, processor, spec);
    }

    Function create(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec) final {
        return wrapChecked<sync::Unary>([spec, imageSpec]() { return fixedConvolution2D(spec, imageSpec); }, imageSpec);
    }

    Function create(const FillSpec &spec, const ImageTypeSpec &imageSpec) final {
        return wrapChecked<sync::Nullary>([spec, imageSpec]() { return fill(spec, imageSpec); }, imageSpec);
    }
};
}

std::unique_ptr<Factory> createFactory(Processor &processor) {
    return std::unique_ptr<Factory>(new GpuFactory(processor));
}

}
}
}
