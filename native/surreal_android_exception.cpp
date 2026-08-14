#include "Utils/Exception.h"

#include <mutex>
#include <stdexcept>

std::mutex Exception::mutex;

[[noreturn]] void Exception::Throw(const std::string& text) {
    throw std::runtime_error(text);
}
