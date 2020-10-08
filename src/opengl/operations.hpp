#include "../standard_ops.hpp"

namespace accelerated {
class Processor;
namespace opengl {
class Image;

namespace operations {
typedef std::function< void(Image **inputs, int nInputs, Image &output) > NAry;
typedef std::function< void(Image &output) > Nullary;
typedef std::function< void(Image &input, Image &output) > Unary;
typedef std::function< void(Image &a, Image &b, Image &output) > Binary;

class Factory : public ::accelerated::operations::StandardFactory {
public:
    virtual ::accelerated::operations::Function wrapNAry(const NAry &func) = 0;
    template <class T> ::accelerated::operations::Function wrap(const T &func) {
        return wrapNAry(::accelerated::operations::sync::convert(func));
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
