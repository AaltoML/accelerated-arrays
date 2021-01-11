# Accelerated arrays

An array programming library focused on image processing & computer vision on mobile GPUs.

## Main concepts

The classes described below are defined in the namespace `accelerated`,
e.g., `Image` refers to `accelerated::Image`.

### Image

An `Image` represents a mutable array of data with dimensions `image.width` × `image.height` × `image.channels`, where the number of channels is 1, 2, 3 or 4.

#### Channels

The available channel numbers correspond to the available vector types in the GLSL language.
Even though they most traditionally represent the color channels R,G,B, and A, they can be used for any data. For example, a 2-channel image can be used to represent optical flow, where each pixel has components Δx and Δy.

#### Data types

All scalars in the image have the same data type. The data types can be referred to in two different ways: The `Image::DataType` enum (stored in `image.dataType`) or C++ types (when creating an image or accessing pixels). Examples of available data types:

 * `Image::DataType::FLOAT32` / `float`: single-prevision floating point
 * `Image::DataType::SINT16` / `std::int18_t`: 16-bit signed integer
 * `Image::DataTYpe::UFIXED8` / `FixedPoint<std::uint8_t>`: 8-bit unsigned fixed-point number in the range [0, 1], i.e., one of the values 0, 1/255, 2/255, ..., 254/255, 1.

#### Storage types

The `Image` may be backed either by
 * An array of data in CPU-side RAM (`cpu::Image` class / `Image::StorageType::CPU`). In this case, it can be
   accessed directly and synchronously through methods like `image.get<float>(x, y, channel)`
 * An OpenGL texture (`opengl::Image` / `Image::StorageType::GPU_OPENGL_*`). In this case, the pixels are not directly accessible from C++.

The type is available in `image.storageType`.
The pixel data of an image can be transferred from/to normal CPU RAM using the asynchronous `read` and `write` operations, but these may not
be available for all GPU textures and in some cases, you may have to write the data to another type of texture before reading it, especially on OpenGL ES.

### Type spec

`ImageTypeSpec` consists of the `channels`, `dataType` and `storageType` of the `Image`. They can be used to define operations on certain types of images before the actual `Image`s exist. However, if you already have an `Image`, you can use it in place of any `ImageTypeSpec`.

### Function

Images are primarily modified by `Function`s, which should be always thought of as asynchronous operations. A function takes zero (`Nullary`), one (`Unary`), or more (`NAry`) input `Image`s and writes to a single output `Image`.

The return value from calling the function, available on the CPU side is a `Future`. It is possible to block the current thread and wait for the operation represented by the function to complete by calling the `.wait()` method of the returned future. However, calling wait is not the only option and something you want to avoid doing in the OpenGL thread.

A generic `Function` is assumed to be `NAry` and currently the easiest way of calling simpler functions is using the `operations::call*` helpers.

### Processor

`Processor`s are in charge of completing `Function`s or other asynchronous operations. They are given as references to factories (see below) or can be used directly for enqueuing custom `std::function<void()>`s as `processor.enqueue([]() { /* do something */ })` in a thread-safe manner.

The following options are available for creating a processor:

 * `Processor::createInstant())`: dummy processor that runs every operation right away. Makes sense for certain CPU-based processing and testing.
 * `Processor::createThreadPool(n)`: a thread pool with `n` threads. With `n=1` the enqueued operations are processed in order, which is convenient in many cases.
 * `Processor::createQueue()`: Returns (a unique ptr of) a `Queue`, a subclass that does not automatically process anything, but the user must manually facilitate processing by calling `queue.processAll()` (or `processOne`), which can happen in another thread than the one(s) that enqueued the operations.
 * `opengl::createGLFWProcessor()` an easy way of creating a (headless) OpenGL GPU processor in commandline applications. Also `createGLFWWindow` is available for rendering to screen.

### Factories

#### Image factory

`Image`s are created using an `Image::Factory`. There are two implementations

 * `cpu::Image::createFactory()` returns a `cpu::Image::Factory` factory that builds `cpu::Image`s, with, the following methods
    - `create<std::uint8_t, 3>(width, height)` (new image)
    - `createReference<std::int16_t, 2>(width, height, ptrToExistingData)` (reference to existing data)
 * `opengl::Image::createFactory(Processor &)` returns an `opengl::Image:Factory` with these methods
    - `create<std::uint16_t, 2>(widht, height)` create a new OpenGL texture and Frame Buffer Object (of type `GL_RG16UI` in this example
    - `wrapTexture<FixedPoint<std::uint8_t>, 3>(textureId, width, height)` create a read-only reference to an existing texture (of type `GL_RGB8` in this case)
    - `wrapFrameBuffer<FixedPoint<std::int8_t>, 4>(fboId, width, height)` create a write-only reference to an existing texture (of type `GL_RGBA8_SNORM` in this case).
    - `wrapScreen(width, height)` create write-only reference to the screen, assuming it exists, has the given dimensions, and is of type `GL_RGBA8`.

#### Operation factory

`Function`s are defined using an "operation factory". Two implementations exist:

 * `cpu::operations::createFactory(Processor &)` for CPU operations. Has a method `wrap` for converting synchronous operations to `Functions`.
 * `opengl::operations::createFactory(Processor &)` for GPU operations. Has a method `wrapShader(fragShaderBody, inputTypeSpec, outputTypeSpec)` for creating GLSL shader operations directly.

Certain "standard" functions are available for both implementations through the `operations::StandardFactory` interface. The standard operations are usually defined using a "spec" / "builder" and the `ImageTypeSpec` (part).

## Building

```bash
mkdir target
cd target
cmake ..
make
./test/run-tests
```

See the `CMakeLists.txt` files for available options.

## Examples

Basic example using the optional OpenCV adapter
```c++
#include <accelerated-arrays/cpu/operations.hpp>
#include <accelerated-arrays/opencv_adapter.hpp>
#include <opencv2/imgcodecs.hpp>

void main() {
    using namespace accelerated;
    cv::Mat inputCvMat = cv::imread("input.png");

    auto imgFactory = cpu::Image::createFactory();
    auto inputImage = opencv::ref(inputCvMat);
    auto outputImage = imgFactory->createLike(*inputImage);

    auto processor = Processor::createThreadPool(1);
    auto opFactory = cpu::operations::createFactory(*processor);

    auto invert = opFactory->channelwiseAffine(-1, 255).build(*inputImage, *outputImage);
    operations::callUnary(invert, *inputImage, *outputImage).wait();

    cv::imwrite("output.png", opencv::ref(*outputImage));
}
```
See the `test/` folder for more examples.
