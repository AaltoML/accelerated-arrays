#include "../standard_ops.hpp"

namespace accelerated {
class Processor;
namespace opengl {
class Image;
struct Destroyable;

namespace operations {
namespace sync {
typedef std::function< void(Image **inputs, int nInputs, Image &output) > NAry;
typedef std::function< void(Image &output) > Nullary;
typedef std::function< void(Image &input, Image &output) > Unary;
typedef std::function< void(Image &a, Image &b, Image &output) > Binary;
}

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
    virtual ::accelerated::operations::Function wrapNAry(const Shader<sync::NAry>::Builder &builder) = 0;
    virtual ::accelerated::operations::Function wrapNAryChecked(const Shader<sync::NAry>::Builder &builder, const ImageTypeSpec &spec) = 0;

    template <class T> ::accelerated::operations::Function wrap(const typename Shader<T>::Builder &builder) {
        return wrapNAry(convertToNAry<T>(builder));
    }

    template <class T> ::accelerated::operations::Function wrapChecked(const typename Shader<T>::Builder &builder, const ImageTypeSpec &spec) {
        return wrapNAryChecked(convertToNAry<T>(builder), spec);
    }

private:
    template <class T> static Shader<sync::NAry>::Builder convertToNAry(const typename Shader<T>::Builder &otherAryBuilder) {
        return [otherAryBuilder]() {
           auto otherAry = otherAryBuilder();
           std::unique_ptr< Shader<sync::NAry> > r(new Shader<sync::NAry>());
           r->resources = std::move(otherAry->resources);
           r->function = ::accelerated::operations::sync::convert(otherAry->function);
           return r;
       };
    }
};

std::unique_ptr<Factory> createFactory(Processor &processor);
}

/**
 * Create a processor with its own thread, (hidden) window and GL context
 * using the GLFW library. Not available on mobile, where one should use
 * Processor::createQueue() and call its processAll method in the existing
 * OpenGL thread / onDraw function manually.
 */
std::unique_ptr<Processor> createGLFWProcessor();
}
}
