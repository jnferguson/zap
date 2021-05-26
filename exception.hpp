#pragma once
#include <stdexcept>

class journal_parse_error_t : public std::runtime_error
{
	public:
		journal_parse_error_t(const char* msg) : std::runtime_error(msg) {}
};

class journal_verification_error_t : public std::runtime_error
{
	public:
		journal_verification_error_t(const char* msg) : std::runtime_error(msg) {}
};

class journal_parameter_error_t : public std::invalid_argument
{
	public:
		journal_parameter_error_t(const char* msg) : std::invalid_argument(msg) {}
};

class journal_invalid_logic_error_t : public std::logic_error
{
	public:
		journal_invalid_logic_error_t(const char* msg) : std::logic_error(msg) {}
};

class journal_overflow_error_t : public std::overflow_error
{
	public:
		journal_overflow_error_t(const char* msg) : std::overflow_error(msg) {}
};

class journal_allocation_error_t : public std::runtime_error
{
	public:
		journal_allocation_error_t(const char* msg) : std::runtime_error(msg) {}
};

class dynamic_cast_failure_error_t : public std::runtime_error
{
	public:
		dynamic_cast_failure_error_t(const char* msg) : std::runtime_error(msg) {}
};
