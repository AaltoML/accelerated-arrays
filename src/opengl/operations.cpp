#include <atomic>
#include <cmath>
#include <sstream>

#include "adapters.hpp"
#include "operations.hpp"
#include "image.hpp"
#include "glsl_helpers.hpp"

namespace accelerated {
namespace opengl {
namespace operations {
namespace {
typedef ::accelerated::operations::fill::Spec FillSpec;
typedef ::accelerated::operations::fixedConvolution2D::Spec FixedConvolution2DSpec;
typedef ::accelerated::operations::pixelwiseAffineCombination::Spec PixelwiseAffineCombinationSpec;
typedef ::accelerated::operations::channelwiseAffine::Spec ChannelwiseAffineSpec;
using ::accelerated::operations::Function;

void checkSpec(const ImageTypeSpec &spec) {
    aa_assert(Image::isCompatible(spec.storageType));
}

namespace impl {
Shader<Nullary>::Builder fill(const FillSpec &spec, const ImageTypeSpec &imageSpec) {
    aa_assert(!spec.value.empty());

    // GLSL shader source is built in syncrhonously on the calling thread
    // to minimize unnecessary stuff on the GL thread, even though this
    // would not matter so much since this operation is quite lightweight
    std::string fragmentShaderBody;
    {
        std::ostringstream oss;
        oss << "void main() {\n";
        oss << "outValue = " << glsl::wrapToVec(spec.value, imageSpec) << ";\n";
        oss << "}\n";
        fragmentShaderBody = oss.str();
    }

    return [fragmentShaderBody, imageSpec]() {
        // GLSL compilation happens in the GL thread
        std::unique_ptr< Shader<Nullary> > shader(new Shader<Nullary>);
        shader->resources = GlslPipeline::create(fragmentShaderBody.c_str(), {}, imageSpec);
        GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

        shader->function = [&pipeline](Image &output) {
            // GLSL shader invocation also happens in the GL thread
            Binder binder(pipeline);
            pipeline.call(output.getFrameBuffer());
        };

        return shader;
    };
}

Shader<NAry>::Builder pixelwiseAffineCombination(const PixelwiseAffineCombinationSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {

    const int nInputs = spec.linear.size();
    std::string fragmentShaderBody;
    {
        std::ostringstream oss;

        const auto swiz = glsl::swizzleSubset(outSpec.channels);

        aa_assert(!spec.linear.empty());
        for (int i = 0; i < nInputs; ++i) {
            const auto &mat = spec.linear.at(i);

            aa_assert(outSpec.channels == int(mat.size()));
            aa_assert(inSpec.channels == int(mat.at(0).size()));
            // TODO: could use smaller mat or dot product for different
            // special cases for perhaps improved performance
            oss << "const mat4 m" << i << " = mat4(";
            for (int col = 0; col < 4; ++col) {
                oss << "vec4(";
                for (int row = 0; row < 4; ++row) {
                    if (row > 0) oss << ", ";
                    if (row < int(mat.size()) && col < int(mat.at(row).size()))
                        oss << mat.at(row).at(col);
                    else
                        oss << "0";
                }
                oss << ")";
                if (col < 3) oss << ",";
                oss << "\n";
            }
            oss << ");\n";
        }

        oss << "void main() {\n";
        const std::string vtype = glsl::floatVecType(outSpec.channels);
        oss << vtype << " v = ";
        if (spec.bias.empty()) {
            oss << vtype << "(0)";
        } else {
            aa_assert(outSpec.channels == int(spec.bias.size()));
            oss << glsl::wrapToFloatVec(spec.bias);
        }
        oss << ";\n";
        for (int i = 0; i < nInputs; ++i) {
            const auto &mat = spec.linear.at(i);
            oss << "vec4 texValue" << i << " = vec4(texelFetch(u_texture";
            if (nInputs > 1) oss << (i + 1);
            oss << ", ivec2(v_texCoord * vec2(u_outSize)), 0));\n";
            oss << "v += " << vtype << "(";
            if (!mat.empty()) oss << "m" << i << " * ";
            oss << "texValue" << i <<  ")." << swiz << ";\n";
        }
        oss << "outValue = " << getGlslVecType(outSpec) << "(v);\n";
        oss << "}\n";
        fragmentShaderBody = oss.str();
    }

    return [fragmentShaderBody, inSpec, outSpec, nInputs]() {
        std::unique_ptr< Shader<NAry> > shader(new Shader<NAry>);
        std::vector<ImageTypeSpec> inSpecs;
        for (int i = 0; i < nInputs; ++i) inSpecs.emplace_back(inSpec);

        shader->resources = GlslPipeline::create(fragmentShaderBody.c_str(), inSpecs, outSpec);
        GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

        auto textureBinders = std::shared_ptr< std::vector<Binder::Target*> >(new std::vector<Binder::Target*>);
        textureBinders->resize(nInputs, nullptr);

        shader->function = [&pipeline, inSpec, outSpec, nInputs, textureBinders](Image **inputs, int n, Image &output) {
            aa_assert(n == nInputs);

            Binder binder(pipeline);

            aa_assert(output == outSpec);
            for (int i = 0; i < nInputs; ++i) {
                auto &input = *inputs[i];
                aa_assert(input == inSpec);
                textureBinders->at(i) = &pipeline.bindTexture(i, input.getTextureId());
                textureBinders->at(i)->bind();
            }
            pipeline.call(output.getFrameBuffer());
            for (auto *b : *textureBinders) b->unbind();
        };

        return shader;
    };
}

Shader<Unary>::Builder channelwiseAffine(const ChannelwiseAffineSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    std::string fragmentShaderBody;
    {
        std::ostringstream oss;
        const auto swiz = glsl::swizzleSubset(inSpec.channels);

        oss << "void main() {\n";
        oss << "outValue = " << getGlslVecType(outSpec) << "(";
        if (std::fabs(spec.scale - 1.0) > 1e-10) oss << "float(" << spec.scale << ") * ";
        oss << "vec4(texelFetch(u_texture, ivec2(v_texCoord * vec2(u_outSize)), 0))." << swiz << "\n";
        if (std::fabs(spec.bias) > 1e-10) oss << " + float(" << spec.bias << ")";
        oss << ");\n";
        oss << "}\n";

        fragmentShaderBody = oss.str();
    }

    return [fragmentShaderBody, inSpec, outSpec]() {
        std::unique_ptr< Shader<Unary> > shader(new Shader<Unary>);
        shader->resources = GlslPipeline::create(fragmentShaderBody.c_str(), { inSpec }, outSpec);
        GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

        shader->function = [&pipeline, inSpec, outSpec](Image &input, Image &output) {
            aa_assert(input == inSpec);
            aa_assert(output == outSpec);
            Binder binder(pipeline);
            Binder inputBinder(pipeline.bindTexture(0, input.getTextureId()));
            pipeline.call(output.getFrameBuffer());
        };

        return shader;
    };
}

Shader<Unary>::Builder fixedConvolution2D(const FixedConvolution2DSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    aa_assert(!spec.kernel.empty());

    log_warn("TODO: convolution border property is not handled yet");

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

        const auto vtype = glsl::floatVecType(outSpec.channels);

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
        oss << "vec2 alpha = stride * vec2(u_outSize);\n";
        oss << vtype << " v = " << vtype << "(" << spec.bias << ");\n";
        oss << "for (int i = 0; i < KERNEL_H; i++) {\n";
        oss << "for (int j = 0; j < KERNEL_W; j++) {\n";
        oss << "    float k = kernel[uint(i * KERNEL_W + j)];\n";
        oss << "    vec2 coord = (alpha * v_texCoord + (vec2(j, i) + pixelOffset)) / vec2(textureSize(u_texture, 0));\n";
        oss << "    v += k * " << vtype << "(texture(u_texture, coord));\n";
        oss << "}\n";
        oss << "}\n";
        oss << "outValue = " << getGlslVecType(outSpec) << "(v);\n";
        oss << "}\n";

        fragmentShaderBody = oss.str();
    }

    return [fragmentShaderBody, spec, inSpec, outSpec]() {
        std::unique_ptr< Shader<Unary> > shader(new Shader<Unary>);
        shader->resources = GlslPipeline::create(fragmentShaderBody.c_str(), { inSpec }, outSpec);
        GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

        shader->function = [&pipeline, spec](Image &input, Image &output) {
            input.setBorder(spec.border);
            Binder binder(pipeline);
            Binder inputBinder(pipeline.bindTexture(0, input.getTextureId()));
            pipeline.call(output.getFrameBuffer());
        };

        return shader;
    };
}
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
            aa_assert(tmp && tmp->function);
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

    Function create(const FixedConvolution2DSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) final {
        checkSpec(inSpec);
        checkSpec(outSpec);
        return wrap<Unary>(impl::fixedConvolution2D(spec, inSpec, outSpec));
    }

    Function create(const FillSpec &spec, const ImageTypeSpec &imageSpec) final {
        checkSpec(imageSpec);
        return wrap<Nullary>(impl::fill(spec, imageSpec));
    }

    Function create(const PixelwiseAffineCombinationSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) final {
        checkSpec(inSpec);
        checkSpec(outSpec);
        return wrapNAry(impl::pixelwiseAffineCombination(spec, inSpec, outSpec));
    }

    Function create(const ChannelwiseAffineSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) final {
        checkSpec(inSpec);
        checkSpec(outSpec);
        return wrap<Unary>(impl::channelwiseAffine(spec, inSpec, outSpec));
    }
};
}

std::unique_ptr<Factory> createFactory(Processor &processor) {
    return std::unique_ptr<Factory>(new GpuFactory(processor));
}

}
}
}
