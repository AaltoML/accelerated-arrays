#include <cstring>

#include "read_adapters.hpp"
#include "glsl_helpers.hpp"
#include "operations.hpp"
#include "../log.hpp"

namespace accelerated {
namespace opengl {
namespace operations {
namespace {
struct Adapter {
    std::vector<std::uint8_t> cpuBuffer;
    std::function<void(std::uint8_t*)> cpuFunction;

    std::unique_ptr<::accelerated::Image> buffer;
    ::accelerated::operations::Function function;

    bool setRepackFunction(const Image &image) {
        if (buffer->size() == image.size()) return false;
        cpuBuffer.resize(buffer->size());
        const int origRowWidth = image.width * image.bytesPerPixel();
        const int bufRowWidth = buffer->width * buffer->bytesPerPixel();
        aa_assert(origRowWidth < bufRowWidth);
        log_debug("repacking to rows of %d bytes from rows of length %d", origRowWidth, bufRowWidth);

        cpuFunction = [this, origRowWidth, bufRowWidth](std::uint8_t *out) {
            const int nRows = buffer->height;

            //for (const auto &el : cpuBuffer) LOG_TRACE("cpu-buffer-unpacked: %d", int(el));

            int inOffset = 0, outOffset = 0;
            for (int i = 0; i < nRows; ++i) {
                std::memcpy(out + outOffset, cpuBuffer.data() + inOffset, origRowWidth);
                outOffset += origRowWidth;
                inOffset += bufRowWidth;
            }
            //for (const auto &el : cpuBuffer) LOG_TRACE("cpu-buffer-packed: %d", int(el));
            aa_assert(inOffset == int(buffer->size()));
        };
        return true;
    }
};

operations::Shader<Unary>::Builder createFunction(const Image &img, int targetChannels, int &targetWidth) {
    int origChannelsPerRow = img.channels * img.width;

    // only support these atm
    aa_assert(targetChannels > img.channels);
    aa_assert(targetChannels % img.channels == 0); // TODO

    targetWidth = origChannelsPerRow / targetChannels;
    if (origChannelsPerRow % targetChannels != 0) {
        targetWidth++;
    }

    std::string fragmentShaderBody;
    {
        std::ostringstream oss;
        const auto swiz = glsl::swizzleSubset(img.channels);
        oss << "void main() {\n";
        oss << "ivec2 outCoord = ivec2(v_texCoord * vec2(u_outSize));\n";
        int ratio = targetChannels / img.channels;
        oss << "int x0 = outCoord.x * " << ratio << ";\n";
        // target should be UFIXED8
        oss << "outValue = " << glsl::floatVecType(targetChannels) << "(";
        for (int i = 0; i < ratio; ++i) {
            if (i > 0) oss << ",\n";
            oss << "texelFetch(u_texture, " << "ivec2(x0 + " << i << ", outCoord.y)" << ", 0)." << swiz;
        }
        oss << ");\n";
        oss << "}\n";
        fragmentShaderBody = oss.str();
    }

    ImageTypeSpec spec = img;
    ImageTypeSpec outSpec {
        targetChannels,
        img.dataType,
        ImageTypeSpec::StorageType::GPU_OPENGL
    };

    return [fragmentShaderBody, spec, outSpec]() {
        std::unique_ptr< Shader<Unary> > shader(new Shader<Unary>);
        shader->resources = GlslPipeline::create(fragmentShaderBody.c_str(), { spec }, outSpec);
        GlslPipeline &pipeline = reinterpret_cast<GlslPipeline&>(*shader->resources);

        shader->function = [&pipeline](Image &input, Image &output) {
            Binder binder(pipeline);
            Binder inputBinder(pipeline.bindTexture(0, input.getTextureId()));
            pipeline.call(output.getFrameBuffer());
        };

        return shader;
    };
}
}

std::function<Future(std::uint8_t*)> createReadAdpater(
    Image &image,
    Processor &processor,
    Image::Factory &imageFactory,
    Factory &opFactory)
{
    std::shared_ptr<Adapter> adapter(new Adapter);

    ImageTypeSpec::DataType targetDataType = image.dataType;
    int targetChannels = image.channels;

    #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
        aa_assert(image.bytesPerChannel() != 2);
        #ifdef ACCELERATED_ARRAYS_MAX_COMPATIBILITY_READS
            aa_assert(image.bytesPerChannel() == 1);
            targetChannels = 4;
        #endif
        #ifndef ACCELERATED_ARRAYS_DODGY_READS
        aa_assert(!ImageTypeSpec::isSigned(image.dataType));
        #endif
    #else
        if (image.channels == 2) targetChannels = 4;
    #endif
    aa_assert(targetDataType == image.dataType); // TODO

    int targetWidth;
    adapter->function = opFactory.wrap<Unary>(createFunction(image, targetChannels, targetWidth));
    aa_assert(adapter->function);

    adapter->buffer = imageFactory.create(
        targetWidth,
        image.height,
        targetChannels,
        targetDataType);

    if (adapter->setRepackFunction(image)) {
        log_warn("image read dimensions not optimal, need CPU repacking");
    }

    return [adapter, &image, &processor](std::uint8_t *outData) -> Future {
        // aa_assert(adapter->buffer->supportsDirectRead());
        ::accelerated::operations::callUnary(adapter->function, image, *adapter->buffer);
        // no need to wait if single-threaded
        if (adapter->cpuFunction) {
            adapter->buffer->readRaw(adapter->cpuBuffer.data());
            return processor.enqueue([adapter, outData]() {
                LOG_TRACE("CPU copy");
                adapter->cpuFunction(outData);
            });
        } else {
            return adapter->buffer->readRaw(outData);
        }
    };
}

}
}
}
