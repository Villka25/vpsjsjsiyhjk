// FPS Checker – OpenGL сцена + оверлей в заголовке окна (800x600, фиксированное окно)
#include <iostream>
#include <chrono>
#include <cmath>
#include <cstdio>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "glad.h"

// ----------------------------------------------------------------------
// Простые математические структуры
// ----------------------------------------------------------------------
struct Vec3 { float x, y, z; };
struct Camera {
    Vec3 pos = {0.0f, 2.0f, 5.0f};
    Vec3 target = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    float fov = 60.0f;
};

// ----------------------------------------------------------------------
// Глобальные переменные
// ----------------------------------------------------------------------
GLFWwindow* g_window = nullptr;
int g_width = 800, g_height = 600;          // ← изменено на 800x600
double g_lastTime = 0.0;
int g_frameCount = 0;
float g_currentFPS = 0.0f;

GLuint g_vao = 0, g_vbo = 0, g_ebo = 0, g_shaderProgram = 0;
Camera g_camera;
float g_angle = 0.0f;

// ----------------------------------------------------------------------
// Шейдеры (GLSL 330)
// ----------------------------------------------------------------------
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

// ----------------------------------------------------------------------
// Данные куба (позиция + цвет)
// ----------------------------------------------------------------------
float vertices[] = {
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
    0,1,2, 0,2,3,
    4,7,6, 4,6,5,
    0,4,5, 0,5,1,
    2,6,7, 2,7,3,
    0,3,7, 0,7,4,
    1,5,6, 1,6,2
};

// ----------------------------------------------------------------------
// Вспомогательные функции
// ----------------------------------------------------------------------
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "Shader error: " << info << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool initShaders() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    if (!vs || !fs) return false;
    g_shaderProgram = glCreateProgram();
    glAttachShader(g_shaderProgram, vs);
    glAttachShader(g_shaderProgram, fs);
    glLinkProgram(g_shaderProgram);
    int success;
    glGetProgramiv(g_shaderProgram, GL_LINK_STATUS, &success);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(g_shaderProgram, 512, nullptr, info);
        std::cerr << "Link error: " << info << std::endl;
        return false;
    }
    return true;
}

void initGeometry() {
    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glGenBuffers(1, &g_ebo);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void setProjectionMatrix(float fov, float aspect, float near, float far, float* matrix) {
    float tanHalf = tanf(fov * 3.14159265f / 360.0f);
    matrix[0] = 1.0f / (aspect * tanHalf);
    matrix[5] = 1.0f / tanHalf;
    matrix[10] = -(far + near) / (far - near);
    matrix[11] = -1.0f;
    matrix[14] = -(2.0f * far * near) / (far - near);
    // остальные обнуляем
    for (int i = 0; i < 16; ++i) {
        if (i != 0 && i != 5 && i != 10 && i != 11 && i != 14 && i != 15)
            matrix[i] = 0.0f;
    }
    matrix[15] = 1.0f;
}

void setLookAt(const Vec3& eye, const Vec3& center, const Vec3& up, float* matrix) {
    Vec3 f = { center.x - eye.x, center.y - eye.y, center.z - eye.z };
    float len = sqrtf(f.x*f.x + f.y*f.y + f.z*f.z);
    f.x /= len; f.y /= len; f.z /= len;
    Vec3 s = { up.y * f.z - up.z * f.y, up.z * f.x - up.x * f.z, up.x * f.y - up.y * f.x };
    len = sqrtf(s.x*s.x + s.y*s.y + s.z*s.z);
    s.x /= len; s.y /= len; s.z /= len;
    Vec3 u = { f.y * s.z - f.z * s.y, f.z * s.x - f.x * s.z, f.x * s.y - f.y * s.x };
    matrix[0] = s.x;  matrix[4] = s.y;  matrix[8]  = s.z;  matrix[12] = -(s.x*eye.x + s.y*eye.y + s.z*eye.z);
    matrix[1] = u.x;  matrix[5] = u.y;  matrix[9]  = u.z;  matrix[13] = -(u.x*eye.x + u.y*eye.y + u.z*eye.z);
    matrix[2] = -f.x; matrix[6] = -f.y; matrix[10] = -f.z; matrix[14] =  (f.x*eye.x + f.y*eye.y + f.z*eye.z);
    matrix[3] = 0.0f; matrix[7] = 0.0f; matrix[11] = 0.0f; matrix[15] = 1.0f;
}

void setIdentity(float* m) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void updateWindowTitle() {
    char title[128];
    snprintf(title, sizeof(title), "FPS Checker - %.1f FPS", g_currentFPS);
    glfwSetWindowTitle(g_window, title);
}

void render() {
    // FPS счётчик
    double now = glfwGetTime();
    g_frameCount++;
    if (now - g_lastTime >= 1.0) {
        g_currentFPS = g_frameCount / (now - g_lastTime);
        g_lastTime = now;
        g_frameCount = 0;
        updateWindowTitle();
    }

    int width, height;
    glfwGetFramebufferSize(g_window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Вращение куба
    g_angle += 0.01f;
    float model[16];
    setIdentity(model);
    float c = cosf(g_angle), s = sinf(g_angle);
    model[0] = c;  model[2] = s;
    model[8] = -s; model[10] = c;

    // Вид
    float view[16];
    setLookAt(g_camera.pos, g_camera.target, g_camera.up, view);

    // Проекция
    float proj[16];
    setProjectionMatrix(g_camera.fov, (float)width/(float)height, 0.1f, 100.0f, proj);

    glUseProgram(g_shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "model"), 1, GL_FALSE, model);
    glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "projection"), 1, GL_FALSE, proj);

    glBindVertexArray(g_vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    glfwSwapBuffers(g_window);
    glfwPollEvents();
}

bool initGLFW() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // ← запрет изменения размера
    g_window = glfwCreateWindow(g_width, g_height, "FPS Checker", nullptr, nullptr);
    if (!g_window) { glfwTerminate(); return false; }
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);  // VSync on – для честного FPS (можно выключить, поставив 0)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;
    glEnable(GL_DEPTH_TEST);
    return true;
}

int main() {
    if (!initGLFW()) return -1;
    if (!initShaders()) return -1;
    initGeometry();

    g_lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(g_window)) {
        render();
        if (glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(g_window, true);
    }

    glDeleteVertexArrays(1, &g_vao);
    glDeleteBuffers(1, &g_vbo);
    glDeleteBuffers(1, &g_ebo);
    glDeleteProgram(g_shaderProgram);
    glfwDestroyWindow(g_window);
    glfwTerminate();
    return 0;
}
