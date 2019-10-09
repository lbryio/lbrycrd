
#include <claimtrie/log.h>

void CLogPrint::setLogger(ClogBase* log)
{
    logger = log;
}

CLogPrint& CLogPrint::global()
{
    static CLogPrint logger;
    return logger;
}

CLogPrint& CLogPrint::operator<<(const Clog& cl)
{
    if (logger && cl == Clog::endl) {
        ss << '\n';
        logger->LogPrintStr(ss.str());
        ss.str({});
    }
    return *this;
}
