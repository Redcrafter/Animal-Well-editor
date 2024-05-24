#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <span>

#include <glad/gl.h>
#include <stb_image.h>
#include <glm/glm.hpp>

template <typename T>
struct Unique {
    T value;

    Unique(const T& val) {
        value = val;
    }
    Unique(Unique&& other) noexcept {
        value = other.value;
        other.value = 0;
    }

    Unique(const Unique&) = delete;
    Unique& operator=(const Unique&) = delete;

    operator T() const {
        return value;
    }
};

static_assert(sizeof(Unique<GLuint>) == sizeof(GLuint));

struct VAO {
    Unique<GLuint> id = 0;
    VAO() {
        glGenVertexArrays(1, &id.value);
    }
    ~VAO() {
        glDeleteVertexArrays(1, &id.value);
    }

    void Bind() const {
        glBindVertexArray(id);
    }
};

struct VBO {
    Unique<GLuint> id = 0;
    GLenum target;
    GLenum usage;

    VBO(GLenum target, GLenum usage) {
        this->target = target;
        this->usage = usage;

        glGenBuffers(1, &id.value);
    }
    ~VBO() {
        glDeleteBuffers(1, &id.value);
    }

    void Bind() const {
        glBindBuffer(target, id);
    }
    void BufferData(const void* data, int dataSize) {
        glBindBuffer(target, id);
        glBufferData(target, dataSize, data, usage);
    }
};

struct Texture {
    Unique<GLuint> id = 0;
    int width;
    int height;

    // GLenum target;

    Texture() {
        glGenTextures(1, &id.value);
    }

    /*
    Texture(GLenum target, GLint internalformat, int width, int height, GLenum format, GLenum type, void* pixels) {
        glGenTextures(1, &id.value);
        glTexImage2D(target, 0, internalformat, width, height, 0, format, type, pixels);
        this->width = width;
        this->height = height;
    }
    */
    /*explicit Texture(const char* filename) {
        glGenTextures(1, &id.value);
        Load(filename);
    }*/

    ~Texture() {
        glDeleteTextures(1, &id.value);
    }

    void Bind() const {
        glBindTexture(GL_TEXTURE_2D, id);
    }

    void Load(const std::vector<uint8_t>& data) {
        int n;
        auto* dat = stbi_load_from_memory(data.data(), data.size(), &width, &height, &n, 4);
        if (dat == nullptr) {
            throw std::runtime_error("missing texture");
        }

        glBindTexture(GL_TEXTURE_2D, id);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, dat);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        stbi_image_free(dat);
    }

    void LoadSubImage(int layer, std::span<const uint8_t> data) {
        int n;
        auto* dat = stbi_load_from_memory(data.data(), data.size(), &width, &height, &n, 4);
        if(dat == nullptr) {
            throw std::runtime_error("missing texture");
        }

        glBindTexture(GL_TEXTURE_2D, id);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, dat);

        stbi_image_free(dat);
    }
};

struct Framebuffer {
    Unique<GLuint> id = 0;

    Framebuffer() {
        glGenFramebuffers(1, &id.value);
    }
    ~Framebuffer() {
        glDeleteFramebuffers(1, &id.value);
    }

    void Bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, id);
    }
};

struct Textured_Framebuffer {
    Framebuffer fb;
    Texture tex;
    GLint internal_format;

    Textured_Framebuffer(int width, int height, GLint internal_format) : internal_format(internal_format) {
        fb.Bind();
        tex.Bind();

        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0);
    }

    void resize(int width, int height) {
        if(width == tex.width && height == tex.height)
            return;

        glBindTexture(GL_TEXTURE_2D, tex.id);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    }

    void Bind() {
        fb.Bind();
    }
};

struct ShaderProgram {
    Unique<GLuint> ID = 0;

    ShaderProgram(std::string vertexPath, std::string fragmentPath) {
        // 1. retrieve the vertex/fragment source code from filePath
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        // ensure ifstream objects can throw exceptions:
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        try {
            // open files
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            // read file's buffer contents into streams
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            // close file handlers
            vShaderFile.close();
            fShaderFile.close();
            // convert stream into string
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        } catch (std::ifstream::failure& e) {
            printf("ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ\n");
        }

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        // 2. compile shaders
        int success;
        char infoLog[512];

        // vertex Shader
        auto vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, nullptr);
        glCompileShader(vertex);
        // print compile errors if any
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertex, 512, nullptr, infoLog);
            printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
        }

        // similiar for Fragment Shader
        auto fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, nullptr);
        glCompileShader(fragment);
        // print compile errors if any
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment, 512, nullptr, infoLog);
            printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
        }

        // shader Program
        ID.value = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        // print linking errors if any
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(ID, 512, nullptr, infoLog);
            printf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
        }

        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
    ~ShaderProgram() {
        glDeleteProgram(ID);
    }

    void Use() const {
        glUseProgram(ID);
    }

    void setMat4(const char* name, const glm::mat4& mat) {
        glUniformMatrix4fv(glGetUniformLocation(ID, name), 1, GL_FALSE, &mat[0][0]);
    }
    void setInt(const char* name, int value) {
        glUniform1i(glGetUniformLocation(ID, name), value);
    }
    void setVec3(const std::string& name, const glm::vec3& vec) {
        setVec3(name.c_str(), vec);
    }
    void setVec3(const char* name, const glm::vec3& vec) {
        glUniform3f(glGetUniformLocation(ID, name), vec.x, vec.y, vec.z);
    }
    void setVec4(const std::string& name, const glm::vec4& vec) {
        setVec4(name.c_str(), vec);
    }
    void setVec4(const char* name, const glm::vec4& vec) {
        glUniform4f(glGetUniformLocation(ID, name), vec.x, vec.y, vec.z, vec.w);
    }
};
