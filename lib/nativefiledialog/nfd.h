#pragma once
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

namespace NFD {

enum class Result {
	Error, /* programmatic error */
	Okay,  /* user pressed okay, or successful return */
	Cancel /* user pressed cancel */
};

struct FilterItem {
	std::string Name;
	std::vector<std::string> Extensions;
};

using FilterList = std::vector<FilterItem>;

/* single file open dialog */
Result OpenDialog(const FilterList& filterList, const char* defaultPath, std::string& outPath, GLFWwindow* parent);

/* multiple file open dialog */
Result OpenDialogMultiple(const FilterList& filterList, const char* defaultPath, std::vector<std::string>& outPaths, GLFWwindow* parent);

/* save dialog */
Result SaveDialog(const FilterList& filterList, const char* defaultPath, std::string& outPath, GLFWwindow* parent);

/* select folder dialog */
Result PickFolder(const char* defaultPath, std::string& outPath, GLFWwindow* parent);

/* get last error -- set when Result is NFD_ERROR */
std::string GetError();

}
