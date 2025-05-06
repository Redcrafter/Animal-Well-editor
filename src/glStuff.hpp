#pragma once

#include <fstream>
#include <span>
#include <string>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cmrc/cmrc.hpp>
CMRC_DECLARE(resources);
CMRC_DECLARE(font);

#include "image.hpp"
#include "windows/errors.hpp"

#define IM_COL32(R, G, B, A) (((uint32_t)(A) << 24) | ((uint32_t)(B) << 16) | ((uint32_t)(G) << 8) | ((uint32_t)(R)))
#define IM_COL32_WHITE IM_COL32(255, 255, 255, 255)

template<typename T>
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
        glBindVertexArray(id.value);
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
    int width = -1;
    int height = -1;

    // GLenum target;

    Texture() {
        glGenTextures(1, &id.value);
    }
    Texture(int width, int height) : width(width), height(height) {
        glGenTextures(1, &id.value);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    ~Texture() {
        glDeleteTextures(1, &id.value);
    }

    void Bind() const {
        glBindTexture(GL_TEXTURE_2D, id);
    }

    void Load(const Image& image) {
        width = image.width();
        height = image.height();

        glBindTexture(GL_TEXTURE_2D, id);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    void LoadSubImage(int x, int y, std::span<const uint8_t> data) {
        LoadSubImage(x, y, Image(data));
    }
    void LoadSubImage(int x, int y, const Image& image) {
        auto [w, h] = image.size();
        assert(x >= 0 && x + w <= width && y >= 0 && y + h <= height);

        glBindTexture(GL_TEXTURE_2D, id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
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

    Textured_Framebuffer(int width, int height) {
        fb.Bind();
        tex.Bind();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0);

        tex.width = width;
        tex.height = height;
    }

    void resize(int width, int height) {
        if(width == tex.width && height == tex.height)
            return;

        glBindTexture(GL_TEXTURE_2D, tex.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        tex.width = width;
        tex.height = height;
    }

    void Bind() {
        fb.Bind();
    }
};

struct ShaderProgram {
    Unique<GLuint> ID = 0;

    ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
        auto fs = cmrc::resources::get_filesystem();
        if(!fs.exists(vertexPath)) {
            error_dialog.pushf("File not found \"%s\"", vertexPath.c_str());
            return;
        }
        if(!fs.exists(fragmentPath)) {
            error_dialog.pushf("File not found \"%s\"", fragmentPath.c_str());
            return;
        }

        auto vs = fs.open(vertexPath);
        auto vertexCode = std::string(vs.begin(), vs.end());
        auto fs_ = fs.open(fragmentPath);
        auto fragmentCode = std::string(fs_.begin(), fs_.end());

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
        if(!success) {
            glGetShaderInfoLog(vertex, 512, nullptr, infoLog);
            error_dialog.pushf("ERROR::SHADER::VERTEX::COMPILATION_FAILED \"%s\"\n%s\n", vertexPath.c_str(), infoLog);
        }

        // similiar for Fragment Shader
        auto fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, nullptr);
        glCompileShader(fragment);
        // print compile errors if any
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if(!success) {
            glGetShaderInfoLog(fragment, 512, nullptr, infoLog);
            error_dialog.pushf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED \"%s\"\n%s\n", fragmentPath.c_str(), infoLog);
        }

        // shader Program
        ID.value = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        // print linking errors if any
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if(!success) {
            glGetProgramInfoLog(ID, 512, nullptr, infoLog);
            error_dialog.pushf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
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
    void setBool(const char* name, bool value) {
        setInt(name, value ? 1 : 0);
    }
    void setInt(const char* name, int value) {
        glUniform1i(glGetUniformLocation(ID, name), value);
    }
    void setFloat(const char* name, float value) {
        glUniform1f(glGetUniformLocation(ID, name), value);
    }
    void setVec2(const char* name, const glm::vec2& vec) {
        glUniform2f(glGetUniformLocation(ID, name), vec.x, vec.y);
    }
    void setVec3(const char* name, const glm::vec3& vec) {
        glUniform3f(glGetUniformLocation(ID, name), vec.x, vec.y, vec.z);
    }
    void setVec4(const char* name, const glm::vec4& vec) {
        glUniform4f(glGetUniformLocation(ID, name), vec.x, vec.y, vec.z, vec.w);
    }
    void setVec4v(const char* name, std::span<glm::vec4> vals) {
        glUniform4fv(glGetUniformLocation(ID, name), vals.size(), &vals[0].x);
    }
};

struct Vertex {
    glm::vec2 position;
    glm::vec2 uv;
    uint32_t color;

    Vertex(glm::vec2 position, glm::vec2 uv, uint32_t color) : position(position), uv(uv), color(color) {}
    Vertex(glm::vec2 position, glm::vec2 uv) : position(position), uv(uv), color(IM_COL32_WHITE) {}
};

struct Mesh {
    VAO vao;
    VBO vbo {GL_ARRAY_BUFFER, GL_STATIC_DRAW};

    std::vector<Vertex> data;

    Mesh() {
        vao.Bind();
        vbo.Bind();

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(4 * sizeof(float)));
    }

    void AddLine(glm::vec2 p1, glm::vec2 p2, uint32_t col = IM_COL32_WHITE, float thickness = 1) {
        auto d = glm::normalize(p2 - p1) * (thickness * 0.5f);

        data.emplace_back(glm::vec2(p1.x + d.y, p1.y - d.x), glm::vec2(), col); // tl
        data.emplace_back(glm::vec2(p2.x + d.y, p2.y - d.x), glm::vec2(), col); // tr
        data.emplace_back(glm::vec2(p1.x - d.y, p1.y + d.x), glm::vec2(), col); // bl

        data.emplace_back(glm::vec2(p2.x - d.y, p2.y + d.x), glm::vec2(), col); // br
        data.emplace_back(glm::vec2(p1.x - d.y, p1.y + d.x), glm::vec2(), col); // bl
        data.emplace_back(glm::vec2(p2.x + d.y, p2.y - d.x), glm::vec2(), col); // tr
    }

    void AddLineDashed(glm::vec2 p1, glm::vec2 p2, uint32_t col = IM_COL32_WHITE, float thickness = 1, float dashLength = 1) {
        auto delta = p2 - p1;
        auto length = glm::length(delta);
        auto steps = (int)(length / dashLength);
        delta = (delta / length) * dashLength / 2.0f;

        for(int i = 0; i < steps; ++i) {
            auto n = p1 + delta;
            AddLine(p1, n, col, thickness);
            p1 = n + delta;
        }
        // todo: draw last fractional segment
        // not important for now since dashLength and length will always bee
        // integers so there will never be any overlap
    }

    void AddRect(glm::vec2 p_min, glm::vec2 p_max, uint32_t col = IM_COL32_WHITE, float thickness = 1) {
        AddLine(p_min, glm::vec2(p_max.x, p_min.y), col, thickness);
        AddLine(glm::vec2(p_max.x, p_min.y), p_max, col, thickness);
        AddLine(p_max, glm::vec2(p_min.x, p_max.y), col, thickness);
        AddLine(glm::vec2(p_min.x, p_max.y), p_min, col, thickness);
    }
    void AddRectFilled(glm::vec2 p_min, glm::vec2 p_max, glm::vec2 uv_min, glm::vec2 uv_max, uint32_t col = IM_COL32_WHITE) {
        data.emplace_back(p_min, uv_min, col); // tl
        data.emplace_back(glm::vec2(p_max.x, p_min.y), glm::vec2(uv_max.x, uv_min.y), col); // tr
        data.emplace_back(glm::vec2(p_min.x, p_max.y), glm::vec2(uv_min.x, uv_max.y), col); // bl

        data.emplace_back(glm::vec2(p_max.x, p_min.y), glm::vec2(uv_max.x, uv_min.y), col); // tr
        data.emplace_back(p_max, uv_max, col); // br
        data.emplace_back(glm::vec2(p_min.x, p_max.y), glm::vec2(uv_min.x, uv_max.y), col); // bl
    }

    void AddRectDashed(glm::vec2 p_min, glm::vec2 p_max, uint32_t col = IM_COL32_WHITE, float thickness = 1, float dashLength = 1) {
        AddLineDashed(p_min, glm::vec2(p_max.x, p_min.y), col, thickness, dashLength);
        AddLineDashed(glm::vec2(p_max.x, p_min.y), p_max, col, thickness, dashLength);
        AddLineDashed(p_max, glm::vec2(p_min.x, p_max.y), col, thickness, dashLength);
        AddLineDashed(glm::vec2(p_min.x, p_max.y), p_min, col, thickness, dashLength);
    }

    void Buffer() {
        vbo.BufferData(data.data(), data.size() * sizeof(Vertex));
    }
    void Draw() {
        vao.Bind();
        glDrawArrays(GL_TRIANGLES, 0, data.size());
    }
    void clear() {
        data.clear();
    }
};
