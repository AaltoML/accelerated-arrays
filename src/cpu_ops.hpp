#include "standard_ops.hpp"

namespace accelerated {
namespace cpu {
class Image;
namespace operations {
typedef std::function< void(const Image &input, Image &output) > SyncUnary;

class Factory : public ::accelerated::operations::StandardFactory {};

// may confuse the compiler due to the inherited "create" methods if inside
// the above class
std::unique_ptr<Factory> createFactory();
}
}
}
