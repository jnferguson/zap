#pragma once
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <cstring>
#include <limits>
#include <algorithm> 

#include "global.hpp"
#include "file.hpp"
#include "exception.hpp"
#include "object.hpp"
#include "journal-def.hpp"
#include "journal.hpp"
#include "endian.hpp"
#include "intstring.hpp"
#include "log.hpp"
#include "siphash.hpp"
#include "lookup3.hpp"

class input_journal_t : public journal_base_t
{
	private:
	protected:
		bool			m_parsed;

		virtual void verify_file(const header_contents_t*) const;	
		virtual void verify_offsets(void) const;
		virtual void verify_object(const uint64_t& offset, const object_t* obj) const;
		virtual void verify_entry_array(void) const;	
		virtual void verify_hash_array(void) const;

		virtual obj_hdr_t* to_cpp_object(const object_t*) const;

	public:

		input_journal_t(void);
		input_journal_t(const char* name);

		virtual ~input_journal_t(void);

		virtual void reset(void);
		virtual void parse(const char* name);

		virtual const bool has_field(const std::string&) const;
		virtual const bool has_field_value(const std::string&) const;
		
		virtual const uint64_t get_field_hash(const std::string&) const;
		virtual const uint64_t get_field_value_hash(const std::string&) const;
		
};

