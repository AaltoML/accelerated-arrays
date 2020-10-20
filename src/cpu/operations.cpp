#include <iostream>

#include "operations.hpp"
#include "image.hpp"

namespace accelerated {
namespace cpu {
namespace operations {
namespace {
typedef ::accelerated::Image BaseImage;
typedef ::accelerated::operations::fixedConvolution2D::Spec FixedConvolution2DSpec;
typedef ::accelerated::operations::fill::Spec FillSpec;
typedef ::accelerated::operations::rescale::Spec RescaleSpec;
typedef ::accelerated::operations::swizzle::Spec SwizzleSpec;
typedef ::accelerated::operations::pixelwiseAffineCombination::Spec PixelwiseAffineCombinationSpec;
typedef ::accelerated::operations::channelwiseAffine::Spec ChannelwiseAffineSpec;
using ::accelerated::operations::Function;

void checkSpec(const ImageTypeSpec &spec) {
    (void)spec;
    aa_assert(spec.storageType == ImageTypeSpec::StorageType::CPU);
}

double interpolateFloat(Image &img, double x, double y, int c, Image::Interpolation i, Image::Border b) {
    aa_assert(i == Image::Interpolation::NEAREST || i == Image::Interpolation::UNDEFINED); // TODO
    // note: not necessarily consisten rounding for negative vals
    return img.get<float>(int(x + 0.5), int(y + 0.5), c, b);
}

namespace impl { // to avoid name clashes with StandardFactory

template <class T>
void forEachPixelFast(
    Image &in, Image &out,
    const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec,
    const std::function<void(const T *inPtr, T *outPtr)> &f)
{
    aa_assert(in.width == out.width && in.height == out.height);
    aa_assert(in == inSpec);
    aa_assert(out == outSpec);
    const T *inPtr = in.getData<T>();
    T *outPtr = out.getData<T>();
    for (int y = 0; y < in.height; ++y) {
        for (int x = 0; x < in.width; ++x) {
            f(inPtr, outPtr);
            inPtr += inSpec.channels;
            outPtr += outSpec.channels;
        }
    }
}

void forEachPixelAndChannel(Image &img, const std::function<void(Image &img, int x, int y, int c)> &f) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            for (int c = 0; c < img.channels; ++c) {
                f(img, x, y, c);
            }
        }
    }
}

Nullary fill(const FillSpec &spec, const ImageTypeSpec &outSpec) {
    aa_assert(int(spec.value.size()) == outSpec.channels);
    return [spec, outSpec](Image &output) {
        aa_assert(output == outSpec);
        forEachPixelAndChannel(output, [&spec](Image &output, int x, int y, int c) {
            output.set<float>(x, y, c, spec.value.at(c));
        });
    };
}

Unary rescale(const RescaleSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    return [spec, inSpec, outSpec](Image &input, Image &output) {
        aa_assert(output.channels == input.channels);
        aa_assert(input == inSpec);
        aa_assert(output == outSpec);
        forEachPixelAndChannel(output, [&spec, &input](Image &output, int x, int y, int c) {
            float relX = x / float(output.width);
            float relY = y / float(output.height);
            float newX = (relX * spec.xScale + spec.xTranslation) * input.width;
            float newY = (relY * spec.yScale + spec.yTranslation) * input.height;

            output.set<float>(x, y, c, interpolateFloat(input, newX, newY, c, spec.interpolation, spec.border));
        });
    };
}

Unary swizzleGeneric(const SwizzleSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    aa_assert(int(spec.channelList.size()) == outSpec.channels);
    return [spec, inSpec, outSpec](Image &input, Image &output) {
        aa_assert(input == inSpec);
        aa_assert(output == outSpec);
        forEachPixelAndChannel(output, [&spec, &input](Image &output, int x, int y, int c) {
            int chan = spec.channelList.at(c);
            if (chan == -1) {
                output.set<float>(x, y, c, spec.constantList.at(c));
            } else {
                output.set<float>(x, y, c, input.get<float>(x, y, spec.channelList.at(c)));
            }
        });
    };
}

template <class T> Unary swizzle(const SwizzleSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    aa_assert(int(spec.channelList.size()) == outSpec.channels);
    return [spec, inSpec, outSpec](Image &input, Image &output) {
        int n = spec.channelList.size();
        const int *chanList = spec.channelList.data();
        const int *constList = spec.constantList.data();
        forEachPixelFast<T>(input, output, inSpec, outSpec, [n, chanList, constList](const T *in, T *out) {
            for (int c = 0; c < n; ++c) {
                int chan = chanList[c];
                if (chan == -1) {
                    out[c] = constList[c];
                } else {
                    out[c] = in[chan];
                }
            }
        });
    };
}

NAry pixelwiseAffineCombination(const PixelwiseAffineCombinationSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    return [spec, inSpec, outSpec](Image **inputs, int nInputs, Image &output) {
        aa_assert(int(spec.linear.size()) == nInputs);
        aa_assert(output == outSpec);
        for (int i = 0; i < nInputs; ++i) aa_assert(*inputs[i] == inSpec);
        forEachPixelAndChannel(output, [&spec, inputs, nInputs](Image &output, int x, int y, int c) {
            float v = spec.bias.empty() ? 0.0 : spec.bias.at(c);
            for (int i = 0; i < nInputs; ++i) {
                auto &input = *inputs[i];
                const auto &matRow = spec.linear.at(i).at(c);
                aa_assert(int(matRow.size()) == input.channels);
                for (int inChan = 0; inChan < input.channels; ++inChan) {
                    const float inValue = input.get<float>(x, y, inChan);
                    v += matRow.at(inChan) * inValue;
                }
            }
            // std::cout << x << " " << y << " " << c << " = " << v << std::endl;
            output.set<float>(x, y, c, v);
        });
    };
}

template <class T> Unary pixelwiseAffineUnary(const PixelwiseAffineCombinationSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    aa_assert(int(spec.linear.size()) == 1);
    std::vector<float> bias, matColMajor;
    const int n = outSpec.channels, m = inSpec.channels;
    for (int i = 0; i < n; ++i) {
        if (i >= int(spec.bias.size())) bias.push_back(0);
        else bias.push_back(float(spec.bias.at(i)));

        const auto &mat = spec.linear.at(0);
        for (int j = 0; j < m; ++j) {
            if (i < int(mat.size()) && j < int(mat.at(i).size())) {
                matColMajor.push_back(mat.at(i).at(j));
            } else {
                matColMajor.push_back(0);
            }
        }
    }
    return [bias, matColMajor, n, m, inSpec, outSpec](Image &input, Image &output) {
        const float *biasData = bias.data();
        const float *matData = matColMajor.data();
        forEachPixelFast<T>(input, output, inSpec, outSpec, [n, m, biasData, matData](const T *in, T *out) {
            const float *coeff = matData;
            for (int i = 0; i < n; ++i) {
                float v = biasData[i];
                for (int j = 0; j < m; ++j) {
                    v += (*coeff) * float(in[j]);
                    coeff++;
                }
                out[i] = T(v);
            }
        });
    };
}

Unary channelwiseAffine(const ChannelwiseAffineSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    return [spec, inSpec, outSpec](Image &input, Image &output) {
        aa_assert(output.channels == input.channels);
        aa_assert(input == inSpec);
        aa_assert(output == outSpec);
        forEachPixelAndChannel(output, [&spec, &input](Image &output, int x, int y, int c) {
            const float inValue = input.get<float>(x, y, c);
            output.set<float>(x, y, c, spec.scale * inValue + spec.bias);
        });
    };
}

Unary fixedConvolution2D(const FixedConvolution2DSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) {
    aa_assert(!spec.kernel.empty());
    return [spec, inSpec, outSpec](Image &input, Image &output) {
        aa_assert(input == inSpec);
        aa_assert(output == outSpec);
        const int kernelXOffset = spec.getKernelXOffset();
        const int kernelYOffset = spec.getKernelYOffset();
        // std::cout << spec.kernel.size() << " " << spec.kernel.at(0).size() << std::endl;
        forEachPixelAndChannel(output, [kernelYOffset, kernelXOffset, &spec, &input](Image &output, int x, int y, int c) {
            float v = spec.bias;
            for (int i = 0; i < int(spec.kernel.size()); ++i) {
                const int y1 = y * spec.yStride + i + kernelYOffset;
                const auto &krow = spec.kernel.at(i);
                for (int j = 0; j < int(krow.size()); ++j) {
                    const int x1 = x * spec.xStride + j + kernelXOffset;
                    v += input.get<float>(x1, y1, c, spec.border) * krow.at(j);
                    //std::cout << " x:" << x << " y:" << y << " c:" << c << " i:" << i << " j:"  << j
                    // << " x1:" << x1 << " y1:" << y1 << " : " << double(input.get<T>(x1, y1, c, spec.border)) << " * " <<  krow.at(j) << std::endl;
                 }
            }
            // std::cout << x << " " << y << " " << c << " = " << v << std::endl;
            output.set<float>(x, y, c, v);
        });
    };
}
}

class CpuFactory : public Factory {
private:
    Processor &processor;

public:
    CpuFactory(Processor &processor) : processor(processor) {}

    Function wrapNAry(const NAry &f) final {
        return ::accelerated::operations::sync::wrap(f, processor);
    }

    Function create(const FixedConvolution2DSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) final {
        checkSpec(inSpec);
        checkSpec(outSpec);
        return wrap<Unary>(impl::fixedConvolution2D(spec, inSpec, outSpec));
    }

    Function create(const FillSpec &spec, const ImageTypeSpec &imageSpec) final {
        checkSpec(imageSpec);
        return wrap<Nullary>(impl::fill(spec, imageSpec));
    }

    Function create(const RescaleSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) final {
        checkSpec(inSpec);
        checkSpec(outSpec);
        return wrap<Unary>(impl::rescale(spec, inSpec, outSpec));
    }

    Function create(const SwizzleSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) final {
        checkSpec(inSpec);
        checkSpec(outSpec);
        if (inSpec.dataType == outSpec.dataType) {
            #define X(type, name) if (inSpec.dataType == name) \
                return wrap<Unary>(impl::swizzle<type>(spec, inSpec, outSpec));
            ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(X)
            #undef X
        }
        return wrap<Unary>(impl::swizzleGeneric(spec, inSpec, outSpec));
    }

    Function create(const PixelwiseAffineCombinationSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) final {
        checkSpec(inSpec);
        checkSpec(outSpec);
        if (spec.linear.size() == 1 && inSpec.dataType == outSpec.dataType) {
            #define X(type, name) if (inSpec.dataType == name) \
                return wrap<Unary>(impl::pixelwiseAffineUnary<type>(spec, inSpec, outSpec));
            ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(X)
            #undef X
        }
        return wrapNAry(impl::pixelwiseAffineCombination(spec, inSpec, outSpec));
    }

    Function create(const ChannelwiseAffineSpec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) final {
        checkSpec(inSpec);
        checkSpec(outSpec);
        return wrap<Unary>(impl::channelwiseAffine(spec, inSpec, outSpec));
    }
};
}

std::unique_ptr<Factory> createFactory(Processor &processor) {
    return std::unique_ptr<Factory>(new CpuFactory(processor));
}

}
}
}
