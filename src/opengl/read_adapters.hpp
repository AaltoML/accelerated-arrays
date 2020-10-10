#pragma once
#include "operations.hpp"
#include "image.hpp"

namespace accelerated {
namespace opengl {
namespace operations {
std::function<Future(std::uint8_t*)> createReadAdpater(
    Image &image,
    Processor &processor,
    Image::Factory &imageFactory,
    operations::Factory &opFactory);
}
}
}
