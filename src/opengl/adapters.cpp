#include <cassert>
#include <sstream>

#include "adapters.hpp"
#include "../image.hpp"
#include "../log.hpp"

#define _THING_AS_STRING(x) #x
#define _CHECK_ERROR_MARKER(line) __FILE__ ":" _THING_AS_STRING(line)
#define CHECK_ERROR(func) checkError(_CHECK_ERROR_MARKER(__LINE__), func)

namespace accelerated {
namespace opengl {
static void checkError(const char *tag, const char *tag2) {
    GLint error;
    bool any = false;
    while((error = glGetError())) {
        any = true;
        if (tag2 != nullptr) {
            log_error("%s (%s) produced glError (0x%x)", tag, tag2, error);
        } else {
            log_error("%s produced glError (0x%x)", tag, error);
        }
    }
    if (any) {
        std::abort();
    }
}

void checkError(const char *tag) { checkError(tag, nullptr); }

// could be exposed in the hpp file but not used currently elsewhere
struct Texture : Destroyable, Binder::Target {
    static std::unique_ptr<Texture> create(int w, int h, const ImageTypeSpec &spec);
    virtual int getId() const = 0;
};

namespace {

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
        glTexImage2D(bindType, 0,
            getTextureInternalFormat(spec),
            width, height, 0,
            getCpuFormat(spec),
            getCpuType(spec), nullptr);

        // set default texture parameters
        glTexParameteri(bindType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(bindType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexParameteri(bindType, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(bindType, GL_TEXTURE_WRAP_T, GL_REPEAT);

        CHECK_ERROR(__FUNCTION__);
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
        CHECK_ERROR(__FUNCTION__);
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
        CHECK_ERROR(__FUNCTION__);
    }

    int getId() const { return id; }
};

class FrameBufferImplementation : public FrameBuffer {
private:
    int width, height;
    ImageTypeSpec spec;
    int id;
    std::shared_ptr<Texture> texture;

    const struct Viewport {
        int x0, y0, width, height;
    } viewport;

    bool fullViewport() const {
        return viewport.x0 == 0 &&
            viewport.y0 == 0 &&
            viewport.width == width &&
            viewport.height == height;
    }

    bool isScreen() const {
        // hacky
        return id == 0;
    }

public:
    FrameBufferImplementation(int w, int h, const ImageTypeSpec &spec, int existingFboId = -1, const Viewport *viewportPtr = nullptr) :
        width(w), height(h), spec(spec), id(existingFboId),
        texture(existingFboId < 0 ? new TextureImplementation(w, h, spec) : nullptr),
        viewport(viewportPtr == nullptr ? Viewport { 0, 0, w, h } : *viewportPtr)
    {
        aa_assert(viewport.x0 >= 0 && viewport.y0 >= 0 && viewport.width <= width && viewport.height <= height);
        if (existingFboId >= 0) {
            LOG_TRACE("creating a reference to an existing frame buffer object %d", existingFboId);
        } else {
            GLuint genId;
            glGenFramebuffers(1, &genId);
            id = genId;
            LOG_TRACE("generated frame buffer %d", id);
            CHECK_ERROR(__FUNCTION__);

            aa_assert(spec.storageType == Image::StorageType::GPU_OPENGL);

            Binder binder(*this);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->getId(), 0);
            CHECK_ERROR(__FUNCTION__);
            aa_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            GLenum bufs[1] = { GL_COLOR_ATTACHMENT0 }; // single output at location 0
            glDrawBuffers(1, bufs);
            CHECK_ERROR(__FUNCTION__);
        }
    }

    void destroy() final {
        if (texture) {
            // this is a bit messy
            if (texture.unique()) {
                if (id != 0) {
                    LOG_TRACE("destroying frame buffer %d", id);
                    GLuint uid = id;
                    glDeleteFramebuffers(1, &uid);
                }
                id = 0;
                texture->destroy();
            } else {
                LOG_TRACE("not destroying shared texture %d", id);
                texture.reset();
            }
        } else {
            LOG_TRACE("not destroying external frame buffer %d", id);
        }
    }

    int getViewportWidth() const final { return viewport.width; }
    int getViewportHeight() const final { return viewport.height; }

    ~FrameBufferImplementation() {
        if (texture && texture.unique() && id != 0) {
            log_warn("leaking frame buffer %d", id);
        }
    }

    std::unique_ptr<FrameBuffer> createROI(int x0, int y0, int w, int h) final {
        Viewport view { x0, y0, w, h };
        LOG_TRACE("creating a ROI from frame buffer %d", id);
        auto *r = new FrameBufferImplementation(w, h, spec, id, &view);
        r->texture = texture;
        return std::unique_ptr<FrameBuffer>(r);
    }

    void bind() final {
        // texture.bind();
        LOG_TRACE("bound frame buffer %d", id);
        glBindFramebuffer(GL_FRAMEBUFFER, id);
        CHECK_ERROR(__FUNCTION__);
    }

    void unbind() final {
        if (isScreen()) return; // skip, already bound 0

        LOG_TRACE("unbound frame buffer");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // texture.unbind();
        CHECK_ERROR(__FUNCTION__);
    }

    void setViewport() final {
        LOG_TRACE("glViewport(%d, %d, %d, %d)", viewport.x0, viewport.y0, viewport.width, viewport.height);
        glViewport(viewport.x0, viewport.y0, viewport.width, viewport.height);
        CHECK_ERROR(__FUNCTION__);
    }

    void readPixels(uint8_t *pixels) final {
        LOG_TRACE("reading frame buffer %d");
        Binder binder(*this);

        if (isScreen()) {
            LOG_TRACE("reading screen");
            glReadBuffer(GL_BACK);
        } else {
            // probably not changed but good to set anyway
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            CHECK_ERROR(__FUNCTION__);
        }

        // our CPU data is tightly packed and not 4-byte aligned (default)
        GLint origPackAlignment;
        glGetIntegerv(GL_PACK_ALIGNMENT, &origPackAlignment);
        aa_assert(origPackAlignment >= 1 && origPackAlignment <= 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        CHECK_ERROR(__FUNCTION__);

        // Note: check this
        // https://www.khronos.org/opengl/wiki/Common_Mistakes#Slow_pixel_transfer_performance
        glReadPixels(viewport.x0, viewport.y0, viewport.width, viewport.height, getReadPixelFormat(spec), getCpuType(spec), pixels);

        if (!isScreen()) {
            aa_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            CHECK_ERROR(__FUNCTION__);
        }

        glPixelStorei(GL_PACK_ALIGNMENT, origPackAlignment);
        CHECK_ERROR(__FUNCTION__);
    }

    void writePixels(const uint8_t *pixels) final {
        aa_assert(!isScreen() && "won't write pixels directly to screen");
        aa_assert(texture && "won't write directly to external frame buffer");

        Binder binder(*texture);

        // our CPU data is tightly packed and not 4-byte aligned (default)
        GLint origPackAlignment;
        glGetIntegerv(GL_PACK_ALIGNMENT, &origPackAlignment);
        aa_assert(origPackAlignment >= 1 && origPackAlignment <= 4);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        CHECK_ERROR(__FUNCTION__);

        if (fullViewport()) {
            glTexImage2D(GL_TEXTURE_2D, 0,
                getTextureInternalFormat(spec),
                width, height, 0,
                getCpuFormat(spec),
                getCpuType(spec),
                pixels);
        } else {
            LOG_TRACE("writing a sub image of frame buffer %d", id);
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                viewport.x0, viewport.y0,
                viewport.width, viewport.height,
                getCpuFormat(spec),
                getCpuType(spec),
                pixels);
        }

        CHECK_ERROR(__FUNCTION__);

        glPixelStorei(GL_PACK_ALIGNMENT, origPackAlignment);
        CHECK_ERROR(__FUNCTION__);
    }

    int getId() const { return id; }

    int getTextureId() const final {
        aa_assert(texture && "cannot get texture ID of external frame buffer");
        aa_assert(fullViewport() && "cannot use ROI as a texture");
        return texture->getId();
    }
};

static GLuint loadShader(GLenum shaderType, const char* shaderSource) {
    const GLuint shader = glCreateShader(shaderType);
    aa_assert(shader);

    //#define ACCELERATED_ARRAYS_DEBUG_PRINT_ALL_SHADERS
    #ifdef ACCELERATED_ARRAYS_DEBUG_PRINT_ALL_SHADERS
    log_debug("compiling shader:\n %s\n", shaderSource);
    #else
    LOG_TRACE("compiling shader:\n %s\n", shaderSource);
    #endif

    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLint len = 0;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        aa_assert(len);

        std::vector<char> buf(static_cast<std::size_t>(len));
        glGetShaderInfoLog(shader, len, nullptr, buf.data());
        log_error("Error compiling shader:\n%s", buf.data());
        log_error("Failing shader source:\n%s", shaderSource);
        glDeleteShader(shader);
        aa_assert(false);
    }

    return shader;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
    const GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
    const GLuint program = glCreateProgram();
    aa_assert(program);
    glAttachShader(program, vertexShader);
    CHECK_ERROR(__FUNCTION__);
    glAttachShader(program, fragmentShader);
    CHECK_ERROR(__FUNCTION__);
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
        aa_assert(false);
    }
    return program;
}

class GlslProgramImplementation : public GlslProgram {
private:
    std::string vertSrc, fragSrc;
    GLuint program;

public:
    GlslProgramImplementation(const char *vs, const char *fs) :
        vertSrc(vs), fragSrc(fs),
        program(createProgram(vertSrc.c_str(), fragSrc.c_str()))
    {}

    int getId() const final { return program; }

    void bind() final {
        LOG_TRACE("activating shader: glUseProgram(%d)", program);
        glUseProgram(program);
    }

    void unbind() final {
        LOG_TRACE("deactivating shader: glUseProgram(0)");
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

    std::string getFragmentShaderSource() const {
        return fragSrc;
    }

    std::string getVertexShaderSource() const {
        return vertSrc;
    }
};

class GlslFragmentShaderImplementation : public GlslFragmentShader {
private:
    GLuint vertexBuffer = 0, vertexIndexBuffer = 0;
    GLuint aVertexData = 0;
    GLuint vao = 0;
    GlslProgramImplementation program;

    static std::string vertexShaderSource(bool withTexCoord) {
        const char *varyingTexCoordName = "v_texCoord";

        std::ostringstream oss;
        #ifdef __APPLE__
            oss << "#version 330\n";
        #else
            oss << "#version 300 es\n";
        #endif // __APPLE__
        oss << "precision highp float;\n";
        oss << "in vec4 a_vertexData;\n";
        if (withTexCoord) {
            oss << "out vec2 " << varyingTexCoordName << ";\n";
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
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

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

        // Set up indices
        GLuint indices[] { 2, 1, 0, 0, 3, 2 };
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // Unbind evrything
        glBindVertexArray(0); // Has to happen before unbinding other buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0);
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

        glBindVertexArray(vao);
        CHECK_ERROR(__FUNCTION__);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexIndexBuffer);
        CHECK_ERROR(__FUNCTION__);

        glEnableVertexAttribArray(aVertexData);
        glVertexAttribPointer(aVertexData, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
        CHECK_ERROR(__FUNCTION__);
    }

    void unbind() final {
        glDisableVertexAttribArray(aVertexData);
        CHECK_ERROR(__FUNCTION__);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_ERROR(__FUNCTION__);
        glBindVertexArray(0);
        CHECK_ERROR(__FUNCTION__);

        program.unbind();
    }

    void call(FrameBuffer &frameBuffer) final {
        LOG_TRACE("call with frame buffer %d", frameBuffer.getId());
        // might typically be enabled, thus checking
        GlFlagSetter<GL_DEPTH_TEST, false> noDepthTest;
        GlFlagSetter<GL_BLEND, false> noBlend;
        // GlFlagSetter<GL_ALPHA_TEST, false> noAlphaTest;

        Binder frameBufferBinder(frameBuffer);
        frameBuffer.setViewport();

        if (frameBuffer.getId() == 0) {
            #ifndef ACCELERATED_ARRAYS_USE_OPENGL_ES
                // probably not changed, but good to set explicitly
                GLint origDrawBuffer;
                glGetIntegerv(GL_DRAW_BUFFER, &origDrawBuffer);
                CHECK_ERROR(__FUNCTION__);
                glDrawBuffer(GL_BACK);
            #endif
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            #ifndef ACCELERATED_ARRAYS_USE_OPENGL_ES
                glDrawBuffer(origDrawBuffer);
            #endif
        } else {
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        CHECK_ERROR(__FUNCTION__);
    }

    int getId() const final {
        return program.getId();
    }

    std::string getFragmentShaderSource() const {
        return program.getFragmentShaderSource();
    }

    std::string getVertexShaderSource() const {
        return program.getVertexShaderSource();
    }
};

class TextureUniformBinder : public Binder::Target {
public:
    const unsigned slot;
    const GLuint bindType, uniformId;
    int textureId = -1;
    Image::Border border = Image::Border::UNDEFINED;
    Image::Interpolation interpolation = Image::Interpolation::NEAREST;

    TextureUniformBinder(unsigned slot, GLuint bindType, GLuint uniformId)
    : slot(slot), bindType(bindType), uniformId(uniformId) {
        LOG_TRACE("got texture uniform %d for slot %u", uniformId, slot);
    }

    void bind() final {
        LOG_TRACE("bind texture / uniform at slot %u -> %d", slot, textureId);
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(bindType, textureId);

        const int interpType = getGlInterpType();
        if (interpType != 0) {
            LOG_TRACE("set texture interpolation 0x%x", interpType);
            glTexParameteri(bindType, GL_TEXTURE_MAG_FILTER, interpType);
            glTexParameteri(bindType, GL_TEXTURE_MIN_FILTER, interpType);
        }
        const int borderType = getGlBorderType();
        if (borderType != 0) {
            LOG_TRACE("set border type 0x%x", borderType);
            glTexParameteri(bindType, GL_TEXTURE_WRAP_S, borderType);
            glTexParameteri(bindType, GL_TEXTURE_WRAP_T, borderType);
        }

        glUniform1i(uniformId, slot);
    }

    void unbind() final {
        LOG_TRACE("unbind texture / uniform at slot %u", slot);
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(bindType, 0);
        // restore active texture to the default slot
        glActiveTexture(GL_TEXTURE0);
    }

    // avoid clang warning
    virtual ~TextureUniformBinder() = default;

private:
    int getGlBorderType() const {
        switch (border) {
            case Image::Border::UNDEFINED: return 0;
            case Image::Border::ZERO:
            #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
                aa_assert(false && "GL_CLAMP_TO_BORDER is not supported in OpenGL ES");
            #else
                return GL_CLAMP_TO_BORDER; // TODO: check
            #endif
            case Image::Border::REPEAT: return GL_REPEAT;
            case Image::Border::MIRROR: return GL_MIRRORED_REPEAT;
            case Image::Border::CLAMP: return GL_CLAMP_TO_EDGE;
        }
        aa_assert(false);
        return 0;
    }

    int getGlInterpType() const {
        switch (interpolation) {
            case Image::Interpolation::UNDEFINED: return 0;
            case Image::Interpolation::NEAREST: return GL_NEAREST;
            case Image::Interpolation::LINEAR: return GL_LINEAR;
        }
        aa_assert(false);
        return 0;
    }
};

class GlslPipelineImplementation : public GlslPipeline {
private:
    GLuint outSizeUniform;
    GlslFragmentShaderImplementation program;
    std::vector<TextureUniformBinder> textureBinders;

    std::string textureName(unsigned index, unsigned nTextures) const {
        std::ostringstream oss;
        aa_assert(index < nTextures);
        oss << "u_texture";
        if (nTextures >= 2 || index > 1) {
            oss << (index + 1);
        }
        return oss.str();
    }

    static std::string outSizeName() {
        return "u_outSize";
    }

    static bool hasExternal(const std::vector<ImageTypeSpec> &inputs) {
        for (const auto &in : inputs)
            if (getBindType(in) != GL_TEXTURE_2D) return true;
        return false;
    }

    std::string buildShaderSource(const char *fragmentMain, const std::vector<ImageTypeSpec> &inputs, const ImageTypeSpec &output) const {
        std::ostringstream oss;
        #ifdef __APPLE__
            oss << "#version 330\n";
        #else
            oss << "#version 300 es\n";
        #endif // __APPLE__
        if (hasExternal(inputs)) {
            oss << "#extension GL_OES_EGL_image_external_essl3 : require\n";
        }
        oss << "precision highp float;\n";
        oss << "layout(location = 0) out " << getGlslVecType(output) << " outValue;\n";

        for (std::size_t i = 0; i < inputs.size(); ++i) {
            oss << "uniform "
                << getGlslPrecision(inputs.at(i)) << " "
                << getGlslSamplerType(inputs.at(i)) << " "
                << textureName(i, inputs.size()) << ";\n";
        }

        oss << "uniform ivec2 " << outSizeName() << ";\n";
        oss << "in vec2 v_texCoord;\n";
        oss << fragmentMain;
        oss << std::endl;

        return oss.str();
    }

public:
    GlslPipelineImplementation(const char *fragmentMain, const std::vector<ImageTypeSpec> &inputs, const ImageTypeSpec &output)
    :
        outSizeUniform(0),
        program(buildShaderSource(fragmentMain, inputs, output).c_str())
    {
        outSizeUniform = glGetUniformLocation(program.getId(), outSizeName().c_str());
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            textureBinders.push_back(TextureUniformBinder(
                i,
                getBindType(inputs.at(i)),
                glGetUniformLocation(program.getId(), textureName(i, inputs.size()).c_str())
            ));
        }
        CHECK_ERROR(__FUNCTION__);

        if (ImageTypeSpec::isFixedPoint(output.dataType) && ImageTypeSpec::isSigned(output.dataType)) {
            // https://www.reddit.com/r/opengl/comments/bqe1jo/how_to_render_to_a_snorm_texture/
            #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
                // NOTE: it is possible to tolerate this but then it's possible
                // that all negative output values get clamped to 0
                log_warn("glClampColor not available in OpenGL ES so can't use SNORM render targets");

                #ifndef ACCELERATED_ARRAYS_DODGY_READS
                    aa_assert(false);
                #endif
            #else
                #if defined(__APPLE__)
                    log_error("glClampColor() and GL_CLAMP_FRAGMENT_COLOR not supported on MacOS");
                    assert(false);
                #else
                    log_warn("SNORM render target requires GL bug fixes only found on Reddit. Use with caution.");
                    //constexpr int GL_CLAMP_FRAGMENT_COLOR = 0x891B;
                    glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
                #endif
            #endif
        }
    }

    Binder::Target &bindTexture(unsigned index, int textureId) final {
        auto &binder = textureBinders.at(index);
        binder.textureId = textureId;
        return binder;
    }

    void setTextureInterpolation(unsigned index, ::accelerated::Image::Interpolation i) final {
        textureBinders.at(index).interpolation = i;
    }

    void setTextureBorder(unsigned index, ::accelerated::Image::Border b) final {
        textureBinders.at(index).border = b;
    }

    void call(FrameBuffer &frameBuffer) final {
        const int w = frameBuffer.getViewportWidth(), h =  frameBuffer.getViewportHeight();
        LOG_TRACE("setting out size uniform to %d x %d", w, h);
        glUniform2i(outSizeUniform, w, h);

        CHECK_ERROR(__FUNCTION__);
        program.call(frameBuffer);
    }

    void destroy() final { program.destroy(); }
    void bind() final { program.bind(); }
    void unbind() final { program.unbind(); }
    int getId() const final { return program.getId(); }
    std::string getFragmentShaderSource() const { return program.getFragmentShaderSource(); }
    std::string getVertexShaderSource() const { return program.getVertexShaderSource(); }
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

std::unique_ptr<FrameBuffer> FrameBuffer::createReference(int existingFboId, int w, int h, const ImageTypeSpec &spec) {
    return std::unique_ptr<FrameBuffer>(new FrameBufferImplementation(w, h, spec, existingFboId));
}

std::unique_ptr<FrameBuffer> FrameBuffer::createScreenReference(int w, int h) {
    auto spec = getScreenImageTypeSpec();
    // note: 0 is handled as a special case (hacky-ish)
    return createReference(0, w, h, *spec);
}

std::unique_ptr<GlslProgram> GlslProgram::create(const char *vs, const char *fs) {
    return std::unique_ptr<GlslProgram>(new GlslProgramImplementation(vs, fs));
}

std::unique_ptr<GlslFragmentShader> GlslFragmentShader::create(const char *fragementShaderSource) {
    return std::unique_ptr<GlslFragmentShader>(new GlslFragmentShaderImplementation(fragementShaderSource));
}

std::unique_ptr<GlslPipeline> GlslPipeline::create(const char *fragmentMain, const std::vector<ImageTypeSpec> &inputs, const ImageTypeSpec &output) {
    return std::unique_ptr<GlslPipeline>(new GlslPipelineImplementation(fragmentMain, inputs, output));
}

}
}
