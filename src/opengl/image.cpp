#include <atomic>
#include <cassert>
#include <mutex>
#include <unordered_map>

#include "adapters.hpp"
#include "image.hpp"
#include "read_adapters.hpp"
#include "../log.hpp"

namespace accelerated {
namespace opengl {
namespace {
class ImplementationBase : public Image {
private:
    Border border = Border::UNDEFINED;
    Interpolation interpolation = Interpolation::UNDEFINED;

protected:
    ImplementationBase(int w, int h, const ImageTypeSpec &spec) : Image(w, h, spec) {}

public:
    Border getBorder() const final {
        return border;
    }

    void setBorder(Border b) final {
        border = b;
    }

    Interpolation getInterpolation() const final {
        return interpolation;
    }

    void setInterpolation(Interpolation i) final {
        interpolation = i;
    }
};

class ExternalImage : public ImplementationBase {
private:
    int textureId;

public:
    ExternalImage(int w, int h, int textureId, const ImageTypeSpec &spec)
    : ImplementationBase(w, h, spec), textureId(textureId) {}

    int getTextureId() const final {
        return textureId;
    }

    Future readRaw(std::uint8_t *outputData) final {
        aa_assert(false && "not supported");
        (void)outputData;
        return Future({});
    }

    Future writeRaw(const std::uint8_t *inputData) final {
        aa_assert(false && "not supported");
        (void)inputData;
        return Future({});
    }

    bool supportsDirectRead() const final {
        return false;
    }

    bool supportsDirectWrite() const final {
        return false;
    }

    FrameBuffer &getFrameBuffer() final {
        aa_assert(false && "not supported");
        // nullptr dereference gets a warning as UB, let's try another
        // address :)
        return *reinterpret_cast<FrameBuffer*>(1);
    }

    std::unique_ptr<::accelerated::Image> createROI(int x0, int y0, int roiWidth, int roiHeight) final {
        (void)x0; (void)y0; (void)roiWidth; (void)roiHeight;
        aa_assert(false && "not supported");
        return {};
    }
};

class FrameBufferManager final : public Image::Factory {
private:
    class Reference;
    std::mutex mutex;
    std::unordered_map<const Reference*, std::shared_ptr<FrameBuffer> > frameBuffers;
    std::unique_ptr<operations::Factory> converterFactory;

public:
    Processor &processor;

    FrameBufferManager(Processor &p)
    : converterFactory(operations::createFactory(p)), processor(p) {}

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

    void addFrameBuffer(const Reference *ref, const std::function<std::shared_ptr<FrameBuffer>()> &builder) {
        // easier to implement with shared_ptr in the argument, even if it
        // "should" be unique_ptr and the Reference ctor effectively transfers
        // the ownership here
        processor.enqueue([this, ref, builder]() {
            auto fb = builder();
            {
                std::lock_guard<std::mutex> lock(mutex);
                aa_assert(!frameBuffers.count(ref));
                LOG_TRACE("frame buffer for reference %p set to %d", (void*)ref, fb->getId());
                frameBuffers[ref] = fb;
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
            (void)ref;
            LOG_TRACE("frame buffer for reference %p destroyed", (void*)ref);
        });
    }

    std::shared_ptr<FrameBuffer> getFrameBuffer(const Reference *ref) {
        std::lock_guard<std::mutex> lock(mutex);
        return frameBuffers.at(ref);
    }

    std::unique_ptr<Image> wrapTexture(int textureId, int w, int h, const ImageTypeSpec &spec) final {
        return std::unique_ptr<Image>(new ExternalImage(w, h, textureId, spec));
    }

    std::unique_ptr<::accelerated::Image> create(int w, int h, int channels, ImageTypeSpec::DataType dtype) final;
    std::unique_ptr<Image> wrapFrameBuffer(int frameBufferId, int w, int h, const ImageTypeSpec &spec) final;
    std::unique_ptr<Image> wrapScreen(int w, int h) final;

    ImageTypeSpec getSpec(int channels, ImageTypeSpec::DataType dtype) final {
        return Image::getSpec(channels, dtype);
    }
};

class FrameBufferManager::Reference : public ImplementationBase {
private:
    FrameBufferManager &manager;
    std::function<Future(std::uint8_t*)> readAdpater;

public:
    Reference(int w, int h, const ImageTypeSpec &spec, FrameBufferManager &m, std::unique_ptr<FrameBuffer> existing)
    : ImplementationBase(w, h, spec), manager(m)
    {
        ImageTypeSpec s = spec;
        std::shared_ptr<FrameBuffer> fb = std::move(existing);
        LOG_TRACE("created buffer reference %p", (void*)this);
        manager.addFrameBuffer(this, [w, h, s, fb]() {
            if (fb) return fb;
            return std::shared_ptr<FrameBuffer>(FrameBuffer::create(w, h, s));
        });
    }

    // ROI
    Reference(int x0, int y0, int w, int h, FrameBufferManager &m, Reference &existing)
    : ImplementationBase(w, h, existing), manager(m)
    {
        LOG_TRACE("created buffer reference %p (ROI)", (void*)this);
        manager.addFrameBuffer(this, [x0, y0, w, h, &m, &existing]() {
            auto targetFB = m.getFrameBuffer(&existing);
            aa_assert(targetFB && "failed to create ROI frame buffer, target does not exist");
            return std::shared_ptr<FrameBuffer>(targetFB->createROI(x0, y0, w, h));
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
        if (!supportsDirectRead()) {
            if (!readAdpater) {
                log_warn("frame buffer ref %p does not support direct read, trying to create adapter buffer", (void*)this);
                readAdpater = createReadAdpater(
                    *this,
                    manager.processor,
                    manager,
                    *manager.converterFactory);
            }
            return readAdpater(outputData);
        }
        LOG_TRACE("reading frame buffer reference %p", (void*)this);
        return manager.enqueue(this, [outputData](FrameBuffer &fb) {
            fb.readPixels(outputData);
        });
    }

    Future writeRaw(const std::uint8_t *inputData) final {
        aa_assert(supportsDirectWrite());
        LOG_TRACE("writing frame buffer reference %p", (void*)this);
        return manager.enqueue(this, [inputData](FrameBuffer &fb) {
            fb.writePixels(inputData);
        });
    }

    virtual bool supportsDirectRead() const final {
        // TODO: in OpenGL ES, only a limited subset of textures
        // support reading & writing directly, but this is not the case in
        // general in OpenGL
        #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
            #ifdef ACCELERATED_ARRAYS_MAX_COMPATIBILITY_READS
                if (channels != 4 || bytesPerChannel() != 1) return false;
            #endif
            return bytesPerChannel() != 2;
        #else
            #ifdef ACCELERATED_ARRAYS_DODGY_READS
                return true;
            #else
                return channels != 2;
            #endif
        #endif
    }

    bool supportsDirectWrite() const final {
        return true;
    }

    FrameBuffer &getFrameBuffer() final {
        auto fb = manager.getFrameBuffer(this);
        aa_assert(fb && "frame buffer object not created yet");
        return *fb;
    }

    std::unique_ptr<::accelerated::Image> createROI(int x0, int y0, int roiWidth, int roiHeight) final {
        return std::unique_ptr<::accelerated::Image>(new Reference(x0, y0, roiWidth, roiHeight, manager, *this));
    }
};

std::unique_ptr<::accelerated::Image> FrameBufferManager::create(int w, int h, int channels, ImageTypeSpec::DataType dtype) {
    return std::unique_ptr<::accelerated::Image>(new FrameBufferManager::Reference(w, h,
        Image::getSpec(channels, dtype, ImageTypeSpec::StorageType::GPU_OPENGL), *this, {}));
}

std::unique_ptr<Image> FrameBufferManager::wrapFrameBuffer(int fboId, int w, int h, const ImageTypeSpec &spec) {
    return std::unique_ptr<Image>(
        new FrameBufferManager::Reference(w, h, spec, *this,
            FrameBuffer::createReference(fboId, w, h, spec)));
}

std::unique_ptr<Image> FrameBufferManager::wrapScreen(int w, int h) {
    auto spec = getScreenImageTypeSpec();
    return std::unique_ptr<Image>(
        new FrameBufferManager::Reference(w, h, *spec, *this,
            FrameBuffer::createScreenReference(w, h)));
}
}

ImageTypeSpec Image::getSpec(int channels, DataType dtype, StorageType stype) {
    aa_assert(stype == StorageType::GPU_OPENGL || stype == StorageType::GPU_OPENGL_EXTERNAL);
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
    aa_assert(isCompatible(image.storageType));
    return reinterpret_cast<Image&>(image);
}

std::unique_ptr<Image::Factory> Image::createFactory(Processor &p) {
    return std::unique_ptr<Image::Factory>(new FrameBufferManager(p));
}

Image::Image(int w, int h, const ImageTypeSpec &spec) :
    ::accelerated::Image(w, h, spec) {}

}
}
