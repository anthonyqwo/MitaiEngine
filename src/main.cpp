#include "Application.h"
#include "AssetPath.h"
#include <iostream>

int main() {
    AssetPath::makeRootCurrentDirectory();

    Application app;
    if (app.init()) {
        app.loadAIHuntingScene();
        app.run();
    } else {
        std::cerr << "Failed to initialize game engine application." << std::endl;
        return -1;
    }
    return 0;
}
