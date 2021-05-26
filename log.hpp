#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iostream>
#include <ostream>
#include "global.hpp"

extern params_t g_params;

typedef enum
{
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_INFO_PROMPT,
	LOG_ERROR,
	LOG_ERROR_NOLINE,
	LOG_CRITICAL,
	LOG_INVALID_PRIORITY
} logging_priority_t;


#define DEBUG(...) logger_t::instance().log_wrapper(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define INFO(...)  logger_t::instance().log_wrapper(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define INFO_PROMPT(...) logger_t::instance().log_wrapper(LOG_INFO_PROMPT, __FILE__, __LINE__, __VA_ARGS__)
#define ERROR(...) logger_t::instance().log_wrapper(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define ERROR_NOLINE(...) logger_t::instance().log_wrapper(LOG_ERROR_NOLINE, __FILE__, __LINE__, __VA_ARGS__)
#define CRITICAL(...) logger_t::instance().log_wrapper(LOG_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)

class logger_t
{
	private:
		logger_t(void) {}

	protected:
		static inline std::string priority_to_string(const logging_priority_t& priority);

	public:
		logger_t(logger_t&)				= delete;
		void operator=(logger_t const&) = delete;
	
		~logger_t(void);

		static logger_t& instance(void);

		// variadic template from https://stackoverflow.com/questions/19415845/a-better-log-macro-using-template-metaprogramming
		// there are no specific licensing notes, so its presumably public domain
		template< typename... Args > void 
		log_wrapper(logging_priority_t priority, const char* file, int line, const Args&... args)
		{
			std::ostringstream msg;
			log_recursive(priority, file, line, msg, args...);
		}


		template<typename T, typename... Args> void 
		log_recursive(logging_priority_t priority, const char* file, int line, std::ostringstream& msg, T value, const Args&... args)
		{
			msg << value;
			log_recursive(priority, file, line, msg, args...);
		}

		void log_recursive(logging_priority_t priority, const char* file, int line, std::ostringstream& msg);
};

