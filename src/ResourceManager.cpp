#include "ResourceManager.h"
#include <iostream>
#include <stb_image.h>

std::map<std::string, Shader*>    ResourceManager::Shaders;
std::map<std::string, unsigned int> ResourceManager::Textures;

Shader* ResourceManager::loadShader(const char *vShaderFile, const char *fShaderFile, const char *gShaderFile, const char *tcShaderFile, const char *teShaderFile, std::string name) {
    Shaders[name] = new Shader(vShaderFile, fShaderFile, gShaderFile, tcShaderFile, teShaderFile);
    return Shaders[name];
}

Shader* ResourceManager::getShader(std::string name) {
    return Shaders[name];
}

unsigned int ResourceManager::loadTexture(const char *file, std::string name) {
    unsigned int tid; glGenTextures(1, &tid); int w,h,c; unsigned char *d = stbi_load(file, &w, &h, &c, 0);
    if(d){ GLenum f=(c==1)?GL_RED:(c==3?GL_RGB:GL_RGBA); glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, d); glGenerateMipmap(GL_TEXTURE_2D); }
    else { unsigned char fb[]={180,180,180}; glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, fb); }
    stbi_image_free(d); Textures[name] = tid; return tid;
}

unsigned int ResourceManager::getTexture(std::string name) {
    // 若找不到，應回傳一組預設錯誤圖，這裡直接回傳
    if (Textures.find(name) == Textures.end()) return 0;
    return Textures[name];
}

unsigned int ResourceManager::createSolidTexture(unsigned char r, unsigned char g, unsigned char b, std::string name) {
    unsigned int tid; glGenTextures(1, &tid); unsigned char d[]={r,g,b}; glBindTexture(GL_TEXTURE_2D, tid); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, d); Textures[name] = tid; return tid;
}

void ResourceManager::clear() {
    for (auto iter : Shaders)
        delete iter.second;
    for (auto iter : Textures)
        glDeleteTextures(1, &iter.second);
}
