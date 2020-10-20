#pragma once

#include "function.hpp"
#include "image.hpp"
#include <vector>

namespace accelerated {
namespace operations {

struct StandardFactory;
struct Builder {
    StandardFactory *factory = nullptr;
};

namespace copy {
    struct Spec : Builder {
        Function build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);
        Function build(const ImageTypeSpec &spec);
    };
}

namespace fill {
    struct Spec : Builder {
        std::vector<double> value;
        Spec setValue(const std::vector<double> &v) {
            value = v;
            return *this;
        }

        template <class V> Spec setValue(V v) {
            value = { static_cast<double>(v) };
            return *this;
        }

        Function build(const ImageTypeSpec &outSpec);
    };
}

// fixed-kernel 2D convolution
namespace fixedConvolution2D {
    struct Spec : Builder {
        // double can be losslesly converted to any of (u)int8/16/32 or float
        std::vector< std::vector<double> > kernel;
        double bias = 0.0;

        int xStride = 1;
        int yStride = 1;
        int xOffset = 0;
        int yOffset = 0;

        Image::Border border = Image::Border::ZERO;

        Spec setKernel(const std::vector< std::vector<double> > &k) {
            kernel = k;
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

        Spec scaleKernelValues(double scale) {
            for (auto &row : kernel)
                for (auto &el : row) el *= scale;
            return *this;
        }

        int getKernelXOffset() const {
            return -(kernel.at(0).size() / 2) + xOffset;
        }

        int getKernelYOffset() const {
            return -(kernel.size() / 2) + yOffset;
        }

        Function build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);
        Function build(const ImageTypeSpec &spec);
    };
}

/**
 * Pixel-wise affine transform y = A*x + b, where bo x and y are color
 * vectors (1-4 components), not necessarily the same length. The lengths
 * must match the image. For a "broadcasting" version, use channelwiseAffine
 */
namespace pixelwiseAffine {
    struct Spec  : Builder {
        // empty means identity transform
        std::vector< std::vector<double> > linear;
        // empty means no bias
        std::vector< double > bias;

        Spec setLinear(const std::vector< std::vector<double> > &matrix) {
            linear = matrix;
            return *this;
        }

        Spec setBias(const std::vector<double> &b) {
            bias = b;
            return *this;
        }

        Spec scaleLinearValues(double scale) {
            for (auto &row : linear)
                for (auto &el : row) el *= scale;
            return *this;
        }

        Function build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);
        Function build(const ImageTypeSpec &spec);
    };
}

/**
 * Combine multiple images with pixelwise affine transforms:
 * out = A1 * x1 + A2 * x2 + ... + An * xn + b, where A is a matrix of
 * suitable sizs and b is a bias vector. All input images must be of the
 * same type
 */
namespace pixelwiseAffineCombination {
    struct Spec : Builder {
        std::vector< std::vector< std::vector<double> > > linear;
        // empty means no bias
        std::vector< double > bias;

        Spec setBias(const std::vector<double> &b) {
            bias = b;
            return *this;
        }

        Spec scaleLinearValues(double scale) {
            for (auto &m : linear)
                for (auto &row : m)
                    for (auto &el : row) el *= scale;
            return *this;
        }

        Spec addLinearPart(const std::vector<std::vector<double>> &matrix) {
            linear.push_back(matrix);
            return *this;
        }

        Function build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);
        Function build(const ImageTypeSpec &spec);
    };
}

/**
 * Channel-wise affine transform y = a*x + b.
 * With the default spec, a = 1, b = 0, this is a simple copy / conversion
 * operation.
 */
namespace channelwiseAffine {
    struct Spec : Builder {
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

        Function build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);
        Function build(const ImageTypeSpec &spec);
    };
}

/**
 * Rescale image: all units relative to OUTPUT image size, e.g.,
 * setScale(1, 1) resizes to fit output size.
 */
namespace rescale {
    struct Spec : Builder {
        Image::Interpolation interpolation = Image::Interpolation::UNDEFINED;
        Image::Border border = Image::Border::UNDEFINED;

        // all relative to image size
        double xScale = 1.0;
        double yScale = 1.0;
        double xTranslation = 0.0;
        double yTranslation = 0.0;

        Spec setScale(double x, double y) {
            xScale = x;
            yScale = y;
            return *this;
        }

        Spec setScale(double s) {
            return setScale(s, s);
        }

        Spec setTranslation(double x, double y) {
            xTranslation = x;
            yTranslation = y;
            return *this;
        }

        Spec setInterpolation(Image::Interpolation i) {
            interpolation = i;
            return *this;
        }

        Spec setBorder(Image::Border b) {
            border = b;
            return *this;
        }

        Function build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);
        Function build(const ImageTypeSpec &spec);
    };
}

/**
 * Reorder and replicate image channels using a short syntax, e.g., to
 * to convert from BGR to RGBA with A=1, swizzle with "bgr1"
 */
namespace swizzle {
    struct Spec : Builder {
        /** For example: bgr1 -> [2,1,0 -1] */
        std::vector<int> channelList;
        /** For example: bgr1 -> [0,0,0,1] */
        std::vector<int> constantList;

        Spec(const std::string &s);

        Function build(const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);
        Function build(const ImageTypeSpec &spec);
    };
}

struct StandardFactory : Builder {
    virtual ~StandardFactory();

    // shorthands
    fill::Spec fill(const std::vector<double> &v) { return setFactory(fill::Spec{}.setValue(v)); }
    fill::Spec fill(double v) { return setFactory(fill::Spec{}.setValue({ v })); }

    swizzle::Spec swizzle(const std::string &s) { return setFactory(swizzle::Spec(s)); }

    rescale::Spec rescale() { return setFactory(rescale::Spec{}); }
    rescale::Spec rescale(double s) { return rescale().setScale(s); }
    rescale::Spec rescale(double x, double y) { return rescale().setScale(x, y); }

    copy::Spec copy() {
      return setFactory(copy::Spec{});
    }

    channelwiseAffine::Spec channelwiseAffine(double scale, double bias = 0.0) {
      return setFactory(channelwiseAffine::Spec{}.setScale(scale).setBias(bias));
    }

    pixelwiseAffine::Spec pixelwiseAffine(const std::vector< std::vector<double> > &matrix) {
      return setFactory(pixelwiseAffine::Spec{}.setLinear(matrix));
    }

    pixelwiseAffineCombination::Spec affineCombination() {
      return setFactory(pixelwiseAffineCombination::Spec{});
    }

    fixedConvolution2D::Spec fixedConvolution2D(const std::vector< std::vector<double> > &kernel) {
      return setFactory(fixedConvolution2D::Spec{}.setKernel(kernel));
    }

    // actual implementation
    virtual Function create(const fill::Spec &spec, const ImageTypeSpec &imageSpec) = 0;
    virtual Function create(const swizzle::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) = 0;
    virtual Function create(const rescale::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) = 0;
    virtual Function create(const fixedConvolution2D::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) = 0;
    virtual Function create(const pixelwiseAffineCombination::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) = 0;
    virtual Function create(const channelwiseAffine::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec) = 0;

    // with default implementations
    virtual Function create(const pixelwiseAffine::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);
    virtual Function create(const copy::Spec &spec, const ImageTypeSpec &inSpec, const ImageTypeSpec &outSpec);


private:
    template <class T> inline T &&setFactory(T &&t) { t.factory = this; return std::move(t); }
};

}
}
