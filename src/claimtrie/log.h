
#ifndef CLAIMTRIE_LOG_H
#define CLAIMTRIE_LOG_H

#include <string>
#include <sstream>

struct ClogBase
{
    ClogBase() = default;
    virtual ~ClogBase() = default;
    virtual void LogPrintStr(const std::string&) = 0;
};

enum struct Clog
{
    endl = 0,
};

struct CLogPrint
{
    template <typename T>
    CLogPrint& operator<<(const T& a)
    {
        if (logger)
            ss << a;
        return *this;
    }

    CLogPrint& operator<<(const Clog& cl);

    void setLogger(ClogBase* log);
    static CLogPrint& global();

private:
    CLogPrint() = default;
    std::stringstream ss;
    ClogBase* logger = nullptr;
};

#endif // CLAIMTRIE_LOG_H
