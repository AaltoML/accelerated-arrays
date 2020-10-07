#include <cassert>

#include "adapters.hpp"
#include "../image.hpp"

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
    assert(spec.dataType == ImageTypeSpec::DataType::UINT8);
    if (spec.channels == 1) {
        // log_debug("getInternalFormat:GL_R8");
        return GL_R8;
    }
    else if (spec.channels == 4) {
        // log_debug("getInternalFormat:GL_RGBA");
        return GL_RGBA;
    }
    // TODO
    assert(false && "not implemented");
    return -1;
}

int getFormat(const ImageTypeSpec &spec) {
    if (spec.channels == 1) {
        // log_debug("getFormat:GL_RED");
        return GL_RED;
    }
    else if (spec.channels == 3) {
        // log_debug("getFormat:GL_RGB");
        return GL_RGB; // TODO: check
    }
    else if (spec.channels == 4) {
        // log_debug("getFormat:GL_RGBA");
        return GL_RGBA;
    }
    assert(false && "not implemented");
    return -1;
}

int getBindType(const ImageTypeSpec &spec) {
    if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL) {
        // log_debug("getBindType:GL_TEXTURE_2D");
        return GL_TEXTURE_2D;
    } else if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL_EXTERNAL) {
        // log_debug("getBindType:GL_TEXTURE_EXTERNAL_OES");
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
        log_debug("created texture %d of size %d x %d x %d", id, width, height, spec.channels);

        //glActiveTexture(GL_TEXTURE0); // TODO: required?

        Binder binder(*this);
        glTexImage2D(GL_TEXTURE_2D, 0,
            getInternalFormat(spec),
            width, height, 0,
            getFormat(spec),
            GL_UNSIGNED_BYTE, nullptr);

        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        checkError(__FUNCTION__);
    }

    void destroy() final {
        if (id != 0) {
            log_debug("deleting texture %d", id);
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
        log_debug("bound texture %d", id);
        checkError(__FUNCTION__);
    }

    void unbind() final {
        glBindTexture(bindType, 0);
        log_debug("unbound texture");
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
        log_debug("generated frame buffer %d", id);
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
            log_debug("destroying frame buffer %d", id);
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
        glBindFramebuffer(GL_FRAMEBUFFER, id);
        checkError(__FUNCTION__);
    }

    void unbind() final {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        checkError(__FUNCTION__);
    }

    void readPixels(uint8_t *pixels) final {
        // OpenGL ES only supports GL_RGBA / GL_UNSIGNED_BYTE (in practice)
        Binder(*this);
        assert(spec.channels == 4 && spec.dataType == Image::DataType::UINT8);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        checkError(__FUNCTION__);
    }

    void writePixels(const uint8_t *pixels) final {
        (void)pixels;
        assert(false && "TODO: not implemented");
    }

    // int getId() const { return id; }

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
