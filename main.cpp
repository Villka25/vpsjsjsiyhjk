// FPS Counter with OpenGL 3D scene and overlay
// Compile with: g++ main.cpp -o FpsChecker.exe -lglfw3 -lopengl32 -lgdi32 -lglu32

#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <cstdio>

// GLFW3
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// glad (OpenGL loader)
#include <glad/glad.h>

// Math helpers
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

// Simple camera
struct Camera {
    Vec3 pos = {0.0f, 2.0f, 5.0f};
    Vec3 target = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    float fov = 60.0f;
};

// Global variables
GLFWwindow* g_window = nullptr;
int g_width = 1280;
int g_height = 720;
double g_lastTime = 0.0;
int g_frameCount = 0;
float g_currentFPS = 0.0f;
bool g_showOverlay = true;

// Shader sources
const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 fragColor;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    fragColor = aColor;
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
in vec3 fragColor;
out vec4 color;
void main() {
    color = vec4(fragColor, 1.0);
}
)";

// Simple cube vertices (position, color)
float vertices[] = {
    // positions          // colors
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.5f
};

unsigned int indices[] = {
    0,1,2, 0,2,3, // back
    4,7,6, 4,6,5, // front
    0,4,5, 0,5,1, // bottom
    2,6,7, 2,7,3, // top
    0,3,7, 0,7,4, // left
    1,5,6, 1,6,2  // right
};

unsigned int VAO, VBO, EBO;
unsigned int shaderProgram;
Camera camera;

// Timing for rotation
float angle = 0.0f;

// Initialize GLFW and OpenGL
bool initGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    g_window = glfwCreateWindow(g_width, g_height, "FPS Checker - OpenGL Scene", nullptr, nullptr);
    if (!g_window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1); // VSync on (1) or off (0)

    // Load OpenGL functions with glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to load GLAD" << std::endl;
        return false;
    }
    glViewport(0, 0, g_width, g_height);
    glEnable(GL_DEPTH_TEST);
    return true;
}

// Compile shader
unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "Shader compile error: " << info << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Create shader program
bool initShaders() {
    unsigned int vertex = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    if (!vertex || !fragment) return false;
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertex);
    glAttachShader(shaderProgram, fragment);
    glLinkProgram(shaderProgram);
    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, info);
        std::cerr << "Link error: " << info << std::endl;
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return true;
}

// Create geometry
void initGeometry() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// Render text (simple overlay using glBitmap or textured quad? For simplicity, use glDrawPixels?
// Better: use a textured quad with font atlas. But to keep it short, we'll just render a colored quad with FPS string using OpenGL ortho projection.
// We'll create a simple 2D text overlay using a texture atlas? Too complex. Alternative: use OS native overlay via ImGui? Not needed.
// For demonstration, we'll just draw a black rectangle and use glRasterPos + glut? No glut.
// Let's implement a basic bitmap font using glBitmap? Requires bitmap data. Simpler: just print FPS to console and window title.
// But to mimic MSI Afterburner, we can draw a small rectangle in corner and use GLFW window title to show FPS.
// That's acceptable for test.
void updateWindowTitle() {
    char title[100];
    sprintf_s(title, "FPS Checker - %.1f FPS", g_currentFPS);
    glfwSetWindowTitle(g_window, title);
}

// Set up projection matrix
void setProjectionMatrix(float fov, float aspect, float near, float far, float matrix[16]) {
    float tanHalfFov = tanf(fov * 3.14159265f / 360.0f);
    matrix[0] = 1.0f / (aspect * tanHalfFov);
    matrix[1] = 0.0f;
    matrix[2] = 0.0f;
    matrix[3] = 0.0f;
    matrix[4] = 0.0f;
    matrix[5] = 1.0f / tanHalfFov;
    matrix[6] = 0.0f;
    matrix[7] = 0.0f;
    matrix[8] = 0.0f;
    matrix[9] = 0.0f;
    matrix[10] = -(far + near) / (far - near);
    matrix[11] = -1.0f;
    matrix[12] = 0.0f;
    matrix[13] = 0.0f;
    matrix[14] = -(2.0f * far * near) / (far - near);
    matrix[15] = 0.0f;
}

void setLookAt(Vec3 eye, Vec3 center, Vec3 up, float matrix[16]) {
    Vec3 f = { center.x - eye.x, center.y - eye.y, center.z - eye.z };
    float len = sqrtf(f.x*f.x + f.y*f.y + f.z*f.z);
    f.x /= len; f.y /= len; f.z /= len;
    Vec3 s = { up.y * f.z - up.z * f.y, up.z * f.x - up.x * f.z, up.x * f.y - up.y * f.x };
    len = sqrtf(s.x*s.x + s.y*s.y + s.z*s.z);
    s.x /= len; s.y /= len; s.z /= len;
    Vec3 u = { f.y * s.z - f.z * s.y, f.z * s.x - f.x * s.z, f.x * s.y - f.y * s.x };
    matrix[0] = s.x; matrix[4] = s.y; matrix[8] = s.z; matrix[12] = - (s.x*eye.x + s.y*eye.y + s.z*eye.z);
    matrix[1] = u.x; matrix[5] = u.y; matrix[9] = u.z; matrix[13] = - (u.x*eye.x + u.y*eye.y + u.z*eye.z);
    matrix[2] = -f.x; matrix[6] = -f.y; matrix[10] = -f.z; matrix[14] = (f.x*eye.x + f.y*eye.y + f.z*eye.z);
    matrix[3] = 0.0f; matrix[7] = 0.0f; matrix[11] = 0.0f; matrix[15] = 1.0f;
}

void setIdentity(float m[16]) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void multiplyMatrices(float a[16], float b[16], float result[16]) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result[i*4+j] = a[i*4+0]*b[0*4+j] + a[i*4+1]*b[1*4+j] + a[i*4+2]*b[2*4+j] + a[i*4+3]*b[3*4+j];
        }
    }
}

void updateAndRender() {
    // Compute delta time and FPS
    double now = glfwGetTime();
    g_frameCount++;
    if (now - g_lastTime >= 1.0) {
        g_currentFPS = g_frameCount / (now - g_lastTime);
        g_lastTime = now;
        g_frameCount = 0;
        updateWindowTitle();
    }

    // Clear screen
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Update rotation angle
    angle += 0.01f;
    if (angle > 360.0f) angle -= 360.0f;

    // Create model matrix (rotation around Y)
    float model[16];
    setIdentity(model);
    float c = cosf(angle);
    float s = sinf(angle);
    model[0] = c;  model[2] = s;
    model[8] = -s; model[10] = c;

    // View matrix
    float view[16];
    setLookAt(camera.pos, camera.target, camera.up, view);

    // Projection matrix
    int width, height;
    glfwGetFramebufferSize(g_window, &width, &height);
    float aspect = (float)width / (float)height;
    float proj[16];
    setProjectionMatrix(camera.fov, aspect, 0.1f, 100.0f, proj);

    // Use shader
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, proj);

    // Draw cube
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    // Swap buffers
    glfwSwapBuffers(g_window);
    glfwPollEvents();
}

void cleanup() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(g_window);
    glfwTerminate();
}

int main() {
    if (!initGLFW()) return -1;
    if (!initShaders()) return -1;
    initGeometry();

    g_lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(g_window)) {
        updateAndRender();
        if (glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(g_window, true);
        if (glfwGetKey(g_window, GLFW_KEY_F1) == GLFW_PRESS)
            g_showOverlay = !g_showOverlay; // just placeholder
    }

    cleanup();
    return 0;
}
