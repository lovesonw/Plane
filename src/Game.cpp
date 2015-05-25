#include "Game.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#define PlayerSpeed 1000.f
#define TimePerFrame sf::seconds(1.f / 60.f)

Game::Game()
: mWindow(sf::VideoMode(640,480),"SFML Application")
, mPlayer(),mIsMovingUp(false),mIsMovingDown(false),mIsMovingLeft(false),mIsMovingRight(false)
{
   if (!mTexture.loadFromFile("Textures/Eagle.png"))
   {
   // error
   }
   mPlayer.setTexture(mTexture);
   mPlayer.setPosition(100.f, 100.f);
}

void Game::run()
{
    sf::Clock clock;
    sf::Time timeSinceLastUpdate = sf::Time::Zero;
    while (mWindow.isOpen())
    {
        //sf::Time deltaTime = clock.restart();
        processevents();
        timeSinceLastUpdate += clock.restart();
        while (timeSinceLastUpdate > TimePerFrame)
        {
            timeSinceLastUpdate -=TimePerFrame;
            processevents();
            update(TimePerFrame);
        }
        render();
    }
}

void Game::handlePlayerInput(sf::Keyboard::Key key,bool isPressed)
{
    if (key == sf::Keyboard::Up)
        mIsMovingUp = isPressed;
    else if (key == sf::Keyboard::Down)
        mIsMovingDown = isPressed;
    else if (key == sf::Keyboard::Left)
        mIsMovingLeft = isPressed;
    else if (key == sf::Keyboard::Right)
        mIsMovingRight = isPressed;
}

void Game::processevents()
{
    sf::Event event;
    while (mWindow.pollEvent(event))
    {
        switch (event.type)
        {
        case sf::Event::KeyPressed:
            handlePlayerInput(event.key.code,true);
            break;
        case sf::Event::KeyReleased:
            handlePlayerInput(event.key.code,false);
            break;
        case sf::Event::Closed:
            mWindow.close();
            break;
        }
    }
}

void Game::update(sf::Time deltaTime)
{
    sf::Vector2f movement(0.f, 0.f);
    if (mIsMovingUp)
        movement.y -= PlayerSpeed;
    if (mIsMovingDown)
        movement.y += PlayerSpeed;
    if (mIsMovingLeft)
        movement.x -= PlayerSpeed;
    if (mIsMovingRight)
        movement.x += PlayerSpeed;
    mPlayer.move(movement * deltaTime.asSeconds());
}

void Game::render()
{
    mWindow.clear();
    mWindow.draw(mPlayer);
    mWindow.display();
}
