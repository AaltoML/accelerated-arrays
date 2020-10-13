#pragma once

#include "function.hpp"
#include "image.hpp"
#include <vector>

namespace accelerated {
namespace operations {

namespace fill {
    struct Spec {
        std::vector<double> value;
        Spec setValue(const std::vector<double> &v) {
            value = v;
            return *this;
        }

        template <class V> Spec setValue(V v) {
            value = { static_cast<double>(v) };
            return *this;
        }
    };
}

// fixed-kernel 2D convolution
namespace fixedConvolution2D {
    struct Spec {
        // double can be losslesly converted to any of (u)int8/16/32 or float
        std::vector< std::vector<double> > kernel;
        double bias = 0.0;

        int xStride = 1;
        int yStride = 1;
        int xOffset = 0;
        int yOffset = 0;

        Image::Border border = Image::Border::ZERO;

        Spec setKernel(const std::vector< std::vector<double> > &k, double scale = 1.0) {
            kernel = k;
            for (auto &row : kernel)
                for (auto &el : row) el *= scale;
            return *this;
        }

        Spec setBias(double b) {
            bias = b;
            return *this;
        }

        Spec setStride(int x, int y) {
            xStride = x;
            yStride = y;
            return *this;
        }

        Spec setStride(int xy) {
            return setStride(xy, xy);
        }

        Spec setOffset(int x, int y) {
            xOffset = x;
            yOffset = y;
            return *this;
        }

        Spec setBorder(Image::Border b) {
            border = b;
            return *this;
        }

        int getKernelXOffset() const {
            return -(kernel.at(0).size() / 2) + xOffset;
        }

        int getKernelYOffset() const {
            return -(kernel.size() / 2) + yOffset;
        }
    };
}

/**
 * Pixel-wise affine transform y = A*x + b, where bo x and y are color
 * vectors (1-4 components), not necessarily the same length. The lengths
 * must match the image. For a "broadcasting" version, use channelwiseAffine
 */
namespace pixelwiseAffine {
    struct Spec {
        // empty means identity transform
        std::vector< std::vector<double> > linear;
        // empty means no bias
        std::vector< double > bias;

        Spec setLinear(const std::vector< std::vector<double> > &matrix, double scale = 1.0) {
            linear = matrix;
            for (auto &row : linear)
                for (auto &el : row) el *= scale;
            return *this;
        }

        Spec setBias(const std::vector<double> &b) {
            bias = b;
            return *this;
        }
    };
}

/**
 * Channel-wise affine transform y = a*x + b.
 * With the default spec, a = 1, b = 0, this is a simple copy / conversion
 * operation.
 */
namespace channelwiseAffine {
    struct Spec {
        double scale = 1.0;
        double bias = 0.0;

        Spec setScale(double a) {
            scale = a;
            return *this;
        }

        Spec setBias(double b) {
            bias = b;
            return *this;
        }
    };
}

struct StandardFactory {
    virtual ~StandardFactory() = default;

    virtual Function create(const fill::Spec &spec, const ImageTypeSpec &imageSpec) = 0;
    virtual Function create(const fixedConvolution2D::Spec &spec, const ImageTypeSpec &imageSpec) = 0;
    virtual Function create(const pixelwiseAffine::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) = 0;
    virtual Function create(const channelwiseAffine::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) = 0;

    // shorthands for same input & output spec
    inline Function create(const pixelwiseAffine::Spec &s, const ImageTypeSpec &i) { return create(s, i, i); }
    inline Function create(const channelwiseAffine::Spec &s, const ImageTypeSpec &i) { return create(s, i, i); }
};

}
}
