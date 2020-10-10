#include <cassert>
#include <mutex>
#include <unordered_map>

#include "adapters.hpp"
#include "image.hpp"

//#define ACCELERATED_ARRAYS_DEBUG_IMAGE
#ifdef ACCELERATED_ARRAYS_DEBUG_IMAGE
#define LOG_TRACE(...) log_debug(__VA_ARGS__)
#else
#define LOG_TRACE(...) (void)0
#endif

namespace accelerated {
namespace opengl {
namespace {
class ExternalImage : public Image {
private:
    int textureId = -1;

public:
    ExternalImage(int w, int h, int textureId, const ImageTypeSpec &spec)
    : Image(w, h, spec), textureId(textureId) {}

    int getTextureId() const final {
        return textureId;
    }

    Future readRaw(std::uint8_t *outputData) final {
        assert(false && "not supported");
        (void)outputData;
        return Future({});
    }

    Future writeRaw(const std::uint8_t *inputData) final {
        assert(false && "not supported");
        (void)inputData;
        return Future({});
    }

    bool supportsReadAndWrite() const final {
        return false;
    }

    FrameBuffer &getFrameBuffer() final {
        assert(false && "not supported");
        return *reinterpret_cast<FrameBuffer*>(0);
    }
};

class FrameBufferManager final : public Image::Factory {
private:
    class Reference;
    std::mutex mutex;
    std::unordered_map<const Reference*, std::shared_ptr<FrameBuffer> > frameBuffers;

public:
    Processor &processor;

    FrameBufferManager(Processor &p) : processor(p) {}

    Future enqueue(const Reference *ref, const std::function<void(FrameBuffer &)> &f) {
        return processor.enqueue([this, f, ref]() {
            std::shared_ptr<FrameBuffer> buf;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!frameBuffers.count(ref)) {
                    log_warn("no reference %p found in enqueue (already destroyed?)", (void*)ref);
                    return;
                }
                buf = frameBuffers.at(ref);
            }
            f(*buf);
        });
    }

    void addFrameBuffer(const Reference *ref, const std::function<std::unique_ptr<FrameBuffer>()> &builder) {
        processor.enqueue([this, ref, builder]() {
            auto fb = builder();
            {
                std::lock_guard<std::mutex> lock(mutex);
                assert(!frameBuffers.count(ref));
                LOG_TRACE("frame buffer for reference %p set to %d", (void*)ref, fb->getId());
                frameBuffers[ref] = std::move(fb);
            }
        });
    }

    void removeFrameBuffer(const Reference *ref) {
        std::shared_ptr<FrameBuffer> buf;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!frameBuffers.count(ref)) {
                log_warn("no reference %p found in removeFrameBuffer (already destroyed?)", (void*)ref);
                return;
            }
            buf = frameBuffers.at(ref);
            frameBuffers.erase(ref);
        }

        processor.enqueue([buf, ref]() {
            buf->destroy();
            LOG_TRACE("frame buffer for reference %p destroyed", (void*)ref);
        });
    }

    std::shared_ptr<FrameBuffer> getFrameBuffer(const Reference *ref) {
        std::lock_guard<std::mutex> lock(mutex);
        return frameBuffers.at(ref);
    }

    std::unique_ptr<::accelerated::Image> create(int w, int h, int channels, ImageTypeSpec::DataType dtype) final;

    std::unique_ptr<Image> wrapTexture(int textureId, int w, int h, const ImageTypeSpec &spec) {
        return std::unique_ptr<Image>(new ExternalImage(w, h, textureId, spec));
    }
};

class FrameBufferManager::Reference : public Image {
private:
    FrameBufferManager &manager;

public:
    Reference(int w, int h, const ImageTypeSpec &spec, FrameBufferManager &m) : Image(w, h, spec), manager(m) {
        ImageTypeSpec s = spec;
        LOG_TRACE("created buffer reference %p", (void*)this);
        manager.addFrameBuffer(this, [w, h, s]() {
            return FrameBuffer::create(w, h, s);
        });
    }

    ~Reference() {
        LOG_TRACE("destroyed buffer reference %p", (void*)this);
        manager.removeFrameBuffer(this);
    }

    int getTextureId() const final {
        // TODO: not optimal
        return const_cast<Reference&>(*this).manager.getFrameBuffer(this)->getTextureId();
    }

    Future readRaw(std::uint8_t *outputData) final {
        assert(supportsReadAndWrite());
        LOG_TRACE("reading frame buffer reference %p", (void*)this);
        return manager.enqueue(this, [outputData](FrameBuffer &fb) {
            fb.readPixels(outputData);
        });
    }

    Future writeRaw(const std::uint8_t *inputData) final {
        assert(supportsReadAndWrite());
        LOG_TRACE("writing frame buffer reference %p", (void*)this);
        return manager.enqueue(this, [inputData](FrameBuffer &fb) {
            fb.writePixels(inputData);
        });
    }

    bool supportsReadAndWrite() const final {
        // TODO: in OpenGL ES, only a limited subset of textures
        // support reading & writing directly, but this is not the case in
        // general in OpenGL
        // TODO: not correct
        #ifdef USE_OPENGL_ES_ONLY
            return dataType == DataType::UINT8 && channels == 4;
        #else
            return true;
        #endif
    }

    FrameBuffer &getFrameBuffer() final {
        return *manager.getFrameBuffer(this);
    }
};

std::unique_ptr<::accelerated::Image> FrameBufferManager::create(int w, int h, int channels, ImageTypeSpec::DataType dtype) {
    return std::unique_ptr<::accelerated::Image>(new FrameBufferManager::Reference(w, h,
        Image::getSpec(channels, dtype, ImageTypeSpec::StorageType::GPU_OPENGL), *this));
}

}

ImageTypeSpec Image::getSpec(int channels, DataType dtype, StorageType stype) {
    assert(stype == StorageType::GPU_OPENGL || stype == StorageType::GPU_OPENGL_EXTERNAL);
    return ImageTypeSpec {
        channels,
        dtype,
        stype
    };
}

bool Image::isCompatible(ImageTypeSpec::StorageType storageType) {
    return storageType == StorageType::GPU_OPENGL ||
        storageType == StorageType::GPU_OPENGL_EXTERNAL;
}

Image &Image::castFrom(::accelerated::Image &image) {
    assert(isCompatible(image.storageType));
    return reinterpret_cast<Image&>(image);
}

std::unique_ptr<Image::Factory> Image::createFactory(Processor &p) {
    return std::unique_ptr<Image::Factory>(new FrameBufferManager(p));
}

Image::Image(int w, int h, const ImageTypeSpec &spec) :
    ::accelerated::Image(w, h, spec) {}

}
}