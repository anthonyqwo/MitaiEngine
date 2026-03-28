#include "Application.h"
#include <iostream>

int main() {
    Application app;
    if (app.init()) {
        app.loadCollisionDemoScene();
        app.run();
    } else {
        std::cerr << "Failed to initialize game engine application." << std::endl;
        return -1;
    }
    return 0;
}
