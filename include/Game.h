#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#ifndef GAME_H
#define GAME_H

class Game
{
    public:
        Game();
        void run();

    private:
        void processevents();
        void update(sf::Time deltaTime);
        void render();
        void handlePlayerInput(sf::Keyboard::Key key,bool isPressed);
    private:
        sf::RenderWindow mWindow;
        sf::Texture mTexture;
        sf::Sprite mPlayer;
    private:
        bool mIsMovingUp,mIsMovingDown,mIsMovingLeft,mIsMovingRight;
};
#endif // GAME_H
