#include "../standard_ops.hpp"

namespace accelerated {
class Processor;
namespace opengl {
class Image;
typedef std::function< void(Image &input, Image &output) > SyncUnary;

namespace operations {

class Factory : public ::accelerated::operations::StandardFactory {
    virtual ::accelerated::operations::Function wrap(const SyncUnary &func) = 0;
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
