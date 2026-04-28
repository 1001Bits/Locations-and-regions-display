#pragma once

#include "F4SE/F4SE.h"
#include "RE/Fallout.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

namespace logger = F4SE::log;

using namespace std::literals;

#define DLLEXPORT __declspec(dllexport)
