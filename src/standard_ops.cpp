#include "standard_ops.hpp"

namespace accelerated {
namespace operations {

StandardFactory::~StandardFactory() = default;

Function fill::Spec::build(const ImageTypeSpec &outSpec) {
    aa_assert(factory != nullptr);
    return factory->create(*this, outSpec);
}

#define DEF_UNARY(x) \
    Function x::Spec::build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) { \
        aa_assert(factory != nullptr); \
        return factory->create(*this, inSpec, outSpec); \
    } \
    Function x::Spec::build(const ImageTypeSpec &spec) { return build(spec, spec); }

DEF_UNARY(fixedConvolution2D)
DEF_UNARY(pixelwiseAffine)
DEF_UNARY(channelwiseAffine)

#undef DEF_UNARY

}
}
