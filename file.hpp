#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "exception.hpp"

class input_file_t
{
	private:
	protected:
		std::string m_name;
		signed int 	m_fd;

	public:
		input_file_t(void);
		input_file_t(const char*);

		virtual ~input_file_t(void);

		virtual std::string filename(void);
		virtual void filename(const char* name);

		virtual void open(void);
		virtual void open(const char* name);

		virtual void close(void);

		virtual std::size_t size(void);

		virtual void read(uint8_t* ptr, std::size_t siz);
};

class output_file_t
{
	private:
	protected:
		std::string m_name;
		signed int	m_fd;

	public:
		output_file_t(void);
		output_file_t(const char*);

		virtual ~output_file_t(void);

		virtual std::string filename(void);
		virtual void filename(const char*);

		virtual void open(void);
		virtual void open(const char* name);

		virtual void close(void);

		virtual const std::size_t size(void) const;
		virtual void write(const uint8_t* ptr, const std::size_t siz);
};
