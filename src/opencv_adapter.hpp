#pragma once

/*
 * OpenCV is not a depdencency unless you manually include this file.
 * It provides (inline) conversion methods to / from OpenCV matrices
 */

#include <opencv2/core.hpp>

#include "future.hpp"
#include "image.hpp"
#include "cpu/image.hpp"
#include "assert.hpp"

namespace accelerated {

// using struct instead of namespace to avoid compiler warnings about
// unused functions
struct opencv {
    static cv::Mat ref(Image &img) {
        aa_assert(img.storageType == ImageTypeSpec::StorageType::CPU);
        auto &cpuImg = cpu::Image::castFrom(img);
        return cv::Mat(img.height, img.width, convertSpec(img), cpuImg.getDataRaw(), cpuImg.bytesPerRow());
    }

    static std::unique_ptr<cpu::Image> ref(const cv::Mat &m, bool preferFixedPoint = false) {
        const auto spec = convertSpec(m, preferFixedPoint);
        auto bytesPerRow = m.step;
        aa_assert(bytesPerRow % spec.bytesPerPixel() == 0);
        auto pixelsPerRow = bytesPerRow / spec.bytesPerPixel();
        return cpu::Image::createReference(m.cols, m.rows, spec.channels, spec.dataType, m.data, pixelsPerRow);
    }

    static cv::Mat emptyLike(const Image &img) {
        cv::Mat m(img.height, img.width, convertSpec(img));
        return m;
    }

    static std::unique_ptr<cpu::Image> emptyLike(const cv::Mat &mat, bool preferFixedPoint = false) {
        const auto spec = convertSpec(mat, preferFixedPoint);
        return std::unique_ptr<cpu::Image>(reinterpret_cast<cpu::Image*>(cpu::Image::createFactory()->create(
            mat.cols,
            mat.rows,
            spec.channels,
            spec.dataType).release()));
    }

    static Future copy(const cv::Mat &from, Image &to) {
        // TODO: copy using OpenCV to support ROIs
        return ref(from, ImageTypeSpec::isFixedPoint(to.dataType))->copyTo(to);
    }

    static Future copy(Image &from, cv::Mat &to) {
        // TODO: copy using OpenCV to support ROIs
        if (to.empty()) to = emptyLike(from);
        return ref(to, ImageTypeSpec::isFixedPoint(from.dataType))->copyFrom(from);
    }

    static ImageTypeSpec convertSpec(const cv::Mat &image, bool preferFixedPoint = false) {
        int ch = image.channels();
        aa_assert(ch >= 1 && ch <= 4);
        return cpu::Image::getSpec(ch, convertDataType(CV_MAT_DEPTH(image.type()), preferFixedPoint));
    }

    static int convertSpec(const ImageTypeSpec &spec) {
        return CV_MAKETYPE(convertDataType(spec.dataType), spec.channels);
    }

    static int convertDataType(ImageTypeSpec::DataType dtype) {
        typedef ImageTypeSpec::DataType DataType;
        switch (dtype) {
            case DataType::UINT8: return CV_8U;
            case DataType::SINT8: return CV_8S;
            case DataType::UINT16: return CV_16U;
            case DataType::SINT16: return CV_16S;
            case DataType::UINT32: aa_assert(false && "UINT32 type is not supported by OpenCV"); return 0;
            case DataType::SINT32: return CV_32S;
            case DataType::FLOAT32: return CV_32F;
            case DataType::UFIXED8: return CV_8U;
            case DataType::SFIXED8: return CV_8S;
            case DataType::UFIXED16: return CV_16U;
            case DataType::SFIXED16: return CV_16S;
            case DataType::UFIXED32: aa_assert(false && "UINT32 (fixed-point) type is not supported by OpenCV"); return 0;
            case DataType::SFIXED32: return CV_32S;
            default: aa_assert(false);
        }
        return 0;
    }

    static ImageTypeSpec::DataType convertDataType(int cvDataType, bool preferFixedPoint = false) {
        typedef ImageTypeSpec::DataType DataType;
        switch (cvDataType) {
            case CV_8U:
                if (preferFixedPoint) return DataType::UFIXED8;
                else return DataType::UINT8;
            case CV_8S:
                if (preferFixedPoint) return DataType::SFIXED8;
                else return DataType::SINT8;
            case CV_16U:
                if (preferFixedPoint) return DataType::UFIXED16;
                else return DataType::UINT16;
            case CV_16S:
                if (preferFixedPoint) return DataType::SFIXED16;
                else return DataType::SINT16;
            // note: no UFIXED32 / UINT32
            case CV_32S:
                if (preferFixedPoint) return DataType::SFIXED32;
                else return DataType::SINT32;
            case CV_32F:
                return DataType::FLOAT32;
            default:
                aa_assert(false && "unsupported OpenCV data type");
                break;
        }
        return DataType::UFIXED8;
    }
};
}
