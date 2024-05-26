#include "nfd.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace NFD;

static std::string g_errorstr;

/* public routines */
std::string NFD::GetError() {
	return g_errorstr;
}

/* internal routines */
static void NFDi_SetError(const char* msg) {
	g_errorstr = msg;
}

#ifdef _WIN32
#include <shobjidl.h>
#include <wchar.h>
#include <windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#ifdef __MINGW32__
// Explicitly setting NTDDI version, this is necessary for the MinGW compiler
#define NTDDI_VERSION NTDDI_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#define _CRTDBG_MAP_ALLOC

/* only locally define UNICODE in this compilation unit */
#ifndef UNICODE
#define UNICODE
#endif

static BOOL COMIsInitialized(HRESULT coResult) {
	if(coResult == RPC_E_CHANGED_MODE) {
		// If COM was previously initialized with different init flags,
		// NFD still needs to operate. Eat this warning.
		return TRUE;
	}

	return SUCCEEDED(coResult);
}

static HRESULT COMInit() {
	return CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
}

static void COMUninit(HRESULT coResult) {
	// do not uninitialize if RPC_E_CHANGED_MODE occurred -- this
	// case does not refcount COM.
	if(SUCCEEDED(coResult))
		CoUninitialize();
}

static std::string WCharToChar(const wchar_t* inStr) {
	int inStrCharacterCount = static_cast<int>(wcslen(inStr));
	int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, inStr, inStrCharacterCount, nullptr, 0, nullptr, nullptr);
	assert(bytesNeeded);
	bytesNeeded += 1;

	auto chars = new char[bytesNeeded];

	WideCharToMultiByte(CP_UTF8, 0, inStr, -1, chars, bytesNeeded, nullptr, nullptr);

	std::string ret = chars;
	delete[] chars;
	return ret;
}

// allocs the space in outStr -- call free()
static void CopyNFDCharToWChar(const char* inStr, wchar_t** outStr) {
	int inStrByteCount = static_cast<int>(strlen(inStr));
	int charsNeeded = MultiByteToWideChar(CP_UTF8, 0, inStr, inStrByteCount, nullptr, 0);
	assert(!*outStr);
	charsNeeded += 1; // terminator

	*outStr = new wchar_t[charsNeeded];

	int ret = MultiByteToWideChar(CP_UTF8, 0, inStr, inStrByteCount, *outStr, charsNeeded);
	(*outStr)[charsNeeded - 1] = '\0';
}

static Result AddFiltersToDialog(IFileDialog* fileOpenDialog, const std::vector<FilterItem>& filterList) {
	if(filterList.empty()) {
		return Result::Okay;
	}

	std::vector<COMDLG_FILTERSPEC> specList;
	for(const auto& filter : filterList) {
		wchar_t* name = nullptr;
		wchar_t* spec = nullptr;

		std::string ext;

		for(int i = 0; i < filter.Extensions.size(); ++i) {
			if(i > 0) {
				ext += ";";
			}
			ext += "*" + filter.Extensions[i];
		}

		CopyNFDCharToWChar(filter.Name.c_str(), &name);
		CopyNFDCharToWChar(ext.c_str(), &spec);

		specList.push_back({ name, spec });
	}

	/* Add wildcard */
	specList.push_back({ L"All Files", L"*.*" });

	fileOpenDialog->SetFileTypes(specList.size(), specList.data());

	/* free speclist */
	for(size_t i = 0; i < specList.size() - 1; ++i) {
		delete[] specList[i].pszName;
		delete[] specList[i].pszSpec;
	}

	return Result::Okay;
}

static Result AllocPathSet(IShellItemArray* shellItems, std::vector<std::string>& outPaths) {
	const char ERRORMSG[] = "Error allocating pathset.";

	assert(shellItems);

	// How many items in shellItems?
	DWORD numShellItems;
	HRESULT result = shellItems->GetCount(&numShellItems);
	if(!SUCCEEDED(result)) {
		NFDi_SetError(ERRORMSG);
		return Result::Error;
	}

	/* fill buf */
	for(DWORD i = 0; i < numShellItems; ++i) {
		IShellItem* shellItem;
		result = shellItems->GetItemAt(i, &shellItem);
		if(!SUCCEEDED(result)) {
			NFDi_SetError(ERRORMSG);
			return Result::Error;
		}

		// Confirm SFGAO_FILESYSTEM is true for this shellitem, or ignore it.
		SFGAOF attribs;
		result = shellItem->GetAttributes(SFGAO_FILESYSTEM, &attribs);
		if(!SUCCEEDED(result)) {
			NFDi_SetError(ERRORMSG);
			return Result::Error;
		}
		if(!(attribs & SFGAO_FILESYSTEM))
			continue;

		LPWSTR name;
		shellItem->GetDisplayName(SIGDN_FILESYSPATH, &name);

		std::string cstring = WCharToChar(name);
		outPaths.push_back(cstring);
	}

	return Result::Okay;
}

static Result SetDefaultPath(IFileDialog* dialog, const char* defaultPath) {
	if(!defaultPath || strlen(defaultPath) == 0)
		return Result::Okay;

	wchar_t* defaultPathW = nullptr;
	CopyNFDCharToWChar(defaultPath, &defaultPathW);

	IShellItem* folder;
	HRESULT result = SHCreateItemFromParsingName(defaultPathW, nullptr, IID_PPV_ARGS(&folder));

	// Valid non results.
	if(result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || result == HRESULT_FROM_WIN32(ERROR_INVALID_DRIVE)) {
		delete defaultPathW;
		return Result::Okay;
	}

	if(!SUCCEEDED(result)) {
		NFDi_SetError("Error creating ShellItem");
		delete defaultPathW;
		return Result::Error;
	}

	// Could also call SetDefaultFolder(), but this guarantees defaultPath -- more consistency across API.
	dialog->SetFolder(folder);

	delete defaultPathW;
	folder->Release();

	return Result::Okay;
}

/* public */
Result NFD::OpenDialog(const std::vector<FilterItem>& filterList, const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	Result nfdResult = Result::Error;

	HRESULT coResult = COMInit();
	if(!COMIsInitialized(coResult)) {
		NFDi_SetError("Could not initialize COM.");
		return nfdResult;
	}

	// Create dialog
	IFileOpenDialog* fileOpenDialog = nullptr;
	HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&fileOpenDialog));
	HWND wind = nullptr;

	if(!SUCCEEDED(result)) {
		NFDi_SetError("Could not create dialog.");
		goto end;
	}

	// Build the filter list
	if(AddFiltersToDialog(fileOpenDialog, filterList) != Result::Okay) {
		goto end;
	}

	// Set the default path
	if(SetDefaultPath(fileOpenDialog, defaultPath) != Result::Okay) {
		goto end;
	}

	if(parent) {
		wind = glfwGetWin32Window(parent);
	}

	// Show the dialog.
	result = fileOpenDialog->Show(wind);
	if(SUCCEEDED(result)) {
		// Get the file name
		IShellItem* shellItem(nullptr);
		result = fileOpenDialog->GetResult(&shellItem);
		if(!SUCCEEDED(result)) {
			NFDi_SetError("Could not get shell item from dialog.");
			goto end;
		}
		wchar_t* filePath(NULL);
		result = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
		if(!SUCCEEDED(result)) {
			NFDi_SetError("Could not get file path for selected.");
			shellItem->Release();
			goto end;
		}

		outPath = WCharToChar(filePath);
		CoTaskMemFree(filePath);

		nfdResult = Result::Okay;
		shellItem->Release();
	} else if(result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
		nfdResult = Result::Cancel;
	} else {
		NFDi_SetError("File dialog box show failed.");
		nfdResult = Result::Error;
	}

end:
	if(fileOpenDialog)
		fileOpenDialog->Release();

	COMUninit(coResult);

	return nfdResult;
}

Result NFD::OpenDialogMultiple(const std::vector<FilterItem>& filterList, const char* defaultPath, std::vector<std::string>& outPaths, GLFWwindow* parent) {
	Result nfdResult = Result::Error;

	HRESULT coResult = COMInit();
	if(!COMIsInitialized(coResult)) {
		NFDi_SetError("Could not initialize COM.");
		return nfdResult;
	}

	// Create dialog
	IFileOpenDialog* fileOpenDialog(nullptr);
	HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&fileOpenDialog));
	HWND wind = nullptr;

	if(!SUCCEEDED(result)) {
		fileOpenDialog = nullptr;
		NFDi_SetError("Could not create dialog.");
		goto end;
	}

	// Build the filter list
	if(AddFiltersToDialog(fileOpenDialog, filterList) != Result::Okay) {
		goto end;
	}

	// Set the default path
	if(SetDefaultPath(fileOpenDialog, defaultPath) != Result::Okay) {
		goto end;
	}

	// Set a flag for multiple options
	DWORD dwFlags;
	result = fileOpenDialog->GetOptions(&dwFlags);
	if(!SUCCEEDED(result)) {
		NFDi_SetError("Could not get options.");
		goto end;
	}
	result = fileOpenDialog->SetOptions(dwFlags | FOS_ALLOWMULTISELECT);
	if(!SUCCEEDED(result)) {
		NFDi_SetError("Could not set options.");
		goto end;
	}

	if(parent) {
		wind = glfwGetWin32Window(parent);
	}

	// Show the dialog.
	result = fileOpenDialog->Show(wind);
	if(SUCCEEDED(result)) {
		IShellItemArray* shellItems;
		result = fileOpenDialog->GetResults(&shellItems);
		if(!SUCCEEDED(result)) {
			NFDi_SetError("Could not get shell items.");
			goto end;
		}

		std::string str;

		if(AllocPathSet(shellItems, outPaths) == Result::Error) {
			shellItems->Release();
			goto end;
		}

		shellItems->Release();
		nfdResult = Result::Okay;
	} else if(result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
		nfdResult = Result::Cancel;
	} else {
		NFDi_SetError("File dialog box show failed.");
		nfdResult = Result::Error;
	}

end:
	if(fileOpenDialog)
		fileOpenDialog->Release();

	COMUninit(coResult);

	return nfdResult;
}

Result NFD::SaveDialog(const std::vector<FilterItem>& filterList, const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	Result nfdResult = Result::Error;

	HRESULT coResult = COMInit();
	if(!COMIsInitialized(coResult)) {
		NFDi_SetError("Could not initialize COM.");
		return nfdResult;
	}

	// Create dialog
	IFileSaveDialog* fileSaveDialog(nullptr);
	HRESULT result = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_IFileSaveDialog, reinterpret_cast<void**>(&fileSaveDialog));
	HWND wind = nullptr;

	if(!SUCCEEDED(result)) {
		fileSaveDialog = nullptr;
		NFDi_SetError("Could not create dialog.");
		goto end;
	}

	// Build the filter list
	if(AddFiltersToDialog(fileSaveDialog, filterList) != Result::Okay) {
		goto end;
	}

	// Set the default path
	if(SetDefaultPath(fileSaveDialog, defaultPath) != Result::Okay) {
		goto end;
	}

	if(parent) {
		wind = glfwGetWin32Window(parent);
	}

	// Show the dialog.
	result = fileSaveDialog->Show(wind);
	if(SUCCEEDED(result)) {
		// Get the file name
		::IShellItem* shellItem;
		result = fileSaveDialog->GetResult(&shellItem);
		if(!SUCCEEDED(result)) {
			NFDi_SetError("Could not get shell item from dialog.");
			goto end;
		}
		wchar_t* filePath(nullptr);
		result = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
		if(!SUCCEEDED(result)) {
			shellItem->Release();
			NFDi_SetError("Could not get file path for selected.");
			goto end;
		}

		outPath = WCharToChar(filePath);
		CoTaskMemFree(filePath);

		nfdResult = Result::Okay;
		shellItem->Release();
	} else if(result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
		nfdResult = Result::Cancel;
	} else {
		NFDi_SetError("File dialog box show failed.");
		nfdResult = Result::Error;
	}

end:
	if(fileSaveDialog)
		fileSaveDialog->Release();

	COMUninit(coResult);

	return nfdResult;
}

Result NFD::PickFolder(const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	Result nfdResult = Result::Error;
	DWORD dwOptions = 0;

	HRESULT coResult = COMInit();
	if(!COMIsInitialized(coResult)) {
		NFDi_SetError("CoInitializeEx failed.");
		return nfdResult;
	}

	// Create dialog
	IFileOpenDialog* fileDialog(nullptr);
	HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileDialog));
	HWND wind = nullptr;

	if(!SUCCEEDED(result)) {
		NFDi_SetError("CoCreateInstance for CLSID_FileOpenDialog failed.");
		goto end;
	}

	// Set the default path
	if(SetDefaultPath(fileDialog, defaultPath) != Result::Okay) {
		NFDi_SetError("SetDefaultPath failed.");
		goto end;
	}

	// Get the dialogs options
	if(!SUCCEEDED(fileDialog->GetOptions(&dwOptions))) {
		NFDi_SetError("GetOptions for IFileDialog failed.");
		goto end;
	}

	// Add in FOS_PICKFOLDERS which hides files and only allows selection of folders
	if(!SUCCEEDED(fileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS))) {
		NFDi_SetError("SetOptions for IFileDialog failed.");
		goto end;
	}

	if(parent) {
		wind = glfwGetWin32Window(parent);
	}

	// Show the dialog to the user
	result = fileDialog->Show(wind);
	if(SUCCEEDED(result)) {
		// Get the folder name
		::IShellItem* shellItem(nullptr);

		result = fileDialog->GetResult(&shellItem);
		if(!SUCCEEDED(result)) {
			NFDi_SetError("Could not get file path for selected.");
			shellItem->Release();
			goto end;
		}

		wchar_t* path = nullptr;
		result = shellItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &path);
		if(!SUCCEEDED(result)) {
			NFDi_SetError("GetDisplayName for IShellItem failed.");
			shellItem->Release();
			goto end;
		}

		outPath = WCharToChar(path);
		CoTaskMemFree(path);

		nfdResult = Result::Okay;
		shellItem->Release();
	} else if(result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
		nfdResult = Result::Cancel;
	} else {
		NFDi_SetError("Show for IFileDialog failed.");
		nfdResult = Result::Error;
	}

end:

	if(fileDialog)
		fileDialog->Release();

	COMUninit(coResult);

	return nfdResult;
}
#elif defined(__linux)
#include <gtk-2.0/gtk/gtk.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

const char INIT_FAIL_MSG[] = "gtk_init_check failed to initilaize GTK+";

static void AddFiltersToDialog(GtkWidget* dialog, const std::vector<FilterItem>& filterList) {
	if(filterList.empty()) {
		return;
	}

	GtkFileFilter* filter = nullptr;
	for(const auto& f : filterList) {
		filter = gtk_file_filter_new();
		gtk_file_filter_set_name(filter, f.Name.c_str());
		for(const auto& ext : f.Extensions) {
			auto name = "*." + ext;
			gtk_file_filter_add_pattern(filter, name.c_str());
		}

		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	}

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All Files");
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
}

static void SetDefaultPath(GtkWidget* dialog, const char* defaultPath) {
	if(!defaultPath || strlen(defaultPath) == 0)
		return;

	/* GTK+ manual recommends not specifically setting the default path.
	   We do it anyway in order to be consistent across platforms.

	   If consistency with the native OS is preferred, this is the line
	   to comment out. -ml */
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), defaultPath);
}

static void WaitForCleanup() {
	while(gtk_events_pending())
		gtk_main_iteration();
}

/* public */

Result NFD::OpenDialog(const std::vector<FilterItem>& filterList, const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	GtkWidget* dialog;
	Result result;

	if(!gtk_init_check(NULL, NULL)) {
		NFDi_SetError(INIT_FAIL_MSG);
		return Result::Error;
	}

	// glfwGetX11Window(window) only returns a X11 window which can't be used as a parent
	dialog = gtk_file_chooser_dialog_new("Open File", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);

	/* Build the filter list */
	AddFiltersToDialog(dialog, filterList);

	/* Set the default path */
	SetDefaultPath(dialog, defaultPath);

	result = Result::Cancel;
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char* filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		outPath = filename;
		g_free(filename);

		result = Result::Okay;
	}

	WaitForCleanup();
	gtk_widget_destroy(dialog);
	WaitForCleanup();

	return result;
}

Result NFD::OpenDialogMultiple(const std::vector<FilterItem>& filterList, const char* defaultPath, std::vector<std::string>& outPaths, GLFWwindow* parent) {
	GtkWidget* dialog;
	Result result;

	if(!gtk_init_check(NULL, NULL)) {
		NFDi_SetError(INIT_FAIL_MSG);
		return Result::Error;
	}

	dialog = gtk_file_chooser_dialog_new("Open Files", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

	/* Build the filter list */
	AddFiltersToDialog(dialog, filterList);

	/* Set the default path */
	SetDefaultPath(dialog, defaultPath);

	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		GSList* fileList = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

		GSList* node;

		/* fill buf */
		for(node = fileList; node; node = node->next) {
			outPaths.push_back((char*)node->data);
			g_free(node->data);
		}

		g_slist_free(fileList);
		result = Result::Okay;
	} else {
		result = Result::Cancel;
	}

	WaitForCleanup();
	gtk_widget_destroy(dialog);
	WaitForCleanup();

	return result;
}

Result NFD::SaveDialog(const std::vector<FilterItem>& filterList, const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	GtkWidget* dialog;
	Result result;

	if(!gtk_init_check(NULL, NULL)) {
		NFDi_SetError(INIT_FAIL_MSG);
		return Result::Error;
	}

	dialog = gtk_file_chooser_dialog_new("Save File", NULL, GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	/* Build the filter list */
	AddFiltersToDialog(dialog, filterList);

	/* Set the default path */
	SetDefaultPath(dialog, defaultPath);

	result = Result::Cancel;
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char* filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		outPath = filename;
		g_free(filename);

		result = Result::Okay;
	}

	WaitForCleanup();
	gtk_widget_destroy(dialog);
	WaitForCleanup();

	return result;
}

Result NFD::PickFolder(const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	if(!gtk_init_check(NULL, NULL)) {
		NFDi_SetError(INIT_FAIL_MSG);
		return Result::Error;
	}

	GtkWidget* dialog = gtk_file_chooser_dialog_new("Select folder", NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	/* Set the default path */
	SetDefaultPath(dialog, defaultPath);

	Result result = Result::Cancel;
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char* filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		outPath = filename;
		g_free(filename);

		result = Result::Okay;
	}

	WaitForCleanup();
	gtk_widget_destroy(dialog);
	WaitForCleanup();

	return result;
}

#else
#pragma warning("No nfd found")
// TODO: use imgui to draw dialog

Result OpenDialog(const std::vector<FilterItem>& filterList, const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	NFDi_SetError("No native framework found");
	return Result::Error;
}

Result OpenDialogMultiple(const std::vector<FilterItem>& filterList, const char* defaultPath, std::vector<std::string>& outPaths, GLFWwindow* parent) {
	NFDi_SetError("No native framework found");
	return Result::Error;
}

Result SaveDialog(const std::vector<FilterItem>& filterList, const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	NFDi_SetError("No native framework found");
	return Result::Error;
}

Result PickFolder(const char* defaultPath, std::string& outPath, GLFWwindow* parent) {
	NFDi_SetError("No native framework found");
	return Result::Error;
}

#endif
