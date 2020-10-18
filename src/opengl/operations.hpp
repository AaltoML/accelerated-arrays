#pragma once
#include "../standard_ops.hpp"

namespace accelerated {
struct Processor;
namespace opengl {
class Image;
struct Destroyable;

namespace operations {
typedef std::function< void(Image **inputs, int nInputs, Image &output) > NAry;
typedef std::function< void(Image &output) > Nullary;
typedef std::function< void(Image &input, Image &output) > Unary;
typedef std::function< void(Image &a, Image &b, Image &output) > Binary;

template <class F> struct Shader {
    /** Will be invoked in the GL thread */
    F function;
    /**
     * Something that can be properly cleaned up by calling ->destroy() in
     * the GL thread. Typically the GL shader program and associated stuff
     */
    std::unique_ptr<Destroyable> resources;

    typedef std::function< std::unique_ptr<Shader<F>>() > Builder;
};

class Factory : public ::accelerated::operations::StandardFactory {
public:
    /**
     * The builder function is called on the GL thread. The things in the
     * shader object it creates will also be accessed / called in the GL
     * thread.
     */
    virtual ::accelerated::operations::Function wrapNAry(const Shader<NAry>::Builder &builder) = 0;

    template <class T> ::accelerated::operations::Function wrap(const typename Shader<T>::Builder &builder) {
        return wrapNAry(convertToNAry<T>(builder));
    }

    virtual ::accelerated::operations::Function wrapShader(
        const std::string &fragmentShaderBody,
        const std::vector<ImageTypeSpec> &inputs,
        const ImageTypeSpec &output) = 0;

    virtual void debugLogShaders(bool enabled) = 0;

private:
    template <class T> static Shader<NAry>::Builder convertToNAry(const typename Shader<T>::Builder &otherAryBuilder) {
        return [otherAryBuilder]() {
           auto otherAry = otherAryBuilder();
           std::unique_ptr< Shader<NAry> > r(new Shader<NAry>());
           r->resources = std::move(otherAry->resources);
           r->function = ::accelerated::operations::sync::convert(otherAry->function);
           return r;
       };
    }
};

std::unique_ptr<Factory> createFactory(Processor &processor);
}

enum class GLFWProcessorMode {
    /** Prefer ASYNC but fall back to SYNC if that's not available (on Mac) */
    AUTO,
    /** Execute all (OpenGL) commands instantly on the calling thread */
    SYNC,
    /** Execute all (OpenGL) commands on their own worker thread */
    ASYNC
};

/**
 * Create a processor with a (hidden) window and GL context using the GLFW
 * library. Not available on mobile, where one should use
 * Processor::createQueue() and call its processAll method in the existing
 * OpenGL thread / onDraw function manually.
 *
 * By default, executes the commands in its own worker thread, but may
 * fall back to executing them instantly if that's not possible (on Mac),
 * see GLFWProcessorMode for more details.
 */
std::unique_ptr<Processor> createGLFWProcessor(GLFWProcessorMode mode = GLFWProcessorMode::AUTO);


/**
 * Same as createGLFWProcessor but with a visible window of given size
 * and title. To draw to the window, first create an opengl::Image::Factory,
 * then obtain a screen frame buffer reference with factory->wrapScreen,
 * create an operation that writes something to it, and finally call
 * processor->enqueue([]() -> { glfwSwapBuffers(); });
 *
 * You may also want to use the optionally outputted GLFWwindow pointer
 * to check if the user has requested to close the window etc.
 */
std::unique_ptr<Processor> createGLFWWindow(int w, int h,
    const char *title = nullptr,
    GLFWProcessorMode mode = GLFWProcessorMode::AUTO,
    void **glfwWindowOutput = nullptr);

}
}
