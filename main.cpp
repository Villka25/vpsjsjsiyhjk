#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>

// ----------------------------------------------------------------------
// Простые математические структуры
// ----------------------------------------------------------------------
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x+o.x, y+o.y, z+o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x-o.x, y-o.y, z-o.z); }
    Vec3 operator*(float s) const { return Vec3(x*s, y*s, z*s); }
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const {
        float l = length();
        return (l > 0.0001f) ? Vec3(x/l, y/l, z/l) : Vec3(0,0,0);
    }
};

float dot(const Vec3& a, const Vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y*b.z - a.z*b.y,
                a.z*b.x - a.x*b.z,
                a.x*b.y - a.y*b.x);
}

// ----------------------------------------------------------------------
// Камера от первого лица
// ----------------------------------------------------------------------
struct Camera {
    Vec3 pos;
    float yaw, pitch;   // углы Эйлера (радианы)
    Vec3 forward, right, up;

    Camera() : pos(0, 2, 5), yaw(-1.57f), pitch(0) {
        updateVectors();
    }

    void updateVectors() {
        forward = Vec3(std::cos(pitch) * std::cos(yaw),
                       std::sin(pitch),
                       std::cos(pitch) * std::sin(yaw)).normalized();
        right = cross(forward, Vec3(0,1,0)).normalized();
        up = cross(right, forward).normalized();
    }

    void rotate(float dyaw, float dpitch) {
        yaw += dyaw;
        pitch += dpitch;
        const float limit = 1.5f; // ограничение угла взгляда
        if (pitch > limit) pitch = limit;
        if (pitch < -limit) pitch = -limit;
        updateVectors();
    }

    void move(float forwardAmount, float rightAmount) {
        pos = pos + forward * forwardAmount + right * rightAmount;
    }
};

// ----------------------------------------------------------------------
// Структура врага
// ----------------------------------------------------------------------
struct Enemy {
    Vec3 pos;
    float speed;
    bool alive;
    Enemy(const Vec3& p) : pos(p), speed(0.02f), alive(true) {}
};

// ----------------------------------------------------------------------
// Вспомогательные функции OpenGL
// ----------------------------------------------------------------------
void drawCube(const Vec3& pos, float size, float r, float g, float b) {
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    // Передняя грань
    glVertex3f(-size, -size,  size);
    glVertex3f( size, -size,  size);
    glVertex3f( size,  size,  size);
    glVertex3f(-size,  size,  size);
    // Задняя грань
    glVertex3f(-size, -size, -size);
    glVertex3f(-size,  size, -size);
    glVertex3f( size,  size, -size);
    glVertex3f( size, -size, -size);
    // Верхняя грань
    glVertex3f(-size,  size, -size);
    glVertex3f(-size,  size,  size);
    glVertex3f( size,  size,  size);
    glVertex3f( size,  size, -size);
    // Нижняя грань
    glVertex3f(-size, -size, -size);
    glVertex3f( size, -size, -size);
    glVertex3f( size, -size,  size);
    glVertex3f(-size, -size,  size);
    // Правая грань
    glVertex3f( size, -size, -size);
    glVertex3f( size,  size, -size);
    glVertex3f( size,  size,  size);
    glVertex3f( size, -size,  size);
    // Левая грань
    glVertex3f(-size, -size, -size);
    glVertex3f(-size, -size,  size);
    glVertex3f(-size,  size,  size);
    glVertex3f(-size,  size, -size);
    glEnd();
    glPopMatrix();
}

void drawGround(float size) {
    glColor3f(0.3f, 0.5f, 0.3f);
    glBegin(GL_QUADS);
    glVertex3f(-size, 0, -size);
    glVertex3f( size, 0, -size);
    glVertex3f( size, 0,  size);
    glVertex3f(-size, 0,  size);
    glEnd();
}

// ----------------------------------------------------------------------
// Главный цикл
// ----------------------------------------------------------------------
int main() {
    // Инициализация окна
    sf::Window window(sf::VideoMode(1024, 768), "3D Shooter (Cube Invaders)", sf::Style::Default, sf::ContextSettings(24, 8, 4, 3, 3));
    window.setVerticalSyncEnabled(true);
    window.setMouseCursorVisible(false);
    window.setMouseCursorGrabbed(true);

    // Инициализация OpenGL
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glViewport(0, 0, 1024, 768);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 1024.0/768.0, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);

    // Генератор случайных чисел
    std::srand(std::time(nullptr));

    // Камера
    Camera cam;

    // Враги (10 штук)
    std::vector<Enemy> enemies;
    for (int i = 0; i < 10; ++i) {
        float x = (std::rand() % 40) - 20.0f;
        float z = (std::rand() % 40) - 20.0f;
        enemies.emplace_back(Vec3(x, 1.5f, z));
    }

    int score = 0;
    sf::Clock clock;

    // Главный цикл
    while (window.isOpen()) {
        // Время кадра
        float dt = clock.restart().asSeconds();

        // Обработка событий
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape)
                    window.close();
            }
        }

        // Управление камерой (мышь и клавиши)
        sf::Vector2i mouseDelta = sf::Mouse::getPosition(window) - sf::Vector2i(512, 384);
        sf::Mouse::setPosition(sf::Vector2i(512, 384), window);
        cam.rotate(mouseDelta.x * 0.002f, -mouseDelta.y * 0.002f);

        float speed = 5.0f * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) cam.move(speed, 0);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) cam.move(-speed, 0);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) cam.move(0, -speed);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) cam.move(0, speed);

        // Стрельба (по пробелу)
        static bool spacePressed = false;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) {
            if (!spacePressed) {
                spacePressed = true;
                // Проверяем попадание во врагов (рейкаст вперёд)
                Vec3 rayDir = cam.forward;
                Vec3 rayOrigin = cam.pos;
                float closest = 1000.0f;
                int hitIndex = -1;
                for (int i = 0; i < enemies.size(); ++i) {
                    if (!enemies[i].alive) continue;
                    Vec3 toEnemy = enemies[i].pos - rayOrigin;
                    float t = dot(toEnemy, rayDir);
                    if (t < 0) continue;
                    Vec3 closestPoint = rayOrigin + rayDir * t;
                    float dist = (enemies[i].pos - closestPoint).length();
                    if (dist < 1.5f && t < closest) {
                        closest = t;
                        hitIndex = i;
                    }
                }
                if (hitIndex >= 0) {
                    enemies[hitIndex].alive = false;
                    ++score;
                }
            }
        } else {
            spacePressed = false;
        }

        // Обновление врагов (движутся к игроку)
        for (auto& e : enemies) {
            if (!e.alive) continue;
            Vec3 dir = (cam.pos - e.pos).normalized();
            e.pos = e.pos + dir * e.speed;
        }

        // Рендеринг
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        // Применяем камеру
        gluLookAt(cam.pos.x, cam.pos.y, cam.pos.z,
                  cam.pos.x + cam.forward.x, cam.pos.y + cam.forward.y, cam.pos.z + cam.forward.z,
                  0.0, 1.0, 0.0);

        drawGround(50.0f);

        // Враги
        for (const auto& e : enemies) {
            if (e.alive)
                drawCube(e.pos, 1.0f, 1.0f, 0.0f, 0.0f);
        }

        // Прицел (крестик в центре)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glColor3f(1,1,1);
        glBegin(GL_LINES);
        glVertex2f(-0.02f, 0);
        glVertex2f( 0.02f, 0);
        glVertex2f(0, -0.02f);
        glVertex2f(0,  0.02f);
        glEnd();
        glEnable(GL_DEPTH_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        window.display();

        // Обновление заголовка с очками
        window.setTitle("3D Shooter | Score: " + std::to_string(score));
    }

    return 0;
}
