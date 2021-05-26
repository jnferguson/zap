#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <unistd.h>

inline std::size_t
page_size(void)
{
	return sysconf(_SC_PAGESIZE);
}

inline std::size_t 
ALIGN_TO(size_t l, size_t ali) 
{
	return ((l + ali - 1) & ~(ali - 1));
}

#define PAGE_ALIGN_DOWN(l) ((l) & ~(page_size() - 1))
#define PAGE_ALIGN(l) ALIGN_TO((l), page_size())
#define FILE_SIZE_INCREASE (8 * 1024 * 1024ULL)
#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y)) 

typedef struct {
    std::string 				input_file;
    std::string 				output_file;
    std::vector< std::string > 	fields;
	std::vector< std::string >  field_values;
    bool 						print_all;
    bool 						print_matches;
    bool 						confirm_matches;
    bool 						yes;
    bool 						debug;
} params_t;


