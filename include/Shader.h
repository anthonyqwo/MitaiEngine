#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "AssetPath.h"

class Shader {
public:
    unsigned int ID;
    bool hasTessellation;

    // constructor generates the shader on the fly
    Shader(const char* computePath) {
        hasTessellation = false;
        std::string computeCode = readShaderSource(computePath);

        const char* cCode = computeCode.c_str();
        unsigned int compute = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(compute, 1, &cCode, NULL);
        glCompileShader(compute);
        checkCompileErrors(compute, "COMPUTE");

        ID = glCreateProgram();
        glAttachShader(ID, compute);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(compute);
    }

    Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr, const char* tessControlPath = nullptr, const char* tessEvalPath = nullptr) {
        hasTessellation = (tessControlPath != nullptr || tessEvalPath != nullptr);
        // 1. retrieve the vertex/fragment source code from filePath
        std::string vertexCode, fragmentCode, geometryCode, tessControlCode, tessEvalCode;
        vertexCode = readShaderSource(vertexPath);
        fragmentCode = readShaderSource(fragmentPath);
        if (geometryPath) geometryCode = readShaderSource(geometryPath);
        if (tessControlPath) tessControlCode = readShaderSource(tessControlPath);
        if (tessEvalPath) tessEvalCode = readShaderSource(tessEvalPath);

        unsigned int vertex, fragment, geometry, tessControl, tessEval;
        auto compileShader = [&](unsigned int& shader, const char* code, GLenum type, const char* name) {
            shader = glCreateShader(type);
            glShaderSource(shader, 1, &code, NULL);
            glCompileShader(shader);
            checkCompileErrors(shader, name);
        };

        compileShader(vertex, vertexCode.c_str(), GL_VERTEX_SHADER, "VERTEX");
        compileShader(fragment, fragmentCode.c_str(), GL_FRAGMENT_SHADER, "FRAGMENT");
        
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);

        if (geometryPath) {
            compileShader(geometry, geometryCode.c_str(), GL_GEOMETRY_SHADER, "GEOMETRY");
            glAttachShader(ID, geometry);
        }
        if (tessControlPath) {
            compileShader(tessControl, tessControlCode.c_str(), GL_TESS_CONTROL_SHADER, "TESS_CONTROL");
            glAttachShader(ID, tessControl);
        }
        if (tessEvalPath) {
            compileShader(tessEval, tessEvalCode.c_str(), GL_TESS_EVALUATION_SHADER, "TESS_EVALUATION");
            glAttachShader(ID, tessEval);
        }

        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex); glDeleteShader(fragment);
        if (geometryPath) glDeleteShader(geometry);
        if (tessControlPath) glDeleteShader(tessControl);
        if (tessEvalPath) glDeleteShader(tessEval);
    }

    // activate the shader
    void use() const {
        glUseProgram(ID);
    }

    // utility uniform functions
    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
    }
    void setVec2(const std::string& name, const glm::vec2& value) const {
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
    }

private:
    static std::string readShaderSource(const char* path) {
        if (!path) return "";

        std::filesystem::path shaderPath = AssetPath::resolve(path);
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        try {
            file.open(shaderPath);
            std::stringstream stream;
            stream << file.rdbuf();
            file.close();

            return expandIncludes(stream.str(), shaderPath.parent_path());
        } catch (std::ifstream::failure& e) {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << path << " " << e.what() << std::endl;
            return "";
        }
    }

    static std::string expandIncludes(const std::string& source, const std::filesystem::path& baseDir) {
        std::stringstream input(source);
        std::stringstream output;
        std::string line;

        while (std::getline(input, line)) {
            std::string trimmed = line;
            size_t first = trimmed.find_first_not_of(" \t");
            if (first != std::string::npos) trimmed = trimmed.substr(first);

            if (trimmed.rfind("#include", 0) == 0) {
                size_t begin = trimmed.find('"');
                size_t end = trimmed.find('"', begin + 1);
                if (begin != std::string::npos && end != std::string::npos && end > begin + 1) {
                    std::filesystem::path includePath = baseDir / trimmed.substr(begin + 1, end - begin - 1);
                    output << readShaderSource(includePath.string().c_str()) << "\n";
                    continue;
                }
            }

            output << line << "\n";
        }

        return output.str();
    }

    // utility function for checking shader compilation/linking errors.
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};
#endif
