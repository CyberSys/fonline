#pragma once

#include "Common.h"

using LogFunc = std::function<void(const string&)>;

// Write formatted text
void WriteLogMessage(const string& message);
template<typename... Args>
inline void WriteLog(const string& message, Args... args)
{
    WriteLogMessage(fmt::format(message, std::forward<Args>(args)...));
}

// Control
void LogWithoutTimestamp();
void LogToFile(const string& fname);
void LogToFunc(const string& key, LogFunc func, bool enable);
void LogToBuffer(bool enable);
void LogGetBuffer(string& buf);
