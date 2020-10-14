#include "standard_ops.hpp"

namespace accelerated {
namespace operations {

StandardFactory::~StandardFactory() = default;

Function fill::Spec::build(const ImageTypeSpec &outSpec) {
    aa_assert(factory != nullptr);
    return factory->create(*this, outSpec);
}

// unary or more arguments
#define DEF_FUNC(x) \
    Function x::Spec::build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) { \
        aa_assert(factory != nullptr); \
        return factory->create(*this, inSpec, outSpec); \
    } \
    Function x::Spec::build(const ImageTypeSpec &spec) { return build(spec, spec); }

DEF_FUNC(fixedConvolution2D)
DEF_FUNC(channelwiseAffine)
DEF_FUNC(pixelwiseAffine)
DEF_FUNC(pixelwiseAffineCombination)

#undef DEF_FUNC

Function StandardFactory::create(const pixelwiseAffine::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    pixelwiseAffineCombination::Spec comboSpec;
    comboSpec.factory = spec.factory;
    return create(comboSpec.addLinearPart(spec.linear).setBias(spec.bias), inSpec, outSpec);
}

}
}
