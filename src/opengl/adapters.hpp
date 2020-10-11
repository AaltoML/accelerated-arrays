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
#include <GL/gl.h>
#endif

// logging
#ifndef log_error
#include <cstdio>
// ## is a "gcc" hack for allowing empty __VA_ARGS__
// https://stackoverflow.com/questions/5891221/variadic-macros-with-zero-arguments
#define log_debug(...) do { std::printf("DEBUG: "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
#define log_info(...) do { std::printf("INFO: "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
#define log_warn(...) do { std::fprintf(stderr, "WARN: "); std::fprintf(stderr, ## __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#define log_error(...) do { std::fprintf(stderr, "ERROR: "); std::fprintf(stderr, ## __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#endif

//#define ACCELERATED_ARRAYS_LOG_TRACE
#ifdef ACCELERATED_ARRAYS_LOG_TRACE
#define LOG_TRACE(...) log_debug(__FILE__ ": " __VA_ARGS__)
#else
#define LOG_TRACE(...) (void)0
#endif

namespace accelerated {
struct ImageTypeSpec;

namespace opengl {
void checkError(const char *tag);
int getTextureInternalFormat(const ImageTypeSpec &spec);
int getCpuFormat(const ImageTypeSpec &spec);
int getReadPixelFormat(const ImageTypeSpec &spec);
int getCpuType(const ImageTypeSpec &spec);
int getBindType(const ImageTypeSpec &spec);
std::string getGlslSamplerType(const ImageTypeSpec &spec);
std::string getGlslScalarType(const ImageTypeSpec &spec);
std::string getGlslVecType(const ImageTypeSpec &spec);

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

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;

    // these bind the frame buffer automatically
    virtual void readPixels(uint8_t *pixels) = 0;
    virtual void writePixels(const uint8_t *pixels) = 0;

    /** set glViewport to the full extent of this buffer */
    virtual void setViewport() = 0;

    virtual int getId() const = 0;
    virtual int getTextureId() const = 0;

};

struct GlslProgram : Destroyable, Binder::Target {
    static std::unique_ptr<GlslProgram> create(
        const char *vertexShaderSource,
        const char *fragmentShaderSource);

    virtual int getId() const = 0;
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
    static std::unique_ptr<GlslPipeline> create(const char *fragmentMain, const std::vector<ImageTypeSpec> &inputs, const ImageTypeSpec &output);
    virtual Binder::Target &bindTexture(unsigned index, int textureId) = 0;
};

}
}
