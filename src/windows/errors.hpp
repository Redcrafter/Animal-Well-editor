#pragma once

#include <string>
#include <vector>
#include <format>

struct ErrorInfo {
    bool error;
    std::string text;
};

class ErrorDialog {
    std::vector<ErrorInfo> errors;

  public:
    void drawPopup();

    void error(std::string text) {
        errors.push_back(ErrorInfo {true, std::move(text) });
    }
    void warning(std::string text) {
        errors.push_back(ErrorInfo {false, std::move(text) });
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        error(std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    void clear() {
        errors.clear();
    }
};

inline ErrorDialog error_dialog;
