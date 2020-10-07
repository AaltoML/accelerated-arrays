#pragma once

#include <memory>

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
// NDK bug workaround: https://stackoverflow.com/a/31025110
#define __gl2_h_
#include <GLES2/gl2ext.h>

// logging
#ifndef log_error
#include <cstdio>
// ## is a "gcc" hack for allowing empty __VA_ARGS__
// https://stackoverflow.com/questions/5891221/variadic-macros-with-zero-arguments
#define log_debug(fmt, ...) ((void)printf(fmt" (debug)\n", ## __VA_ARGS__))
#define log_info(fmt, ...) ((void)printf(fmt" (info)\n", ## __VA_ARGS__))
#define log_warn(fmt, ...) ((void)printf(fmt" (warn)\n", ## __VA_ARGS__))
#define log_error(fmt, ...) ((void)printf(fmt" (error)\n", ## __VA_ARGS__))
#endif

namespace accelerated {
struct ImageTypeSpec;

namespace opengl {
void checkError(const char *tag);

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

struct FrameBuffer : Binder::Target {
    static std::unique_ptr<FrameBuffer> create(int w, int h, const ImageTypeSpec &spec);

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
     * destructor (~FrameBuffer) without calling destroy first, is a suitable
     * solution when quitting the program (or some GL-enabled view).
     */
    virtual void destroy() = 0;
    virtual ~FrameBuffer();

    // these bind the frame buffer automatically
    virtual void readPixels(uint8_t *pixels) = 0;
    virtual void writePixels(const uint8_t *pixels) = 0;

    // virtual int getId() const = 0;
    virtual int getTextureId() const = 0;

};

struct GlslProgram : Binder::Target {
    static std::unique_ptr<GlslProgram> create(
        const char *vertexShaderSource,
        const char *fragmentShaderSource);

    static std::unique_ptr<GlslProgram> create(
        const char *fragmentShaderSource);

    virtual void destroy() = 0;
    virtual ~GlslProgram();
    virtual int getId() const = 0;
};

}
}
