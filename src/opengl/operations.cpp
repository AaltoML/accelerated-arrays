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

Binder::Target &bindImage(GlslPipeline &pipeline, unsigned slot, Image &image) {
    return pipeline.bindTexture(slot, image.getTextureId(), image.width, image.height);
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

Shader<Nullary>::Builder fill(const FillSpec &spec, const ImageTypeSpec &imageSpec) {
    assert(!spec.value.empty());

    // GLSL shader source is built in syncrhonously on the calling thread
    // to minimize unnecessary stuff on the GL thread, even though this
    // would not matter so much since this operation is quite lightweight
    std::string fragmentShaderBody;
    {
        std::ostringstream oss;
        oss << "void main() {\n";
        oss << "gl_FragColor." << glsl::swizzleSubset(imageSpec.channels) << " = ";
        oss << glsl::convertToFloatOutputValue(glsl::wrapToVec(spec.value), imageSpec.dataType) << ";\n";
        oss << "}\n";
        fragmentShaderBody = oss.str();
    }

    return [fragmentShaderBody]() {
        // GLSL compilation happens in the GL thread
        std::unique_ptr< Shader<Nullary> > shader(new Shader<Nullary>);
        shader->resources = GlslPipeline::createWithoutTexCoords(fragmentShaderBody.c_str());
        GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

        shader->function = [&pipeline](Image &output) {
            // GLSL shader invocation also happens in the GL thread
            Binder binder(pipeline);
            pipeline.call(output.getFrameBuffer());
        };

        return shader;
    };
}

Shader<Unary>::Builder fixedConvolution2D(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec) {
    assert(!spec.kernel.empty());

    std::string fragmentShaderBody;
    {
        std::ostringstream oss;

        const int kernelH = spec.kernel.size();
        const int kernelW = spec.kernel.at(0).size();

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

        // NOTE: this is a recurring problem in many kernels, could make a
        // suitable helper

        // texCoord = (ix + 0.5) / width_out
        // => ix = texCoord*width_out - 0.5
        // targetTexCoord = (ix * xStride + jx + xOffs + 0.5) / width_in
        //  = ((texCoord*width_out - 0.5) * xStride + jx + xOffs + 0.5) / width_in
        //  = ((width_out*xStride) * texCoord  + jx + xOffs + 0.5 * (1 - xStride)) / width_in
        // = (alpha * texCoord + jx + pixelOffset) / width_in

        const float xOffs = spec.getKernelXOffset() + 0.5 * (1 - spec.xStride);
        const float yOffs = spec.getKernelYOffset() + 0.5 * (1 - spec.yStride);

        oss << "const vec2 stride = vec2(" << spec.xStride << ", " << spec.yStride << ");\n";
        oss << "const vec2 pixelOffset = vec2(" << xOffs << ", " << yOffs << ");\n";

        oss << "void main() {\n";
        oss << "vec2 alpha = stride * u_outSize;\n";
        oss << vtype << " v = " << vtype << "(0);\n";
        oss << "for (int i = 0; i < KERNEL_H; i++) {\n";
        oss << "for (int j = 0; j < KERNEL_W; j++) {\n";
        oss << "    float k = kernel[i * KERNEL_W + j];\n";
        oss << "    vec2 coord = (alpha * v_texCoord + (vec2(float(j), float(i)) + pixelOffset)) / u_textureSize;\n";
        oss << "    v += k * texture2D(u_texture, coord)." << swiz << ";\n";
        oss << "}\n";
        oss << "}\n";
        oss << "gl_FragColor." << swiz << " = ";
        oss << glsl::convertToFloatOutputValue("v", imageSpec.dataType) << ";\n";
        oss << "}\n";

        fragmentShaderBody = oss.str();
    }

    return [fragmentShaderBody]() {
        std::unique_ptr< Shader<Unary> > shader(new Shader<Unary>);
        shader->resources = GlslPipeline::create(1, fragmentShaderBody.c_str());
        GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

        shader->function = [&pipeline](Image &input, Image &output) {
            Binder binder(pipeline);
            Binder inputBinder(bindImage(pipeline, 0, input));
            pipeline.call(output.getFrameBuffer());
        };

        return shader;
    };
}

class GpuFactory : public Factory {
private:
    Processor &processor;

    class ShaderWrapper {
    private:
        typedef Shader<NAry> S;
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

        NAry &get() {
            // should never be called at the same time with other actions
            std::shared_ptr<S> tmp = std::atomic_load(&shader);
            assert(tmp && tmp->function);
            return tmp->function;
        }
    };

public:
    GpuFactory(Processor &processor) : processor(processor) {}

    Function wrapNAry(const Shader<NAry>::Builder &builder) final {
        std::shared_ptr<ShaderWrapper> wrapper(new ShaderWrapper(processor));
        processor.enqueue([builder, wrapper]() { wrapper->initialize(builder()); });
        return ::accelerated::operations::sync::wrap<Image>([wrapper](Image **inputs, int nInputs, Image &output) {
            wrapper->get()(inputs, nInputs, output);
        }, processor);
    }

    Function wrapNAryChecked(const Shader<NAry>::Builder &builder, const ImageTypeSpec &spec) final {
        checkSpec(spec);
        std::shared_ptr<ShaderWrapper> wrapper(new ShaderWrapper(processor));
        processor.enqueue([builder, wrapper]() { wrapper->initialize(builder()); });
        return ::accelerated::operations::sync::wrapChecked<Image, ImageTypeSpec>([wrapper](Image **inputs, int nInputs, Image &output) {
            wrapper->get()(inputs, nInputs, output);
        }, processor, spec);
    }

    Function create(const FixedConvolution2DSpec &spec, const ImageTypeSpec &imageSpec) final {
        return wrapChecked<Unary>(fixedConvolution2D(spec, imageSpec), imageSpec);
    }

    Function create(const FillSpec &spec, const ImageTypeSpec &imageSpec) final {
        return wrapChecked<Nullary>(fill(spec, imageSpec), imageSpec);
    }
};
}

std::unique_ptr<Factory> createFactory(Processor &processor) {
    return std::unique_ptr<Factory>(new GpuFactory(processor));
}

}
}
}
