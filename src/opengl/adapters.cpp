#include <cassert>
#include <sstream>

#include "adapters.hpp"
#include "../image.hpp"

#define ACCELERATED_ARRAYS_DEBUG_ADAPTERS
#ifdef ACCELERATED_ARRAYS_DEBUG_ADAPTERS
#define LOG_TRACE(...) log_debug(__VA_ARGS__)
#else
#define LOG_TRACE(...) (void)0
#endif

// TODO: move somewhere else
#define IS_OPENGL_ES

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
struct Texture : Destroyable, Binder::Target {
    static std::unique_ptr<Texture> create(int w, int h, const ImageTypeSpec &spec);
    virtual int getId() const = 0;
};

namespace {
// TODO: Integer formats would be preferable in some cases, but the support
// seems to be limited (e.g., can't be read in glReadPixels)
constexpr bool useGlIntegerFormats = false;

int getInternalFormat(const ImageTypeSpec &spec, bool useUnsizedFormats = false) {
    #define X(x) LOG_TRACE("getInternalFormat:%s", #x); return x

    if (useUnsizedFormats) {
        switch (spec.channels) {
            case 1: X(GL_RED);
            case 2: X(GL_RG);
            case 3: X(GL_RGB);
            case 4: X(GL_RGBA);
            default: assert(false); break;
        }
    }
    else if (useGlIntegerFormats && spec.dataType != ImageTypeSpec::DataType::FLOAT32) {
        if (spec.channels == 1) {
            switch (spec.dataType) {
                case ImageTypeSpec::DataType::UINT8: X(GL_R8UI);
                case ImageTypeSpec::DataType::SINT8: X(GL_R8I);
                case ImageTypeSpec::DataType::UINT16: X(GL_R16UI);
                case ImageTypeSpec::DataType::SINT16: X(GL_R16I);
                case ImageTypeSpec::DataType::UINT32: X(GL_R32UI);
                case ImageTypeSpec::DataType::SINT32: X(GL_R32I);
                default: break;
            }
        }
        else if (spec.channels == 2) {
            switch (spec.dataType) {
                case ImageTypeSpec::DataType::UINT8: X(GL_RG8UI);
                case ImageTypeSpec::DataType::SINT8: X(GL_RG8I);
                case ImageTypeSpec::DataType::UINT16: X(GL_RG16UI);
                case ImageTypeSpec::DataType::SINT16: X(GL_RG16I);
                case ImageTypeSpec::DataType::UINT32: X(GL_RG32UI);
                case ImageTypeSpec::DataType::SINT32: X(GL_RG32I);
                default: break;
            }
        }
        else if (spec.channels == 3) {
            switch (spec.dataType) {
                case ImageTypeSpec::DataType::UINT8: X(GL_RGB8UI);
                case ImageTypeSpec::DataType::SINT8: X(GL_RGB8I);
                case ImageTypeSpec::DataType::UINT16: X(GL_RGB16UI);
                case ImageTypeSpec::DataType::SINT16: X(GL_RGB16I);
                case ImageTypeSpec::DataType::UINT32: X(GL_RGB32UI);
                case ImageTypeSpec::DataType::SINT32: X(GL_RGB32I);
                default: break;
            }
        }
        else if (spec.channels == 4) {
            switch (spec.dataType) {
                case ImageTypeSpec::DataType::UINT8: X(GL_RGBA8UI);
                case ImageTypeSpec::DataType::SINT8: X(GL_RGBA8I);
                case ImageTypeSpec::DataType::UINT16: X(GL_RGBA16UI);
                case ImageTypeSpec::DataType::SINT16: X(GL_RGBA16I);
                case ImageTypeSpec::DataType::UINT32: X(GL_RGBA32UI);
                case ImageTypeSpec::DataType::SINT32: X(GL_RGBA32I);
                default: break;
            }
        }
    } else {
        const bool allowLossy = true;
        #define LOSSY(x) assert(allowLossy && #x); X(x)

        if (spec.channels == 1) {
            switch (spec.dataType) {
                case ImageTypeSpec::DataType::FLOAT32: X(GL_R32F);
                case ImageTypeSpec::DataType::UINT8: X(GL_R8);
                case ImageTypeSpec::DataType::SINT8: X(GL_R8_SNORM);
            #ifdef IS_OPENGL_ES
                case ImageTypeSpec::DataType::UINT16: LOSSY(GL_R16F);
                case ImageTypeSpec::DataType::SINT16: LOSSY(GL_R16F);
            #else
                case ImageTypeSpec::DataType::UINT16: X(GL_R16);
                case ImageTypeSpec::DataType::SINT16: X(GL_R16_SNORM);
            #endif
                case ImageTypeSpec::DataType::UINT32: LOSSY(GL_R32F);
                case ImageTypeSpec::DataType::SINT32: LOSSY(GL_R32F);
                default: break;
            }
        }
        else if (spec.channels == 2) {
            switch (spec.dataType) {
                case ImageTypeSpec::DataType::FLOAT32: X(GL_RG32F);
                case ImageTypeSpec::DataType::UINT8: X(GL_RG8);
                case ImageTypeSpec::DataType::SINT8: X(GL_RG8_SNORM);
            #ifdef IS_OPENGL_ES
                case ImageTypeSpec::DataType::UINT16: LOSSY(GL_RG16F);
                case ImageTypeSpec::DataType::SINT16: LOSSY(GL_RG16F);
            #else
                case ImageTypeSpec::DataType::UINT16: X(GL_RG16);
                case ImageTypeSpec::DataType::SINT16: X(GL_RG16_SNORM);
            #endif
                case ImageTypeSpec::DataType::UINT32: LOSSY(GL_RG32F);
                case ImageTypeSpec::DataType::SINT32: LOSSY(GL_RG32F);
                default: break;
            }
        }
        else if (spec.channels == 3) {
            switch (spec.dataType) {
                case ImageTypeSpec::DataType::FLOAT32: X(GL_RGB32F);
                case ImageTypeSpec::DataType::UINT8: X(GL_RGB8);
                case ImageTypeSpec::DataType::SINT8: X(GL_RGB8_SNORM);
            #ifdef IS_OPENGL_ES
                case ImageTypeSpec::DataType::UINT16: LOSSY(GL_RGB16F);
                case ImageTypeSpec::DataType::SINT16: LOSSY(GL_RGB16F);
            #else
                case ImageTypeSpec::DataType::UINT16: X(GL_RGB16);
                case ImageTypeSpec::DataType::SINT16: X(GL_RGB16_SNORM);
            #endif
                case ImageTypeSpec::DataType::UINT32: LOSSY(GL_RGB32F);
                case ImageTypeSpec::DataType::SINT32: LOSSY(GL_RGB32F);
                default: break;
            }
        }
        else if (spec.channels == 4) {
            switch (spec.dataType) {
                case ImageTypeSpec::DataType::FLOAT32: X(GL_RGBA32F);
                case ImageTypeSpec::DataType::UINT8: X(GL_RGBA8);
                case ImageTypeSpec::DataType::SINT8: X(GL_RGBA8_SNORM);
            #ifdef IS_OPENGL_ES
                case ImageTypeSpec::DataType::UINT16: LOSSY(GL_RGBA16F);
                case ImageTypeSpec::DataType::SINT16: LOSSY(GL_RGBA16F);
            #else
                case ImageTypeSpec::DataType::UINT16: X(GL_RGBA16);
                case ImageTypeSpec::DataType::SINT16: X(GL_RGBA16_SNORM);
            #endif
                case ImageTypeSpec::DataType::UINT32: LOSSY(GL_RGBA32F);
                case ImageTypeSpec::DataType::SINT32: LOSSY(GL_RGBA32F);
                default: break;
            }
        }

        #undef LOSSY
        assert(false && "no suitable internal format");
    }
    #undef X
    assert(false);
    return -1;
}

int getCpuFormat(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getCpuFormat:%s", #x); return x
    if (useGlIntegerFormats && spec.dataType != ImageTypeSpec::DataType::FLOAT32) {
        switch (spec.channels) {
            case 1: X(GL_RED_INTEGER);
            case 2: X(GL_RG_INTEGER);
            case 3: X(GL_RGB_INTEGER);
            case 4: X(GL_RGBA_INTEGER);
            default: break;
        }
    } else {
        switch (spec.channels) {
            case 1: X(GL_RED);
            case 2: X(GL_RG);
            case 3: X(GL_RGB);
            case 4: X(GL_RGBA);
            default: break;
        }
    }
    #undef X
    assert(false);
    return -1;
}

int getReadPixelFormat(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getReadPixelFormat:%s", #x); return x
    switch (spec.channels) {
        case 1: X(GL_RED);
        case 2:
            log_warn("OpenGL spec does not allow reading 2-channel directly");
            X(GL_RG);
        case 3: X(GL_RGB);
        case 4: X(GL_RGBA);
        default: break;
    }
    #undef X
    assert(false);
    return -1;
}

int getCpuType(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getCpuType:%s", #x); return x
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

/**
 * Ensures an OpenGL flag is in the given state and returns it to its
 * original state afterwards
 */
template <GLuint flag, bool targetState> class GlFlagSetter {
private:
    const bool origState;

    void logChange(bool state) {
        (void)state;
        LOG_TRACE("%s GL flag 0x%x (target state %s)",
            state ? "enabling" : "disabling", flag,
            targetState ? "enabled" : "disabled");
    }

public:
    GlFlagSetter() : origState(glIsEnabled(flag)) {
        if (origState != targetState) {
            logChange(targetState);
            if (targetState) glEnable(flag);
            else glDisable(flag);
        }
    }

    ~GlFlagSetter() {
        if (origState != targetState) {
            logChange(origState);
            if (origState) glEnable(flag);
            else glDisable(flag);
        }
    }
};

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
            getCpuFormat(spec),
            getCpuType(spec), nullptr);

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
        // NOTE/TODO: to be most "correct" this should not assume that no
        // texture was bound before bind() was called but restore the one
        // that was.
        // See: https://www.khronos.org/opengl/wiki/Common_Mistakes

        // However there is a little practical benefit in making this work
        // optimally in the middle of any other OpenGL processing. Usually
        // whatever other operation cares about the bound texture state will
        // just overwrite this anyway.
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

    int getWidth() const final { return width; }
    int getHeight() const final { return height; }

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

    void setViewport() final {
        LOG_TRACE("glViewport(0, 0, %d, %d)", width, height);
        glViewport(0, 0, width, height);
        checkError(__FUNCTION__);
    }

    void readPixels(uint8_t *pixels) final {
        LOG_TRACE("reading frame buffer %d", id);
        Binder binder(*this);
        // Note: OpenGL ES only supports GL_RGBA / GL_UNSIGNED_BYTE (in practice)
        // Note: check this
        // https://www.khronos.org/opengl/wiki/Common_Mistakes#Slow_pixel_transfer_performance
        glReadPixels(0, 0, width, height, getReadPixelFormat(spec), getCpuType(spec), pixels);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        checkError(__FUNCTION__);
    }

    void writePixels(const uint8_t *pixels) final {
        Binder binder(texture);

        glTexImage2D(GL_TEXTURE_2D, 0,
            getInternalFormat(spec),
            width, height, 0,
            getCpuFormat(spec),
            getCpuType(spec),
            pixels);
        checkError(__FUNCTION__);
    }

    int getId() const { return id; }

    int getTextureId() const final {
        return texture.getId();
    }
};

static GLuint loadShader(GLenum shaderType, const char* shaderSource) {
    const GLuint shader = glCreateShader(shaderType);
    assert(shader);

    LOG_TRACE("compiling shader:\n %s\n", shaderSource);

    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLint len = 0;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        assert(len);

        std::vector<char> buf(static_cast<std::size_t>(len));
        glGetShaderInfoLog(shader, len, nullptr, buf.data());
        log_error("Error compiling shader:\n%s", buf.data());
        log_error("Failing shader source:\n%s", shaderSource);
        glDeleteShader(shader);
        assert(false);
    }

    return shader;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
    const GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
    const GLuint program = glCreateProgram();
    assert(program);
    glAttachShader(program, vertexShader);
    checkError("glAttachShader");
    glAttachShader(program, fragmentShader);
    checkError("glAttachShader");
    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        GLint bufLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
        if (bufLength) {
            std::vector<char> buf(static_cast<std::size_t>(bufLength));
            glGetProgramInfoLog(program, bufLength, nullptr, buf.data());
            log_error("Could not link program:\n%s", buf.data());
        }
        glDeleteProgram(program);
        assert(false);
    }
    return program;
}

class GlslProgramImplementation : public GlslProgram {
private:
    GLuint program;

public:
    GlslProgramImplementation(const char *vs, const char *fs)
    : program(createProgram(vs, fs)) {}

    int getId() const final { return program; }

    void bind() final {
        LOG_TRACE("activating shader: glUseProgram(%d)", program);
        glUseProgram(program);
    }

    void unbind() final {
        LOG_TRACE("deactivating shader: glUseProgram(0)", program);
        glUseProgram(0);
    }

    void destroy() final {
        if (program != 0) {
            LOG_TRACE("deleting GL program %d", program);
            glDeleteProgram(program);
            program = 0;
        }
    }

    ~GlslProgramImplementation() {
        if (program != 0) {
            log_warn("leaking GL program %d", program);
        }
    }
};

class GlslFragmentShaderImplementation : public GlslFragmentShader {
private:
    GLuint vertexBuffer = 0, vertexIndexBuffer = 0;
    GLuint aVertexData = 0;
    GlslProgramImplementation program;

    static std::string vertexShaderSource(bool withTexCoord) {
        const char *varyingTexCoordName = "v_texCoord";

        std::ostringstream oss;
        oss << R"(
            precision highp float;
            attribute vec4 a_vertexData;
        )";

        if (withTexCoord) {
            oss << "varying vec2 " << varyingTexCoordName << ";\n";
        }

        oss << "void main() {\n";

        if (withTexCoord) {
            oss << varyingTexCoordName << " = a_vertexData.zw;\n";
        }

        oss << "gl_Position = vec4(a_vertexData.xy, 0, 1);\n";
        oss << "}\n";

        return oss.str();
    }

public:
    GlslFragmentShaderImplementation(const char *fragementShaderSource, bool withTexCoord = true)
    : program(vertexShaderSource(withTexCoord).c_str(), fragementShaderSource)
    {
        glGenBuffers(1, &vertexBuffer);
        glGenBuffers(1, &vertexIndexBuffer);

        // Set up vertices
        float vertexData[] {
                // x, y, u, v
                -1, -1, 0, 0,
                -1, 1, 0, 1,
                1, 1, 1, 1,
                1, -1, 1, 0
        };
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Set up indices
        GLuint indices[] { 2, 1, 0, 0, 3, 2 };
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        aVertexData = glGetAttribLocation(program.getId(), "a_vertexData");
    }

    void destroy() final {
        if (vertexBuffer != 0) {
            glDeleteBuffers(1, &vertexBuffer);
            glDeleteBuffers(1, &vertexIndexBuffer);
        }
        program.destroy();
    }

    // if this leaks, then the "program" member also leaks, which produces
    // a warning so this can be noticed without customizing the dtor here

    void bind() final {
        program.bind();

        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexIndexBuffer);
        checkError(std::string(__FUNCTION__) + "/glBindBuffer x 2");

        glEnableVertexAttribArray(aVertexData);
        glVertexAttribPointer(aVertexData, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
        checkError(std::string(__FUNCTION__) + "/glVertexAttribPointer(aVertexData, ...)");
    }

    void unbind() final {
        glDisableVertexAttribArray(aVertexData);
        checkError(std::string(__FUNCTION__) + "/glDisableVertexAttribArray x 2");

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        checkError(std::string(__FUNCTION__) + "/glBindBuffer x 2");

        program.unbind();
    }

    void call(FrameBuffer &frameBuffer) final {
        // might typically be enabled, thus checking
        GlFlagSetter<GL_DEPTH_TEST, false> noDepthTest;
        GlFlagSetter<GL_BLEND, false> noBlend;
        // GlFlagSetter<GL_ALPHA_TEST, false> noAlphaTest;

        Binder frameBufferBinder(frameBuffer);
        frameBuffer.setViewport();

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        checkError(__FUNCTION__);
    }

    int getId() const final {
        return program.getId();
    }
};

class TextureUniformBinder : public Binder::Target {
private:
    const unsigned slot;
    const GLuint bindType, uniformId, sizeUniformId;
    int textureId = -1, width = -1, height = -1;

public:
    TextureUniformBinder(unsigned slot, GLuint bindType, GLuint uniformId, GLuint sizeUniformId = 0)
    : slot(slot), bindType(bindType), uniformId(uniformId), sizeUniformId(sizeUniformId) {
        LOG_TRACE("got texture uniform %d & size uniform %d for slot %u", uniformId, sizeUniformId, slot);
    }

    void bind() final {
        LOG_TRACE("bind texture / unfiform at slot %u -> %d", slot, textureId);
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(bindType, textureId);
        glUniform1i(uniformId, slot);
        if (sizeUniformId != 0) {
            LOG_TRACE("setting texture size uniform to %d x %d", width, height);
            glUniform2f(sizeUniformId, width, height);
        }
    }

    void unbind() final {
        LOG_TRACE("unbind texture / unfiform at slot %u", slot);
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(bindType, 0);
        // restore active texture to the default slot
        glActiveTexture(GL_TEXTURE0);
    }

    TextureUniformBinder &setTextureIdAndSize(int id, int w, int h) {
        textureId = id;
        width = w;
        height = h;
        return *this;
    }
};

class GlslPipelineImplementation : public GlslPipeline {
private:
    const unsigned nTextures;
    const GLuint bindType;
    GLuint outSizeUniform;
    GlslFragmentShaderImplementation program;
    std::vector<TextureUniformBinder> textureBinders;

    std::string textureName(unsigned index) const {
        std::ostringstream oss;
        assert(index < nTextures);
        oss << "u_texture";
        if (nTextures >= 2 || index > 1) {
            oss << (index + 1);
        }
        return oss.str();
    }

    std::string textureSizeName(unsigned index) const {
        return textureName(index) + "Size";
    }

    static std::string outSizeName() {
        return "u_outSize";
    }

    std::string buildShaderSource(const char *fragmentMain, bool withSizes) const {
        std::ostringstream oss;
        std::string texUniformType = "sampler2D";
        if (bindType == GL_TEXTURE_EXTERNAL_OES) {
            oss << "#extension GL_OES_EGL_image_external : require\n";
            texUniformType = "samplerExternalOES";
        }
        oss << "precision mediump float;\n";
        for (unsigned i = 0; i < nTextures; ++i) {
            std::string texName = textureName(i);
            oss << "uniform " << texUniformType << " " << textureName(i) << ";\n";
            if (withSizes) oss << "uniform vec2 " << textureSizeName(i) << ";\n";
        }
        if (withSizes) oss << "uniform vec2 " << outSizeName() << ";\n";
        oss << "varying vec2 v_texCoord;\n";
        oss << fragmentMain;
        oss << std::endl;
        return oss.str();
    }

public:
    GlslPipelineImplementation(const char *fragmentMain, unsigned nTextures, GLuint bindType, bool withSizes = true)
    :
        nTextures(nTextures),
        bindType(bindType),
        outSizeUniform(0),
        program(buildShaderSource(fragmentMain, withSizes).c_str(), bindType != 0)
    {
        if (withSizes) {
            outSizeUniform = glGetUniformLocation(program.getId(), outSizeName().c_str());
        }
        for (unsigned i = 0; i < nTextures; ++i) {
            textureBinders.push_back(TextureUniformBinder(
                i, bindType,
                glGetUniformLocation(program.getId(), textureName(i).c_str()),
                withSizes ? glGetUniformLocation(program.getId(), textureSizeName(i).c_str()) : 0
            ));
        }
        checkError(__FUNCTION__);
    }

    Binder::Target &bindTexture(unsigned index, int textureId, int w, int h) final {
        return textureBinders.at(index).setTextureIdAndSize(textureId, w, h);
    }

    void call(FrameBuffer &frameBuffer) final {
        if (outSizeUniform) {
            LOG_TRACE("setting out size uniform");
            glUniform2f(outSizeUniform, frameBuffer.getWidth(), frameBuffer.getHeight());
        }
        checkError(__FUNCTION__);
        program.call(frameBuffer);
    }

    void destroy() final { program.destroy(); }
    void bind() final { program.bind(); }
    void unbind() final { program.unbind(); }
    int getId() const final { return program.getId(); }
};

}

Binder::Binder(Target &target) : target(target) { target.bind(); }
Binder::~Binder() { target.unbind(); }
Destroyable::~Destroyable() = default;

std::unique_ptr<Texture> Texture::create(int w, int h, const ImageTypeSpec &spec) {
    return std::unique_ptr<Texture>(new TextureImplementation(w, h, spec));
}

std::unique_ptr<FrameBuffer> FrameBuffer::create(int w, int h, const ImageTypeSpec &spec) {
    return std::unique_ptr<FrameBuffer>(new FrameBufferImplementation(w, h, spec));
}

std::unique_ptr<GlslProgram> GlslProgram::create(const char *vs, const char *fs) {
    return std::unique_ptr<GlslProgram>(new GlslProgramImplementation(vs, fs));
}

std::unique_ptr<GlslFragmentShader> GlslFragmentShader::create(const char *fragementShaderSource) {
    return std::unique_ptr<GlslFragmentShader>(new GlslFragmentShaderImplementation(fragementShaderSource));
}

std::unique_ptr<GlslPipeline> GlslPipeline::create(unsigned nTextures, const char *fragmentMain, bool withSizes) {
    return std::unique_ptr<GlslPipeline>(new GlslPipelineImplementation(fragmentMain, nTextures, GL_TEXTURE_2D, withSizes));
}

std::unique_ptr<GlslPipeline> GlslPipeline::createWithExternalTexture(const char *fragmentMain) {
    return std::unique_ptr<GlslPipeline>(new GlslPipelineImplementation(fragmentMain, 1, GL_TEXTURE_EXTERNAL_OES));
}

std::unique_ptr<GlslPipeline> GlslPipeline::createWithoutTexCoords(const char *fragmentMain) {
    return std::unique_ptr<GlslPipeline>(new GlslPipelineImplementation(fragmentMain, 0, 0, false));
}

}
}
