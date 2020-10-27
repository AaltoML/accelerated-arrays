#include <sstream>

#include "adapters.hpp"
#include "image.hpp"
#include "../log.hpp"

namespace accelerated {
namespace opengl {
namespace {
void logSpec(const ImageTypeSpec &spec) {
    (void)spec;
    LOG_TRACE("spec: %d channels, %d bits, %s %s%s%s",
        spec.channels, int(spec.bytesPerChannel()*8),
        ImageTypeSpec::isSigned(spec.dataType) ? "signed" : "",
        ImageTypeSpec::isFixedPoint(spec.dataType) ? "fixed" : "",
        ImageTypeSpec::isIntegerType(spec.dataType) ? "integer" : "",
        ImageTypeSpec::isFloat(spec.dataType) ? "float" : "");
}
}

int getTextureInternalFormat(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getInternalFormat:%s", #x); return x

    logSpec(spec);

    // could be tried as a fallback but not preferrable
    /*
    if (useUnsizedFormats) {
        switch (spec.channels) {
            case 1: X(GL_RED);
            case 2: X(GL_RG);
            case 3: X(GL_RGB);
            case 4: X(GL_RGBA);
            default: aa_assert(false); break;
        }
    }
    */
    const bool allowLossy = false;
    #define LOSSY(x) aa_assert(allowLossy && #x); X(x)

    if (spec.channels == 1) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::UINT8: X(GL_R8UI);
            case ImageTypeSpec::DataType::SINT8: X(GL_R8I);
            case ImageTypeSpec::DataType::UINT16: X(GL_R16UI);
            case ImageTypeSpec::DataType::SINT16: X(GL_R16I);
            case ImageTypeSpec::DataType::UINT32: X(GL_R32UI);
            case ImageTypeSpec::DataType::SINT32: X(GL_R32I);
            case ImageTypeSpec::DataType::FLOAT32: X(GL_R32F);
            case ImageTypeSpec::DataType::UFIXED8: X(GL_R8);
            case ImageTypeSpec::DataType::SFIXED8: X(GL_R8_SNORM);
        #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
            case ImageTypeSpec::DataType::UFIXED16: LOSSY(GL_R16F);
            case ImageTypeSpec::DataType::SFIXED16: LOSSY(GL_R16F);
        #else
            case ImageTypeSpec::DataType::UFIXED16: X(GL_R16);
            case ImageTypeSpec::DataType::SFIXED16: X(GL_R16_SNORM);
        #endif
            case ImageTypeSpec::DataType::UFIXED32: LOSSY(GL_R32F);
            case ImageTypeSpec::DataType::SFIXED32: LOSSY(GL_R32F);
            default: break;
        }
    }
    else if (spec.channels == 2) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::UINT8: X(GL_RG8UI);
            case ImageTypeSpec::DataType::SINT8: X(GL_RG8I);
            case ImageTypeSpec::DataType::UINT16: X(GL_RG16UI);
            case ImageTypeSpec::DataType::SINT16: X(GL_RG16I);
            case ImageTypeSpec::DataType::UINT32: X(GL_RG32UI);
            case ImageTypeSpec::DataType::SINT32: X(GL_RG32I);
            case ImageTypeSpec::DataType::FLOAT32: X(GL_RG32F);
            case ImageTypeSpec::DataType::UFIXED8: X(GL_RG8);
            case ImageTypeSpec::DataType::SFIXED8: X(GL_RG8_SNORM);
        #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
            case ImageTypeSpec::DataType::UFIXED16: LOSSY(GL_RG16F);
            case ImageTypeSpec::DataType::SFIXED16: LOSSY(GL_RG16F);
        #else
            case ImageTypeSpec::DataType::UFIXED16: X(GL_RG16);
            case ImageTypeSpec::DataType::SFIXED16: X(GL_RG16_SNORM);
        #endif
            case ImageTypeSpec::DataType::UFIXED32: LOSSY(GL_RG32F);
            case ImageTypeSpec::DataType::SFIXED32: LOSSY(GL_RG32F);
            default: break;
        }
    }
    else if (spec.channels == 3) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::UINT8: X(GL_RGB8UI);
            case ImageTypeSpec::DataType::SINT8: X(GL_RGB8I);
            case ImageTypeSpec::DataType::UINT16: X(GL_RGB16UI);
            case ImageTypeSpec::DataType::SINT16: X(GL_RGB16I);
            case ImageTypeSpec::DataType::UINT32: X(GL_RGB32UI);
            case ImageTypeSpec::DataType::SINT32: X(GL_RGB32I);
            case ImageTypeSpec::DataType::FLOAT32: X(GL_RGB32F);
            case ImageTypeSpec::DataType::UFIXED8: X(GL_RGB8);
            case ImageTypeSpec::DataType::SFIXED8: X(GL_RGB8_SNORM);
        #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
            case ImageTypeSpec::DataType::UFIXED16: LOSSY(GL_RGB16F);
            case ImageTypeSpec::DataType::SFIXED16: LOSSY(GL_RGB16F);
        #else
            case ImageTypeSpec::DataType::UFIXED16: X(GL_RGB16);
            case ImageTypeSpec::DataType::SFIXED16: X(GL_RGB16_SNORM);
        #endif
            case ImageTypeSpec::DataType::UFIXED32: LOSSY(GL_RGB32F);
            case ImageTypeSpec::DataType::SFIXED32: LOSSY(GL_RGB32F);
            default: break;
        }
    }
    else if (spec.channels == 4) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::UINT8: X(GL_RGBA8UI);
            case ImageTypeSpec::DataType::SINT8: X(GL_RGBA8I);
            case ImageTypeSpec::DataType::UINT16: X(GL_RGBA16UI);
            case ImageTypeSpec::DataType::SINT16: X(GL_RGBA16I);
            case ImageTypeSpec::DataType::UINT32: X(GL_RGBA32UI);
            case ImageTypeSpec::DataType::SINT32: X(GL_RGBA32I);
            case ImageTypeSpec::DataType::FLOAT32: X(GL_RGBA32F);
            case ImageTypeSpec::DataType::UFIXED8: X(GL_RGBA8);
            case ImageTypeSpec::DataType::SFIXED8: X(GL_RGBA8_SNORM);
        #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
            case ImageTypeSpec::DataType::UFIXED16: LOSSY(GL_RGBA16F);
            case ImageTypeSpec::DataType::SFIXED16: LOSSY(GL_RGBA16F);
        #else
            case ImageTypeSpec::DataType::UFIXED16: X(GL_RGBA16);
            case ImageTypeSpec::DataType::SFIXED16: X(GL_RGBA16_SNORM);
        #endif
            case ImageTypeSpec::DataType::UFIXED32: LOSSY(GL_RGBA32F);
            case ImageTypeSpec::DataType::SFIXED32: LOSSY(GL_RGBA32F);
            default: break;
        }
    }

    #undef LOSSY
    #undef X

    aa_assert(false && "no suitable internal format");
    aa_assert(false && "not implemented");
    return -1;
}

int getCpuFormat(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getCpuFormat:%s", #x); return x
    // TODO: also support fixed-point types
    if (ImageTypeSpec::isIntegerType(spec.dataType)) {
        switch (spec.channels) {
            case 1: X(GL_RED_INTEGER);
            case 2: X(GL_RG_INTEGER);
            case 3: X(GL_RGB_INTEGER);
            case 4: X(GL_RGBA_INTEGER);
            default: break;
        }
    } else {
        switch (spec.channels) {
            case 1: X(GL_RED);
            case 2: X(GL_RG);
            case 3: X(GL_RGB);
            case 4: X(GL_RGBA);
            default: break;
        }
    }
    #undef X
    aa_assert(false);
    return -1;
}

std::string getGlslPrecision(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getGlslPrecision:%s", x); return x
    switch (spec.dataType) {
        case ImageTypeSpec::DataType::UINT8: X("lowp");
        case ImageTypeSpec::DataType::SINT8: X("lowp");
        case ImageTypeSpec::DataType::UINT16: X("highp");
        case ImageTypeSpec::DataType::SINT16: X("highp");
        case ImageTypeSpec::DataType::UINT32: X("highp");
        case ImageTypeSpec::DataType::SINT32: X("highp");
        case ImageTypeSpec::DataType::FLOAT32: X("highp");
        case ImageTypeSpec::DataType::UFIXED8: X("lowp");
        case ImageTypeSpec::DataType::SFIXED8: X("lowp");
        case ImageTypeSpec::DataType::UFIXED16: X("highp");
        case ImageTypeSpec::DataType::SFIXED16: X("highp");
        case ImageTypeSpec::DataType::UFIXED32: X("highp");
        case ImageTypeSpec::DataType::SFIXED32: X("highp");
        default: break;
    }
    #undef X
    aa_assert(false);
    return "";
}

std::string getGlslSamplerType(const ImageTypeSpec &spec) {
    if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL_EXTERNAL) {
        aa_assert(spec.dataType == ImageTypeSpec::DataType::UFIXED8);
        return "samplerExternalOES";
    }

    if (ImageTypeSpec::isIntegerType(spec.dataType)) {
        if (ImageTypeSpec::isSigned(spec.dataType)) return "isampler2D";
        return "usampler2D";
    }

    return "sampler2D";
}

std::string getGlslScalarType(const ImageTypeSpec &spec) {
    if (ImageTypeSpec::isIntegerType(spec.dataType)) {
        if (ImageTypeSpec::isSigned(spec.dataType)) return "int";
        return "uint";
    }
    return "float";
}

std::string getGlslVecType(const ImageTypeSpec &spec) {
    if (spec.channels == 1) return getGlslScalarType(spec);
    std::ostringstream oss;
    if (ImageTypeSpec::isIntegerType(spec.dataType)) {
        if (ImageTypeSpec::isSigned(spec.dataType)) oss << "i";
        else oss << "u";
    }
    oss << "vec";
    oss << spec.channels;
    return oss.str();
}

int getReadPixelFormat(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getReadPixelFormat:%s", #x); return x
    if (ImageTypeSpec::isIntegerType(spec.dataType)) {
        // https://stackoverflow.com/a/55141849/1426569
        // "Note: the official reference page is incomplete/wrong."
        // (╯°□°)╯︵ ┻━┻
        // (would not be supported in GLES anyway)
        switch (spec.channels) {
            case 1: X(GL_RED_INTEGER);
            case 2: X(GL_RG_INTEGER);
            case 3: X(GL_RGB_INTEGER);
            case 4: X(GL_RGBA_INTEGER);
            default: break;
        }
    } else {
        switch (spec.channels) {
            case 1: X(GL_RED);
            case 2:
            #ifndef ACCELERATED_ARRAYS_USE_OPENGL_ES
            #ifndef ACCELERATED_ARRAYS_DODGY_READS
                // may work just fine in practice
                log_warn("OpenGL spec does not allow reading 2-channel textures directly");
            #endif
            #endif
                X(GL_RG);
            case 3: X(GL_RGB);
            case 4: X(GL_RGBA);
            default: break;
        }
    }
    #undef X
    aa_assert(false);
    return -1;
}

int getCpuType(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getCpuType:%s", #x); return x
    switch (spec.dataType) {
        case ImageTypeSpec::DataType::UINT8: X(GL_UNSIGNED_BYTE);
        case ImageTypeSpec::DataType::SINT8: X(GL_BYTE);
        case ImageTypeSpec::DataType::UINT16: X(GL_UNSIGNED_SHORT);
        case ImageTypeSpec::DataType::SINT16: X(GL_SHORT);
        case ImageTypeSpec::DataType::UINT32: X(GL_UNSIGNED_INT);
        case ImageTypeSpec::DataType::SINT32: X(GL_INT);
        case ImageTypeSpec::DataType::FLOAT32: X(GL_FLOAT);
        // check these...
        case ImageTypeSpec::DataType::UFIXED8: X(GL_UNSIGNED_BYTE);
        case ImageTypeSpec::DataType::SFIXED8: X(GL_BYTE);
        case ImageTypeSpec::DataType::UFIXED16: X(GL_UNSIGNED_SHORT);
        case ImageTypeSpec::DataType::SFIXED16: X(GL_SHORT);
        case ImageTypeSpec::DataType::UFIXED32: X(GL_UNSIGNED_INT);
        case ImageTypeSpec::DataType::SFIXED32: X(GL_INT);
    }
    #undef X
    aa_assert(false);
    return -1;
}

int getBindType(const ImageTypeSpec &spec) {
    if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL) {
        LOG_TRACE("getBindType:GL_TEXTURE_2D");
        return GL_TEXTURE_2D;
    } else if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL_EXTERNAL) {
    #ifdef ACCELERATED_ARRAYS_USE_OPENGL_ES
        LOG_TRACE("getBindType:GL_TEXTURE_EXTERNAL_OES");
        return GL_TEXTURE_EXTERNAL_OES;
    #else
        aa_assert(false && "GL_TEXTURE_EXTERNAL_OES is only available in OpenGL ES");
    #endif
    }
    aa_assert(false);
    return -1;
}

// assumed screen spec
std::unique_ptr<ImageTypeSpec> getScreenImageTypeSpec() {
    return std::unique_ptr<ImageTypeSpec>(new ImageTypeSpec(Image::getSpec(
        4, // Note: could be 3 in some circumstances... or even something else
        ImageTypeSpec::DataType::UFIXED8
    )));
}

}
}
