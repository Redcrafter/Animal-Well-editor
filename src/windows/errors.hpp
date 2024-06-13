#pragma once

#include <string>
#include <vector>

class ErrorDialog {
    std::vector<std::string> errors;

  public:
    void draw();

    void push(const std::string& error);
    void pushf(const char* fmt, ...);

    void clear();
};

inline ErrorDialog error_dialog;
