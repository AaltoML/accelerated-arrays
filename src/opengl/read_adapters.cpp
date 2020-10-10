#include <cassert>
#include <cstring>

#include "read_adapters.hpp"
#include "glsl_helpers.hpp"
#include "operations.hpp"

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
        assert(origRowWidth < bufRowWidth);
        log_debug("repacking to rows of %d bytes from rows of length %d", origRowWidth, bufRowWidth);

        cpuFunction = [this, origRowWidth, bufRowWidth](std::uint8_t *out) {
            const int nRows = buffer->height;

            int inOffset = 0, outOffset = 0;
            for (int i = 0; i < nRows; ++i) {
                std::memcpy(out + outOffset, cpuBuffer.data() + inOffset, origRowWidth);
                outOffset += origRowWidth;
                inOffset += bufRowWidth;
            }
            // for (const auto &el : cpuBuffer) log_debug("cpu-buffer: %d", int(el));
            assert(inOffset == int(buffer->size()));
        };
        return true;
    }
};

operations::Shader<Unary>::Builder createFunction(const Image &img, int targetChannels, int &targetWidth) {
    int origChannelsPerRow = img.channels * img.width;

    // only support these atm
    assert(targetChannels > img.channels);
    assert(targetChannels % img.channels == 0); // TODO

    targetWidth = origChannelsPerRow / targetChannels;
    if (origChannelsPerRow % targetChannels != 0) {
        targetWidth++;
    }

    std::string fragmentShaderBody;
    {
        std::ostringstream oss;
        const auto swiz = glsl::swizzleSubset(4);
        oss << "void main() {\n";
        oss << "float delta = 1.0 / u_textureSize.x;\n";
        oss << "float offset = 0.5 * (delta - 1.0/u_outSize.x);\n";
        oss << "vec2 baseCoord = v_texCoord + vec2(offset, 0);\n";
        for (int i = 0; i < targetChannels / img.channels; ++i) {
            oss << "vec4 col" << i << " = texture2D(u_texture, baseCoord + delta * float(" << i << "));\n";
            for (int j = 0; j < img.channels; ++j) {
                oss << "gl_FragColor." << swiz[i*img.channels + j] << " = col" << i << "." << swiz[j] << ";\n";
            }
        }
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

    #ifdef USE_OPENGL_ES_ONLY
        targetChannels = 4;
        targetDataType = ImageTypeSpec::DataType::UINT8;
    #else
        if (image.channels == 2) targetChannels = 4;
    #endif
    assert(targetDataType == image.dataType); // TODO

    int targetWidth;
    adapter->function = opFactory.wrap<Unary>(createFunction(image, targetChannels, targetWidth));
    assert(adapter->function);

    adapter->buffer = imageFactory.create(
        targetWidth,
        image.height,
        targetChannels,
        targetDataType);

    if (adapter->setRepackFunction(image)) {
        log_warn("image read dimensions not optimal, need CPU repacking");
    }

    return [adapter, &image, &processor](std::uint8_t *outData) -> Future {
        // assert(adapter->buffer->supportsDirectRead());
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
