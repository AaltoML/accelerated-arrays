#include "../standard_ops.hpp"

namespace accelerated {
class Processor;
namespace cpu {
class Image;
namespace operations {
typedef std::function< void(const Image &input, Image &output) > SyncUnary;

class Factory : public ::accelerated::operations::StandardFactory {
public:
    virtual ::accelerated::operations::Function wrap(const SyncUnary &func) = 0;
};

// may confuse the compiler due to the inherited "create" methods if inside
// the above class and called just "create"
std::unique_ptr<Factory> createFactory(Processor &processor);
}
}
}
