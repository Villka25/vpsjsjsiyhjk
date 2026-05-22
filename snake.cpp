#include <SFML/Graphics.hpp>
#include <deque>
#include <cstdlib>
#include <ctime>

const int CELL_SIZE = 20;
const int WIDTH = 30;
const int HEIGHT = 20;
const int WINDOW_W = WIDTH * CELL_SIZE;
const int WINDOW_H = HEIGHT * CELL_SIZE;

struct SnakeSegment {
    int x, y;
};

enum Direction { UP, DOWN, LEFT, RIGHT };

int main() {
    sf::RenderWindow window(sf::VideoMode(WINDOW_W, WINDOW_H), "Snake");
    window.setFramerateLimit(10); // скорость игры

    std::srand(std::time(nullptr));

    // Змейка
    std::deque<SnakeSegment> snake;
    snake.push_back({WIDTH/2, HEIGHT/2});
    snake.push_back({WIDTH/2 - 1, HEIGHT/2});
    snake.push_back({WIDTH/2 - 2, HEIGHT/2});

    // Еда
    sf::Vector2i food;
    auto placeFood = [&]() {
        bool valid = false;
        while (!valid) {
            food.x = std::rand() % WIDTH;
            food.y = std::rand() % HEIGHT;
            valid = true;
            for (const auto& seg : snake)
                if (seg.x == food.x && seg.y == food.y) {
                    valid = false;
                    break;
                }
        }
    };
    placeFood();

    Direction dir = RIGHT;
    Direction nextDir = RIGHT;
    bool gameOver = false;

    sf::RectangleShape cell(sf::Vector2f(CELL_SIZE - 2, CELL_SIZE - 2));
    cell.setFillColor(sf::Color::Green);
    sf::RectangleShape foodShape(sf::Vector2f(CELL_SIZE - 2, CELL_SIZE - 2));
    foodShape.setFillColor(sf::Color::Red);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        // Если шрифт не найден, просто не показываем счёт
        // (можно использовать стандартный или не выводить текст)
    }
    sf::Text scoreText;
    scoreText.setFont(font);
    scoreText.setCharacterSize(24);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(10, 10);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::KeyPressed) {
                if (!gameOver) {
                    if (event.key.code == sf::Keyboard::Up && dir != DOWN)
                        nextDir = UP;
                    else if (event.key.code == sf::Keyboard::Down && dir != UP)
                        nextDir = DOWN;
                    else if (event.key.code == sf::Keyboard::Left && dir != RIGHT)
                        nextDir = LEFT;
                    else if (event.key.code == sf::Keyboard::Right && dir != LEFT)
                        nextDir = RIGHT;
                }
                // Перезапуск по пробелу после проигрыша
                if (gameOver && event.key.code == sf::Keyboard::Space) {
                    snake.clear();
                    snake.push_back({WIDTH/2, HEIGHT/2});
                    snake.push_back({WIDTH/2 - 1, HEIGHT/2});
                    snake.push_back({WIDTH/2 - 2, HEIGHT/2});
                    dir = RIGHT;
                    nextDir = RIGHT;
                    gameOver = false;
                    placeFood();
                }
            }
        }

        if (!gameOver) {
            dir = nextDir;

            // Новая голова
            SnakeSegment newHead = snake.front();
            if (dir == UP)    newHead.y--;
            if (dir == DOWN)  newHead.y++;
            if (dir == LEFT)  newHead.x--;
            if (dir == RIGHT) newHead.x++;

            // Проверка столкновения со стеной
            if (newHead.x < 0 || newHead.x >= WIDTH ||
                newHead.y < 0 || newHead.y >= HEIGHT) {
                gameOver = true;
            }

            // Проверка столкновения с собой (кроме кончика хвоста, который исчезнет)
            for (size_t i = 0; i < snake.size() - 1; ++i) {
                if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
                    gameOver = true;
                    break;
                }
            }

            if (!gameOver) {
                snake.push_front(newHead);
                if (newHead.x == food.x && newHead.y == food.y) {
                    placeFood();
                } else {
                    snake.pop_back();
                }
            }
        }

        // Отрисовка
        window.clear(sf::Color::Black);

        // Еда
        foodShape.setPosition(food.x * CELL_SIZE + 1, food.y * CELL_SIZE + 1);
        window.draw(foodShape);

        // Змейка
        for (const auto& seg : snake) {
            cell.setPosition(seg.x * CELL_SIZE + 1, seg.y * CELL_SIZE + 1);
            window.draw(cell);
        }

        // Счёт
        scoreText.setString("Score: " + std::to_string(snake.size() - 3));
        window.draw(scoreText);

        if (gameOver) {
            sf::Text gameOverText;
            gameOverText.setFont(font);
            gameOverText.setString("GAME OVER\nPress SPACE to restart");
            gameOverText.setCharacterSize(30);
            gameOverText.setFillColor(sf::Color::Red);
            gameOverText.setPosition(WINDOW_W/2 - 120, WINDOW_H/2 - 40);
            window.draw(gameOverText);
        }

        window.display();
    }

    return 0;
}
