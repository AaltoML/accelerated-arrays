#include "operations.hpp"
#include "adapters.hpp"
#include <GLFW/glfw3.h>
#include <string>

namespace accelerated {
namespace opengl {
namespace {
struct GLFWProcessor : Processor {
    std::unique_ptr<Processor> thread;
    GLFWwindow *window = nullptr;

    GLFWProcessor(bool visible, int w, int h, const char *t, GLFWwindow **windowOut) : thread(Processor::createThreadPool(1)) {
        std::string title = "accelerated-arrays GL window";
        if (t != nullptr) {
            title = t;
        }

        thread->enqueue([this, w, h, title, visible, windowOut]() {
            if (glfwInit()) {
                if (!visible) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
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
            // log_debug("op in GL thread");
            // not which of these are really required. It might be slow
            // to call them after each operation
            glfwMakeContextCurrent(window);
            op();
            glfwPollEvents();
        });
    }
};
}

std::unique_ptr<Processor> createGLFWProcessor() {
    return std::unique_ptr<Processor>(new GLFWProcessor(false, 640, 480, nullptr, nullptr));
}

std::unique_ptr<Processor> createGLFWWindow(int w, int h, const char *title, void **window) {
    return std::unique_ptr<Processor>(new GLFWProcessor(true, w, h, title, reinterpret_cast<GLFWwindow**>(window)));
}
}
}
