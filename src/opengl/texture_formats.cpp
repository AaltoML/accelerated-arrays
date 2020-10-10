#include <sstream>

#include "adapters.hpp"
#include "../image.hpp"

namespace accelerated {
namespace opengl {
int getTextureInternalFormat(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getInternalFormat:%s", #x); return x

    // could be tried as a fallback but not preferrable
    /*
    if (useUnsizedFormats) {
        switch (spec.channels) {
            case 1: X(GL_RED);
            case 2: X(GL_RG);
            case 3: X(GL_RGB);
            case 4: X(GL_RGBA);
            default: assert(false); break;
        }
    }
    */

    if (spec.channels == 1) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::UINT8: X(GL_R8UI);
            case ImageTypeSpec::DataType::SINT8: X(GL_R8I);
            case ImageTypeSpec::DataType::UINT16: X(GL_R16UI);
            case ImageTypeSpec::DataType::SINT16: X(GL_R16I);
            case ImageTypeSpec::DataType::UINT32: X(GL_R32UI);
            case ImageTypeSpec::DataType::SINT32: X(GL_R32I);
            case ImageTypeSpec::DataType::FLOAT32: X(GL_R32F);
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
            default: break;
        }
    }

    /*
    const bool allowLossy = true;
    #define LOSSY(x) assert(allowLossy && #x); X(x)

    if (spec.channels == 1) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::FLOAT32: X(GL_R32F);
            case ImageTypeSpec::DataType::UINT8: X(GL_R8);
            case ImageTypeSpec::DataType::SINT8: X(GL_R8_SNORM);
        #ifdef IS_OPENGL_ES
            case ImageTypeSpec::DataType::UINT16: LOSSY(GL_R16F);
            case ImageTypeSpec::DataType::SINT16: LOSSY(GL_R16F);
        #else
            case ImageTypeSpec::DataType::UINT16: X(GL_R16);
            case ImageTypeSpec::DataType::SINT16: X(GL_R16_SNORM);
        #endif
            case ImageTypeSpec::DataType::UINT32: LOSSY(GL_R32F);
            case ImageTypeSpec::DataType::SINT32: LOSSY(GL_R32F);
            default: break;
        }
    }
    else if (spec.channels == 2) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::FLOAT32: X(GL_RG32F);
            case ImageTypeSpec::DataType::UINT8: X(GL_RG8);
            case ImageTypeSpec::DataType::SINT8: X(GL_RG8_SNORM);
        #ifdef IS_OPENGL_ES
            case ImageTypeSpec::DataType::UINT16: LOSSY(GL_RG16F);
            case ImageTypeSpec::DataType::SINT16: LOSSY(GL_RG16F);
        #else
            case ImageTypeSpec::DataType::UINT16: X(GL_RG16);
            case ImageTypeSpec::DataType::SINT16: X(GL_RG16_SNORM);
        #endif
            case ImageTypeSpec::DataType::UINT32: LOSSY(GL_RG32F);
            case ImageTypeSpec::DataType::SINT32: LOSSY(GL_RG32F);
            default: break;
        }
    }
    else if (spec.channels == 3) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::FLOAT32: X(GL_RGB32F);
            case ImageTypeSpec::DataType::UINT8: X(GL_RGB8);
            case ImageTypeSpec::DataType::SINT8: X(GL_RGB8_SNORM);
        #ifdef IS_OPENGL_ES
            case ImageTypeSpec::DataType::UINT16: LOSSY(GL_RGB16F);
            case ImageTypeSpec::DataType::SINT16: LOSSY(GL_RGB16F);
        #else
            case ImageTypeSpec::DataType::UINT16: X(GL_RGB16);
            case ImageTypeSpec::DataType::SINT16: X(GL_RGB16_SNORM);
        #endif
            case ImageTypeSpec::DataType::UINT32: LOSSY(GL_RGB32F);
            case ImageTypeSpec::DataType::SINT32: LOSSY(GL_RGB32F);
            default: break;
        }
    }
    else if (spec.channels == 4) {
        switch (spec.dataType) {
            case ImageTypeSpec::DataType::FLOAT32: X(GL_RGBA32F);
            case ImageTypeSpec::DataType::UINT8: X(GL_RGBA8);
            case ImageTypeSpec::DataType::SINT8: X(GL_RGBA8_SNORM);
        #ifdef IS_OPENGL_ES
            case ImageTypeSpec::DataType::UINT16: LOSSY(GL_RGBA16F);
            case ImageTypeSpec::DataType::SINT16: LOSSY(GL_RGBA16F);
        #else
            case ImageTypeSpec::DataType::UINT16: X(GL_RGBA16);
            case ImageTypeSpec::DataType::SINT16: X(GL_RGBA16_SNORM);
        #endif
            case ImageTypeSpec::DataType::UINT32: LOSSY(GL_RGBA32F);
            case ImageTypeSpec::DataType::SINT32: LOSSY(GL_RGBA32F);
            default: break;
        }
    }

    #undef LOSSY
    */
    assert(false && "no suitable internal format");
    #undef X
    assert(false && "not implemented");
    return -1;
}

int getCpuFormat(const ImageTypeSpec &spec) {
    #define X(x) LOG_TRACE("getCpuFormat:%s", #x); return x
    // TODO: also support fixed-point types
    if (spec.dataType == ImageTypeSpec::DataType::FLOAT32) {
        switch (spec.channels) {
            case 1: X(GL_RED);
            case 2: X(GL_RG);
            case 3: X(GL_RGB);
            case 4: X(GL_RGBA);
            default: break;
        }
    } else {
        switch (spec.channels) {
            case 1: X(GL_RED_INTEGER);
            case 2: X(GL_RG_INTEGER);
            case 3: X(GL_RGB_INTEGER);
            case 4: X(GL_RGBA_INTEGER);
            default: break;
        }
    }
    #undef X
    assert(false);
    return -1;
}

std::string getGlslSamplerType(const ImageTypeSpec &spec) {
    if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL_EXTERNAL) {
        assert(spec.dataType == ImageTypeSpec::DataType::UINT8);
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
    switch (spec.channels) {
        case 1: X(GL_RED);
        case 2:
            log_warn("OpenGL spec does not allow reading 2-channel textures directly");
            X(GL_RG);
        case 3: X(GL_RGB);
        case 4: X(GL_RGBA);
        default: break;
    }
    #undef X
    assert(false);
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
    }
    #undef X
    assert(false);
    return -1;
}

int getBindType(const ImageTypeSpec &spec) {
    if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL) {
        LOG_TRACE("getBindType:GL_TEXTURE_2D");
        return GL_TEXTURE_2D;
    } else if (spec.storageType == ImageTypeSpec::StorageType::GPU_OPENGL_EXTERNAL) {
        LOG_TRACE("getBindType:GL_TEXTURE_EXTERNAL_OES");
        return GL_TEXTURE_EXTERNAL_OES;
    }
    assert(false);
    return -1;
}

}
}