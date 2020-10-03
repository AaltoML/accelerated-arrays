#include "standard_ops.hpp"

namespace accelerated {
class CpuImage;
namespace operations {

class Cpu : public StandardFactory {
public:
    typedef std::function< void(const CpuImage &input, CpuImage &output) > SyncUnary;
    static std::unique_ptr<Cpu> createFactory();
};

}
}
