#include "operations.hpp"
#include "adapters.hpp"
#include "../assert.hpp"
#include "../log.hpp"

#if defined(__APPLE__)
#define GLFW_INCLUDE_GLCOREARB // Select gl3.h within glfw3.h
#endif
#include <GLFW/glfw3.h>
#include <string>

namespace accelerated {
namespace opengl {
namespace {
struct GLFWProcessor : Processor {
    std::unique_ptr<Processor> processor;
    GLFWwindow *window = nullptr;

    GLFWProcessor(bool visible, int w, int h, const char *t, GLFWwindow **windowOut, GLFWProcessorMode mode) {
        if (mode == GLFWProcessorMode::AUTO) {
        #ifdef ACCELERATED_ARRAYS_SYNC_GLFW
            log_warn("Falling back to syncrhonous GLFW processor");
            mode = GLFWProcessorMode::SYNC;
        #else
            mode = GLFWProcessorMode::ASYNC;
        #endif
        }

        switch (mode) {
        case GLFWProcessorMode::SYNC:
            log_debug("Initializing syncrhonous GLFW processor");
            processor = Processor::createInstant();
            break;
        case GLFWProcessorMode::ASYNC:
        #ifdef ACCELERATED_ARRAYS_SYNC_GLFW
            aa_assert(false && "GLFWProcessorMode::ASYNC is not supported on your system");
        #endif
            log_debug("Initializing GLFW processor with its own thread");
            processor = Processor::createThreadPool(1);
            break;
        default:
            aa_assert(false);
        }

        std::string title = "accelerated-arrays GL window";
        if (t != nullptr) {
            title = t;
        }

        processor->enqueue([this, w, h, title, visible, windowOut]() {
            if (glfwInit()) {
                if (!visible) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

                #ifdef __APPLE__
                // We need to explicitly ask for specific version context on OS X
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                #endif

                window = glfwCreateWindow(w, h, title.c_str(), NULL, NULL);
                if (!window) glfwTerminate();
                log_debug("GLFWProcessor initialized window");
                if (windowOut != nullptr) {
                    log_debug("setting window reference");
                    *windowOut = window;
                }
            }
        });
    }

    ~GLFWProcessor() {
        processor->enqueue([this]() {
            if (window) {
                glfwDestroyWindow(window);
                glfwTerminate();
                log_debug("GLFWProcessor destroyed window");
            }
        }).wait();
    }

    Future enqueue(const std::function<void()> &op) final {
        return processor->enqueue([this, op]() {
            aa_assert(window);
            // log_debug("op in GL thread");
            // not which of these are really required. It might be slow
            // to call them after each operation
            glfwMakeContextCurrent(window);
            op();
            glfwPollEvents();
        });
    }
};

constexpr int DEFAULT_W = 640;
constexpr int DEFAULT_H = 480;
}

std::unique_ptr<Processor> createGLFWProcessor(GLFWProcessorMode mode) {
    return std::unique_ptr<Processor>(new GLFWProcessor(false, DEFAULT_W, DEFAULT_H, nullptr, nullptr, mode));
}

std::unique_ptr<Processor> createGLFWWindow(int w, int h, const char *title, GLFWProcessorMode mode, void **window) {
    return std::unique_ptr<Processor>(new GLFWProcessor(true, w, h, title, reinterpret_cast<GLFWwindow**>(window), mode));
}
}
}
