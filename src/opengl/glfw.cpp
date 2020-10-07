#include "operations.hpp"
#include "adapters.hpp"
#include <GLFW/glfw3.h>

namespace accelerated {
namespace opengl {
namespace {
struct GLFWProcessor : Processor {
    std::unique_ptr<Processor> thread;
    GLFWwindow *window = nullptr;

    GLFWProcessor() : thread(Processor::createThreadPool(1)) {
        thread->enqueue([this]() {
            if (glfwInit()) {
                glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                window = glfwCreateWindow(640, 480, "accelerated-arrays dummy GL window", NULL, NULL);
                if (!window) glfwTerminate();
                log_debug("GLFWProcessor initialized window");
            }
        });
    }

    ~GLFWProcessor() {
        thread->enqueue([this]() {
            if (window) {
                glfwDestroyWindow(window);
                glfwTerminate();
                log_debug("GLFWProcessor destroyed window");
            }
        }).wait();
    }

    Future enqueue(const std::function<void()> &op) final {
        return thread->enqueue([this, op]() {
            assert(window);
            // not which of these are really required
            log_debug("op in GL thread");
            glfwMakeContextCurrent(window);
            op();
            glfwPollEvents();
        });
    }
};
}

std::unique_ptr<Processor> createGLFWProcessor() {
    return std::unique_ptr<Processor>(new GLFWProcessor());
}
}
}
