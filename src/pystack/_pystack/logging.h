#pragma once

#include <sstream>
#include <string>

namespace pystack {

enum logLevel {
    NOTSET = 0,
    DEBUG = 10,
    INFO = 20,
    WARNING = 30,
    ERROR = 40,
    CRITICAL = 50,
};

void
logWithPython(const std::string& message, int level);

class LOG
{
  public:
    // Constructors
    LOG()
    : msgLevel(INFO){};

    explicit LOG(logLevel type)
    {
        msgLevel = type;
    };

    // Destructors
    ~LOG()
    {
        logWithPython(buffer.str(), msgLevel);
    };

    // Operators
    template<typename T>
    LOG& operator<<(const T& msg)
    {
        buffer << msg;
        return *this;
    };

  private:
    // Data members
    std::ostringstream buffer;
    logLevel msgLevel = DEBUG;
};

void
initializePythonLoggerInterface();

}  // namespace pystack
