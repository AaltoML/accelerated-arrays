#include <cassert>

#include "adapters.hpp"
#include "../image.hpp"

//#define ACCELERATED_ARRAYS_DEBUG_ADAPTERS
#ifdef ACCELERATED_ARRAYS_DEBUG_ADAPTERS
#define LOG_TRACE(...) log_debug(__VA_ARGS__)
#else
#define LOG_TRACE(...) (void)0
#endif

namespace accelerated {
namespace opengl {
void checkError(const char *tag) {
    GLint error;
    bool any = false;
    while((error = glGetError())) {
        any = true;
        log_error("%s produced glError (0x%x)", tag, error);
    }
    if (any) {
        std::abort();
    }
}

void checkError(std::string tag) {
    checkError(tag.c_str());
}

// could be exposed in the hpp file but not used currently elsewhere
struct Texture : Binder::Target {
    static std::unique_ptr<Texture> create(int w, int h, const ImageTypeSpec &spec);
    virtual ~Texture();
    virtual int getId() const = 0;
    virtual void destroy() = 0;
};

namespace {
int getInternalFormat(const ImageTypeSpec &spec) {
    // TODO: fill in rest https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml

    #define X(x) LOG_TRACE("getInternalFormat:%s", #x); return x
    if (spec.channels == 1) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::UINT8: X(GL_R8);
            case ImageTypeSpec::DataType::SINT8: X(GL_R8_SNORM);
            default: break;
        }
    }
    else if (spec.channels == 4) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::UINT8: X(GL_RGBA); // note: unsized default
            case ImageTypeSpec::DataType::SINT8: X(GL_RGBA8_SNORM);
            default: break;
        }
    }
    #undef X
    assert(false && "not implemented");
    return -1;
}

int getFormat(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getFormat:%s", #x); return x
    switch (spec.channels) {
        case 1: X(GL_RED);
        case 2: assert(false && "not implemented"); return -1;
        case 3: X(GL_RGB); // TODO: check
        case 4: X(GL_RGBA);
        default: break;
    }
    #undef X
    assert(false);
    return -1;
}

int getCpuType(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getFormat:%s", #x); return x
    switch (spec.dataType) {
        case ImageTypeSpec::DataType::UINT8: X(GL_UNSIGNED_BYTE);
        case ImageTypeSpec::DataType::SINT8: X(GL_BYTE);
        case ImageTypeSpec::DataType::UINT16: X(GL_UNSIGNED_SHORT);
        case ImageTypeSpec::DataType::SINT16: X(GL_SHORT);
        case ImageTypeSpec::DataType::UINT32: X(GL_UNSIGNED_INT);
        case ImageTypeSpec::DataType::SINT32: X(GL_INT);
        case ImageTypeSpec::DataType::FLOAT32: X(GL_FLOAT);
    }
    #undef X
    assert(false);
    return -1;
}

int getBindType(const ImageTypeSpec &spec) {
    if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL) {
        LOG_TRACE("getBindType:GL_TEXTURE_2D");
        return GL_TEXTURE_2D;
    } else if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL_EXTERNAL) {
        LOG_TRACE("getBindType:GL_TEXTURE_EXTERNAL_OES");
        return GL_TEXTURE_EXTERNAL_OES;
    }
    assert(false);
    return -1;
}

class TextureImplementation : public Texture {
private:
    const GLuint bindType;
    GLuint id;

public:
    TextureImplementation(int width, int height, const ImageTypeSpec &spec)
    : bindType(getBindType(spec)), id(0) {
        glGenTextures(1, &id);
        LOG_TRACE("created texture %d of size %d x %d x %d", id, width, height, spec.channels);

        // glActiveTexture(GL_TEXTURE0); // TODO: required?

        Binder binder(*this);
        glTexImage2D(GL_TEXTURE_2D, 0,
            getInternalFormat(spec),
            width, height, 0,
            getFormat(spec),
            GL_UNSIGNED_BYTE, nullptr);

        // TODO: move somewhere else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        checkError(__FUNCTION__);
    }

    void destroy() final {
        if (id != 0) {
            LOG_TRACE("deleting texture %d", id);
            glDeleteTextures(1, &id);
        }
        id = 0;
    }

    ~TextureImplementation() {
        if (id != 0) {
            log_warn("leaking GL texture");
        }
    }

    void bind() final {
        glBindTexture(bindType, id);
        LOG_TRACE("bound texture %d", id);
        checkError(__FUNCTION__);
    }

    void unbind() final {
        glBindTexture(bindType, 0);
        LOG_TRACE("unbound texture");
        checkError(__FUNCTION__);
    }

    int getId() const { return id; }
};

class FrameBufferImplementation : public FrameBuffer {
private:
    int width, height;
    ImageTypeSpec spec;
    GLuint id;
    TextureImplementation texture;

public:
    FrameBufferImplementation(int w, int h, const ImageTypeSpec &spec)
    : width(w), height(h), spec(spec), id(0), texture(w, h, spec) {
        glGenFramebuffers(1, &id);
        LOG_TRACE("generated frame buffer %d", id);
        checkError(std::string(__FUNCTION__) + "/glGenFramebuffers");

        assert(spec.storageType == Image::StorageType::GPU_OPENGL);

        Binder binder(*this);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.getId(), 0);
        checkError(std::string(__FUNCTION__) + "/glFramebufferTexture2D");
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        checkError(std::string(__FUNCTION__) + "/end");
    }

    void destroy() final {
        if (id != 0) {
            LOG_TRACE("destroying frame buffer %d", id);
            glDeleteFramebuffers(1, &id);
        }
        id = 0;
        texture.destroy();
    }

    ~FrameBufferImplementation() {
        if (id != 0) {
            log_warn("leaking frame buffer %d", id);
        }
    }

    void bind() final {
        LOG_TRACE("bound frame buffer %d", id);
        glBindFramebuffer(GL_FRAMEBUFFER, id);
        checkError(__FUNCTION__);
    }

    void unbind() final {
        LOG_TRACE("unbound frame buffer");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        checkError(__FUNCTION__);
    }

    void readPixels(uint8_t *pixels) final {
        LOG_TRACE("reading frame buffer %d", id);
        Binder binder(*this);
        // Note: OpenGL ES only supports GL_RGBA / GL_UNSIGNED_BYTE (in practice)
        glReadPixels(0, 0, width, height, getFormat(spec), getCpuType(spec), pixels);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        checkError(__FUNCTION__);
    }

    void writePixels(const uint8_t *pixels) final {
        Binder binder(texture);

        glTexImage2D(GL_TEXTURE_2D, 0,
            getInternalFormat(spec),
            width, height, 0,
            getFormat(spec),
            getCpuType(spec),
            pixels);
        checkError(__FUNCTION__);
    }

    int getId() const { return id; }

    int getTextureId() const final {
        return texture.getId();
    }
};
}

Binder::Binder(Target &target) : target(target) { target.bind(); }
Binder::~Binder() { target.unbind(); }

Texture::~Texture() = default;
FrameBuffer::~FrameBuffer() = default;
GlslProgram::~GlslProgram() = default;

std::unique_ptr<Texture> Texture::create(int w, int h, const ImageTypeSpec &spec) {
    return std::unique_ptr<Texture>(new TextureImplementation(w, h, spec));
}

std::unique_ptr<FrameBuffer> FrameBuffer::create(int w, int h, const ImageTypeSpec &spec) {
    return std::unique_ptr<FrameBuffer>(new FrameBufferImplementation(w, h, spec));
}


}
}
