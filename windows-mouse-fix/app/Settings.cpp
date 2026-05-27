//
// Settings.cpp
// Uses a minimal hand-rolled JSON parser to avoid external dependencies.
// For a production build, replace with nlohmann/json.
//

#include "Settings.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <string>

#pragma comment(lib, "shell32.lib")

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

std::wstring Settings::appDataDir() {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return std::wstring(path) + L"\\WindowsMouseFix";
    }
    return L".";
}

std::wstring Settings::configPath() {
    return appDataDir() + L"\\config.json";
}

// ---------------------------------------------------------------------------
// Minimal JSON helpers (no external deps)
// ---------------------------------------------------------------------------

static std::string readFile(const std::wstring& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool writeFile(const std::wstring& path, const std::string& content) {
    // Ensure directory exists
    std::wstring dir = path.substr(0, path.rfind(L'\\'));
    CreateDirectoryW(dir.c_str(), nullptr);

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return true;
}

// Very simple JSON value extractor — finds "key": value in flat JSON
static bool jsonGetFloat(const std::string& json, const std::string& key, float& out) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    try { out = std::stof(json.substr(pos)); return true; }
    catch (...) { return false; }
}

static bool jsonGetBool(const std::string& json, const std::string& key, bool& out) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (json.substr(pos, 4) == "true")  { out = true;  return true; }
    if (json.substr(pos, 5) == "false") { out = false; return true; }
    return false;
}

// ---------------------------------------------------------------------------
// load / save
// ---------------------------------------------------------------------------

bool Settings::load() {
    std::string json = readFile(configPath());
    if (json.empty()) return false; // use defaults

    jsonGetFloat(json, "speedMultiplier",     speedMultiplier);
    jsonGetBool (json, "accelerationEnabled", accelerationEnabled);
    jsonGetBool (json, "naturalScroll",       naturalScroll);
    jsonGetBool (json, "autoStart",           autoStart);
    jsonGetBool (json, "enabled",             enabled);

    // Clamp speedMultiplier
    if (speedMultiplier < 0.1f) speedMultiplier = 0.1f;
    if (speedMultiplier > 10.f) speedMultiplier = 10.f;

    return true;
}

bool Settings::save() const {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\n"
        "  \"speedMultiplier\": %.2f,\n"
        "  \"accelerationEnabled\": %s,\n"
        "  \"naturalScroll\": %s,\n"
        "  \"autoStart\": %s,\n"
        "  \"enabled\": %s\n"
        "}\n",
        speedMultiplier,
        accelerationEnabled ? "true" : "false",
        naturalScroll       ? "true" : "false",
        autoStart           ? "true" : "false",
        enabled             ? "true" : "false"
    );
    return writeFile(configPath(), buf);
}

// ---------------------------------------------------------------------------
// Auto-start registry
// ---------------------------------------------------------------------------

bool Settings::setAutoStart(bool enable) const {
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey
    );
    if (result != ERROR_SUCCESS) return false;

    const wchar_t* valueName = L"WindowsMouseFix";

    if (enable) {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        result = RegSetValueExW(
            hKey, valueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(exePath),
            static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t))
        );
    } else {
        result = RegDeleteValueW(hKey, valueName);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}
