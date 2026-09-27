#pragma once
#include <map>
#include <string>
// Adapter contract fake only: this does not model ESP32 flash/NVS durability.
namespace fake {
extern std::map<std::string, std::map<std::string, std::string>> storage;
extern bool openOk, clearOk;
extern unsigned opens, clears, closes;
extern std::string openedNamespace;
}
class Preferences {
public:
    bool begin(const char* name, bool readOnly) {
        ++fake::opens;
        fake::openedNamespace = name;
        return !readOnly && fake::openOk;
    }
    bool clear() {
        ++fake::clears;
        if (!fake::clearOk) return false;
        fake::storage[fake::openedNamespace].clear();
        return true;
    }
    void end() { ++fake::closes; }
};
