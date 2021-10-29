/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "named_pipe.h"

#ifdef __linux__

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdexcept>

namespace {

bool isDescriptorValid(const FileDescriptor &desc) {
    return desc > 0;
}

int getPipeMode(NamedPipe::Mode mode) {
    if (mode == NamedPipe::Mode::ReadOnly)
        return O_RDONLY;
    if (mode == NamedPipe::Mode::WriteOnly)
        return O_WRONLY;
    throw std::runtime_error("Invalid pipe mode.");
}
} // namespace

NamedPipe::NamedPipe(const std::string &name, NamedPipe::Mode mode) {
    if (mode == NamedPipe::Mode::ReadOnly) {
        if (mkfifo(name.c_str(), 0666))
            throw std::runtime_error("Can't create pipe " + name + ": " + std::string(strerror(errno)));
    }
    _pipeName = name;
    _pipeDescriptor = ::open(name.c_str(), getPipeMode(mode));
    if (!isDescriptorValid(_pipeDescriptor))
        throw std::runtime_error("Can't open pipe " + name + ".");
}

NamedPipe::~NamedPipe() {
    try {
        close();
        remove(_pipeName.c_str());
    } catch (...) {
    }
}

int NamedPipe::read(void *buf, std::size_t count) {
    return ::read(_pipeDescriptor, buf, count);
}

int NamedPipe::write(const void *buf, std::size_t count) {
    return ::write(_pipeDescriptor, buf, count);
}

void NamedPipe::close() {
    if (::close(_pipeDescriptor))
        throw std::runtime_error("Failed to close a pipe.");
}

std::string NamedPipe::getName() const {
    return _pipeName;
}

#elif _WIN32
#include <cassert>

NamedPipe::NamedPipe(const std::string &, NamedPipe::Mode) {
    assert(!"NamedPipe is not implemented for Windows.");
}

NamedPipe::~NamedPipe() {
}

int NamedPipe::read(void *, std::size_t) {
    assert(!"NamedPipe::read is not implemented for Windows.");
    return -1;
}

int NamedPipe::write(const void *, std::size_t) {
    assert(!"NamedPipe::write is not implemented for Windows.");
    return -1;
}

void NamedPipe::close() {
    assert(!"NamedPipe::close is not implemented for Windows.");
}

std::string NamedPipe::getName() const {
    return _pipeName;
}
#endif
