#include "Game.h"

#include <exception>
#include <iostream>

int main() {
    try {
        Game game;
        game.Run();
    } catch (const std::exception& error) {
        std::cerr << "Hunter's Assassin: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
