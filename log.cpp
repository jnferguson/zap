#include "log.hpp"

logger_t::~logger_t(void)
{
    return;
}

inline std::string
logger_t::priority_to_string(const logging_priority_t& priority)
{
    std::string ret("");

    switch ( priority ) {
        case LOG_DEBUG:
            ret = "DD";
            break;

        case LOG_INFO:
        case LOG_INFO_PROMPT:
            ret = "++";
            break;

        case LOG_ERROR:
        case LOG_ERROR_NOLINE:
            ret = "XX";
            break;

        case LOG_CRITICAL:
            ret = "!!";
            break;

        default:
            throw std::runtime_error("logger_t::priority_to_string(): invalid priority type parameter");
            break;
    }

    return ret;
}

logger_t&
logger_t::instance(void)
{
    static logger_t inst;
    return inst;
}

void
logger_t::log_recursive(logging_priority_t priority, const char* file, int line, std::ostringstream& msg)
{
    std::string entry("[" + priority_to_string(priority) + "]: ");


    if (LOG_DEBUG == priority) {
        if (false == g_params.debug)
            return;
    }

    if (LOG_DEBUG == priority || LOG_INFO == priority || LOG_ERROR_NOLINE == priority)
        entry += msg.str();
    else
        entry += std::string(file) + ':' + std::to_string(line) + " " + msg.str();

    if (LOG_INFO_PROMPT == priority) {
        std::cout << entry << std::flush;
        return;
    }

    if (LOG_ERROR < priority)
        std::cout << entry << std::endl;
    else
        std::cerr << entry << std::endl;

    return;
}
