#pragma once

#include "function.hpp"
#include "image.hpp"
#include <vector>

namespace accelerated {
namespace operations {

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

        Spec &setKernel(const std::vector< std::vector<double> > &k, double scale = 1.0) {
            kernel = k;
            for (auto &row : kernel)
                for (auto &el : row) el *= scale;
            return *this;
        }

        Spec &setBias(double b) {
            bias = b;
            return *this;
        }

        Spec &setStride(int x, int y) {
            xStride = x;
            yStride = y;
            return *this;
        }

        Spec &setStride(int xy) {
            return setStride(xy, xy);
        }

        Spec &setOffset(int x, int y) {
            xOffset = x;
            yOffset = y;
            return *this;
        }

        Spec &setBorder(Image::Border b) {
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

    struct Factory {
        virtual Function create(const Spec &spec, const ImageTypeSpec &imageSpec) = 0;
    };
}

struct StandardFactory : fixedConvolution2D::Factory
{
    virtual ~StandardFactory() = default;
};

}
}
