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
}
}
