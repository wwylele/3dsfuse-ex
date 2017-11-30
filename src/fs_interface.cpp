#include <cstdio>
#include "fs_interface.h"

FsPath::FsPath(const char* str) {
    while (true) {
        while (*str == '/')
            str++;

        if (*str == '\0')
            break;

        FsName new_step{};
        unsigned new_step_i = 0;
        while (*str != '/' && *str != '\0') {
            if (new_step_i < 16)
                new_step[new_step_i++] = *(str++);
            else
                str++;
        }

        if (new_step == FsName{{'.'}})
            continue;

        if (new_step == FsName{{'.', '.'}}) {
            if (steps.empty())
                return;
            steps.pop_back();
        } else {
            steps.push_back(new_step);
        }
    }

    is_valid = true;
}
FsFileInterface::~FsFileInterface() {}

FsInterface::~FsInterface() {}
