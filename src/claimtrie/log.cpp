
#include <log.h>

void CLogPrint::setLogger(ClogBase* log)
{
    ss.str({});
    logger = log;
}

CLogPrint& CLogPrint::global()
{
    static CLogPrint logger;
    return logger;
}

CLogPrint& CLogPrint::operator<<(const Clog& cl)
{
    if (logger) {
        switch(cl) {
        case Clog::endl:
            ss << '\n';
            // fallthrough
        case Clog::flush:
            logger->LogPrintStr(ss.str());
            ss.str({});
            break;
        }
    }
    return *this;
}
