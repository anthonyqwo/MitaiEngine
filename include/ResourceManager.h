#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <map>
#include <string>
#include <glad/glad.h>
#include "Shader.h"

class ResourceManager {
public:
    static std::map<std::string, Shader*>    Shaders;
    static std::map<std::string, unsigned int> Textures;
    
    static Shader* loadShader(const char *vShaderFile, const char *fShaderFile, const char *gShaderFile, const char *tcShaderFile, const char *teShaderFile, std::string name);
    static Shader* getShader(std::string name);
    
    static unsigned int loadTexture(const char *file, std::string name);
    static unsigned int getTexture(std::string name);
    
    static unsigned int createSolidTexture(unsigned char r, unsigned char g, unsigned char b, std::string name);
    
    static void clear();
};

#endif
