#include "../standard_ops.hpp"

namespace accelerated {
class Processor;
namespace opengl {
class Image;
class GlslPipeline;

typedef std::function< std::unique_ptr<GlslPipeline>() > ShaderBuilder;

namespace operations {
typedef std::function< void(GlslPipeline &shader, Image **inputs, int nInputs, Image &output) > NAry;
typedef std::function< void(GlslPipeline &shader, Image &output) > Nullary;
typedef std::function< void(GlslPipeline &shader, Image &input, Image &output) > Unary;
typedef std::function< void(GlslPipeline &shader, Image &a, Image &b, Image &output) > Binary;

NAry convert(const Nullary &f);
NAry convert(const Unary &f);
NAry convert(const Binary &f);

class Factory : public ::accelerated::operations::StandardFactory {
public:
    virtual ::accelerated::operations::Function wrapNAry(
        const ShaderBuilder &shaderBuilder,
        const NAry &func) = 0;

    template <class T> ::accelerated::operations::Function wrap(
        const ShaderBuilder &shaderBuilder,
        const T &func)
    {
        return wrapNAry(shaderBuilder, convert(func));
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
