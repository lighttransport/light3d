#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <light3d/light3d.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Shader sources (GLSL 330 core)
// ---------------------------------------------------------------------------
static const char* kVertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 vColor;

uniform float uAngle;

void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec2 rotated = vec2(aPos.x * c - aPos.y * s,
                        aPos.x * s + aPos.y * c);
    gl_Position = vec4(rotated, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* kFragmentShaderSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint createProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------------------------------------------------------------------------
// GLFW callbacks
// ---------------------------------------------------------------------------
static void framebufferSizeCb(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

static void keyCb(GLFWwindow* window, int key, int /*scancode*/, int action,
                  int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    std::printf("Light3D v%s – OpenGL viewer example\n",
                light3d::getVersionString().c_str());

    // GLFW init
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window =
        glfwCreateWindow(800, 600, "Light3D – glview", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCb);
    glfwSetKeyCallback(window, keyCb);
    glfwSwapInterval(1); // vsync

    // GLAD init
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to initialise GLAD\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::printf("OpenGL %s, %s\n", glGetString(GL_VERSION),
                glGetString(GL_RENDERER));

    // Shader program
    GLuint program = createProgram(kVertexShaderSrc, kFragmentShaderSrc);
    if (!program) {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    GLint uAngleLoc = glGetUniformLocation(program, "uAngle");

    // Triangle vertex data: x, y, r, g, b
    // clang-format off
    float vertices[] = {
         0.0f,  0.6f,   1.0f, 0.0f, 0.0f,  // top    – red
        -0.5f, -0.4f,   0.0f, 1.0f, 0.0f,  // left   – green
         0.5f, -0.4f,   0.0f, 0.0f, 1.0f,  // right  – blue
    };
    // clang-format on

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute (location 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          nullptr);
    glEnableVertexAttribArray(0);

    // color attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float angle =
            static_cast<float>(glfwGetTime()); // ~1 radian / second

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glUniform1f(uAngleLoc, angle);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
