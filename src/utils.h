#pragma once
#ifndef CATMULL_CLARK_SUBDIVITION_UTILS_H_
#define CATMULL_CLARK_SUBDIVITION_UTILS_H_

#include <exception>
#include <fstream>
#include <string>
#include <windows.h>

namespace CatmullClarkSubdivision
{
    static std::string getFileFullPath(const char* name)
    {
        char moduleName[_MAX_PATH] = { 0 };
        if (!GetModuleFileNameA(nullptr, moduleName, _MAX_PATH))
            throw std::exception(std::error_code(static_cast<int>(GetLastError()), std::system_category()).message().c_str());

        char drive[_MAX_DRIVE] = { 0 };
        char path[_MAX_PATH] = { 0 };

        if (_splitpath_s(moduleName, drive, _MAX_DRIVE, path, _MAX_PATH, nullptr, 0, nullptr, 0))
            throw std::exception("Can't split path");

        char filename[_MAX_PATH];
        if (_makepath_s(filename, _MAX_PATH, drive, path, name, nullptr))
            throw std::exception("Can't find filename");

        return filename;
    }
}

#endif // CATMULL_CLARK_SUBDIVITION_UTILS_H_
