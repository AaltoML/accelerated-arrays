#include "standard_ops.hpp"
#include <map>
#include <string>

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

DEF_FUNC(copy)
DEF_FUNC(rescale)
DEF_FUNC(swizzle)
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

Function StandardFactory::create(const copy::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    (void)spec;
    aa_assert(inSpec.channels == outSpec.channels);
    return create(swizzle::Spec(std::string("rgba").substr(0, outSpec.channels)), inSpec, outSpec);
}

swizzle::Spec::Spec(const std::string &s) {
    const std::map<char, int> chanLookup = {
        {'r', 0},
        {'g', 1},
        {'b', 2},
        {'a', 3},
        {'x', 0},
        {'y', 1},
        {'z', 2},
        {'w', 3},
    };

    const std::map<int, int> constLookup = {
        {'0', 0},
        {'1', 1}
    };

    for (char c : s) {
        if (chanLookup.count(c)) {
            channelList.push_back(chanLookup.at(c));
            constantList.push_back(0);
        } else if (constLookup.count(c)) {
            channelList.push_back(-1);
            constantList.push_back(constLookup.at(c));
        } else {
            aa_assert("invalid swizzle character");
        }
    }
}

}
}
