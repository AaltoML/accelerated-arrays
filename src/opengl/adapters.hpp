#pragma once

#include <memory>
#include <vector>

#ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
    #include <GLES3/gl3.h>
    #include <GLES3/gl3ext.h>
    // NDK bug workaround: https://stackoverflow.com/a/31025110
    #define __gl2_h_
    #include <GLES2/gl2ext.h>
#else
    #define GL_GLEXT_PROTOTYPES // again just something from a random forum :(
    #ifdef __APPLE__
        #include <OpenGL/gl3.h>
        #include <OpenGL/gl3ext.h>
    #else
        #include <GL/gl.h>
    #endif // __APPLE__
#endif

#include "../image.hpp"

namespace accelerated {
namespace opengl {
void checkError(const char *tag);
int getTextureInternalFormat(const ImageTypeSpec &spec);
int getCpuFormat(const ImageTypeSpec &spec);
int getReadPixelFormat(const ImageTypeSpec &spec);
int getCpuType(const ImageTypeSpec &spec);
int getBindType(const ImageTypeSpec &spec);
std::string getGlslPrecision(const ImageTypeSpec &spec);
std::string getGlslSamplerType(const ImageTypeSpec &spec);
std::string getGlslScalarType(const ImageTypeSpec &spec);
std::string getGlslVecType(const ImageTypeSpec &spec);
std::unique_ptr<ImageTypeSpec> getScreenImageTypeSpec();

class Binder {
public:
    struct Target {
        virtual void bind() = 0;
        virtual void unbind() = 0;
    };

    Binder(Target &target);
    ~Binder();

private:
    Target &target;
};

/**
 * Resources can be freed in two different ways:
 *  1. calling the appropriate teardown methods like glDeleteFramebuffers
 *  2. abandoning the individual resources and just destroying the whole
 *     OpenGL context
 *
 * The tricky thing is that method 1 usually be done only from a certain
 * thread (the "OpenGL thread"), which may or may not do any processing
 * after the destrutors of these wrapper classes are called.
 *
 * Both destruction methods are thus useful. The graceful destroy
 * method (1) can be useful in long-running programs or then using truly
 * temporary resources. Method (2), which is equivalent to calling the
 * destructor (~Destroyable) without calling destroy first, is a suitable
 * solution when quitting the program (or some GL-enabled view).
 */
struct Destroyable {
    virtual void destroy() = 0;
    virtual ~Destroyable();
};

struct FrameBuffer : Destroyable, Binder::Target {
    static std::unique_ptr<FrameBuffer> create(int w, int h, const ImageTypeSpec &spec);
    static std::unique_ptr<FrameBuffer> createReference(int existingFboId, int w, int h, const ImageTypeSpec &spec);
    static std::unique_ptr<FrameBuffer> createScreenReference(int w, int h);

    virtual std::unique_ptr<FrameBuffer> createROI(int x0, int y0, int w, int h) = 0;

    virtual int getViewportWidth() const = 0;
    virtual int getViewportHeight() const = 0;

    // these bind the frame buffer automatically
    virtual void readPixels(uint8_t *pixels) = 0;
    virtual void writePixels(const uint8_t *pixels) = 0;

    /** set glViewport to the viewport defined for this frame buffer (reference) */
    virtual void setViewport() = 0;

    virtual int getId() const = 0;
    virtual int getTextureId() const = 0;

};

struct GlslProgram : Destroyable, Binder::Target {
    static std::unique_ptr<GlslProgram> create(
        const char *vertexShaderSource,
        const char *fragmentShaderSource);

    virtual int getId() const = 0;

    // useful for debugging
    virtual std::string getFragmentShaderSource() const = 0;
    virtual std::string getVertexShaderSource() const = 0;
};

/**
 * GlslProgram with the default vertex shader that renders a single
 * (full screen) rectangle
 */
struct GlslFragmentShader : GlslProgram {
    static std::unique_ptr<GlslFragmentShader> create(const char *fragmentShaderSource);
    virtual void call(FrameBuffer &fb) = 0;
};

/**
 * Default GlslFragmentShader with N input textures
 */
struct GlslPipeline : GlslFragmentShader {
    static std::unique_ptr<GlslPipeline> create(
        const char *fragmentMain,
        const std::vector<ImageTypeSpec> &inputs,
        const ImageTypeSpec &output);

    virtual Binder::Target &bindTexture(unsigned index, int textureId) = 0;

    // Note: different from how OpenGL works as the texture parameters are
    // part of the texture state, not the texture "slot / unit", but in this
    // library, it makes more sense to define the interpolation paramters
    // as part of the processing pipeline rather than the images themselves
    virtual void setTextureInterpolation(unsigned index, ::accelerated::Image::Interpolation i) = 0;
    virtual void setTextureBorder(unsigned index, ::accelerated::Image::Border b) = 0;
};

}
}
