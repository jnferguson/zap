#include "file.hpp"

output_file_t::output_file_t(void)
	: m_name(""), m_fd(-1)
{
	return;
}

output_file_t::output_file_t(const char* name)
	: m_name(name), m_fd(-1)
{
	if (nullptr == name)
		throw std::invalid_argument("output_file_t::output_file_t(): invalid filename (nullptr)");

	return;
}

output_file_t::~output_file_t(void)
{
	close();

	m_name 	= "";
	m_fd 	= -1;
	
	return;
}

std::string 
output_file_t::filename(void)
{
	return m_name;
}

void 
output_file_t::filename(const char* nm)
{
	if (nullptr == nm)
		throw std::invalid_argument("output_file_t::filename(): invalid filename (nullptr)");

	m_name = nm;
	return;
}

void 
output_file_t::open(void)
{
	if (true == m_name.empty())
		throw std::invalid_argument("output_file_t::open(): invalid filename (blank)");

	m_fd = ::open(m_name.c_str(), O_CREAT|O_EXCL|O_WRONLY, S_IRWXU|S_IRGRP|S_IROTH);

	if (0 > m_fd) 
		throw std::runtime_error("output_file_t::open(): error in open(2)");
	
	return;
}

void 
output_file_t::open(const char* nm)
{
	if (nullptr == nm)
		throw std::invalid_argument("output_file_t::open(): invalid filename (nullptr)");

	m_name = nm;
	this->open();
	return;
}

void 
output_file_t::close(void)
{
	(void)::close(m_fd);
	m_fd = -1;

	return;
}

const std::size_t
output_file_t::size(void) const
{
   struct stat sb = {0};

	if (true == m_name.empty())
		throw std::runtime_error("output_file_t::size(): invalid filename ('')");

	if (0 != ::stat(m_name.c_str(), &sb))
		throw std::runtime_error("output_file_t::size(): error in stat(2)");

	return static_cast< std::size_t >(sb.st_size);
}

void 
output_file_t::write(const uint8_t* ptr, const std::size_t siz)
{
    std::size_t total = 0;
    signed int  last  = 0;

	if (nullptr == ptr)
		throw std::invalid_argument("output_file_t::write(): invalid parameter (nullptr)");
	if (0 == siz)
		return;
	if (true == m_name.empty() || 0 > m_fd)
		throw std::runtime_error("output_file_t::write(): invalid object state");

	do {
		last = ::write(m_fd, ptr+total, siz-total);

		if (0 > last)
			throw std::runtime_error("input_file_t::read(): error in read(2)");

		total += static_cast< std::size_t >(last);
	} while (total < siz);

	return;
}

input_file_t::input_file_t(void) 
	: m_name(""), m_fd(-1) 
{
	return;
}

input_file_t::input_file_t(const char* name)
	: m_name(name), m_fd(-1)
{

	if (nullptr == name)
		throw std::invalid_argument("input_file_t::input_file_t(): invalid filename (null)");

	return;
}
	
input_file_t::~input_file_t(void)
{
	close();

	m_name 	= "";
	m_fd 	= -1;
	return;
}

std::string
input_file_t::filename(void)
{
	return m_name;
}

void
input_file_t::filename(const char* name)
{
	if (nullptr == name)
		throw std::invalid_argument("input_file_t::filename(): invalid filename (null)");

	m_name = name;
	return;
}

void
input_file_t::open(void)
{
	struct stat sb 		= {0};

	if (true == m_name.empty())
		throw std::invalid_argument("input_file_t::open(): invalid filename (blank)");

	if (0 != ::stat(m_name.c_str(), &sb))
		throw std::runtime_error("input_file_t::open(): error in stat(2)"); 

	m_fd = ::open(m_name.c_str(), O_RDONLY);

	if (0 > m_fd) 
		throw std::runtime_error("input_file_t::open(): error in open(2)");

	return;	
}

void
input_file_t::open(const char* name)
{
	if (nullptr == name)
		throw std::invalid_argument("input_file_t::open(): invalid filename (null)");

	m_name = name;
	return this->open();
}

void 
input_file_t::close(void)
{
	(void)::close(m_fd);
	m_fd = -1;
	return;
}


std::size_t
input_file_t::size(void)
{
	struct stat sb = {0};

	if (true == m_name.empty())
		throw std::runtime_error("input_file_t::size(): invalid filename ('')");

	if (0 != ::stat(m_name.c_str(), &sb))
		throw std::runtime_error("input_file_t::size(): error in stat(2)");
	
	return static_cast< std::size_t >(sb.st_size);
}

void
input_file_t::read(uint8_t* ptr, std::size_t siz)
{
	std::size_t total = 0;
	signed int	last  = 0;

	if (true == m_name.empty() || 0 > m_fd)
		throw std::runtime_error("input_file_t::read(): invalid object state");
	if (this->size() > siz || nullptr == ptr)
		throw std::invalid_argument("input_file_t::read(): invalid parameter(s)");
			
	do {
		last = ::read(m_fd, ptr+total, siz-total);

		if (0 > last)
			throw std::runtime_error("input_file_t::read(): error in read(2)"); 

		total += static_cast< std::size_t >(last);
	} while (total < siz);						

	return;
}
