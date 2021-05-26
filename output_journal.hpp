#pragma once
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <cstring>
#include <vector>
#include <list>
#include <limits>

#include "global.hpp"
#include "file.hpp"
#include "exception.hpp"
#include "object.hpp"
#include "journal.hpp"
#include "journal-def.hpp"
#include "endian.hpp"
#include "intstring.hpp"
#include "log.hpp"

class output_journal_t : public journal_base_t
{
	private:
	protected:
		hash_item_t*	m_data_hash_table;
		hash_item_t*	m_field_hash_table;

		virtual bool has_data_object(const data_obj_t&) const;
		virtual bool has_field_object(const field_obj_t&) const;
		virtual bool split_field_value(const std::string&, std::string&) const;
		virtual bool find_field_value(const std::string&, field_obj_t&) const;

		virtual bool allocate(const uint64_t, const uint64_t);
		
		virtual void append_object(const object_type_t&, const uint64_t, object_t**, uint64_t*);
		virtual void append_field(const field_object_t*, const void*, const std::size_t, object_t**, uint64_t*);
		virtual void append_data(const data_object_t*, object_t**, uint64_t*);
		virtual void append_entry_internal(const entry_object_t*, uint64_t*, object_t**, uint64_t*);
		virtual void append_entry(const entry_obj_t*, uint64_t*, object_t**, uint64_t*, bool p = false);
		
		virtual void link_data(object_t*, const uint64_t, const uint64_t);
		virtual void link_field(object_t*, const uint64_t, const uint64_t);
		virtual void link_entry_into_array(data_object_t*, uint64_t*, const uint64_t); 
		virtual void link_entry_into_array(uint64_t*, uint64_t*, const uint64_t);
		virtual void link_entry_item(object_t*, const entry_item_t*, const uint64_t);
		virtual void link_entry_into_array_plus_one(uint64_t*, uint64_t*, uint64_t*, const uint64_t);
		virtual void link_entry_into_array_plus_one(data_object_t*, const uint64_t);
		virtual void link_entry(object_t*, const uint64_t);
		
		virtual const uint64_t entry_seqnum(uint64_t*);
		
		virtual bool next_hash_offset(uint64_t*, uint64_t*, uint64_t*, uint64_t*);

		virtual void map_data_hash_table(const bool remap = false);
		virtual void map_field_hash_table(const bool remap = false);
		virtual bool setup_field_hash_table(void);
		virtual bool setup_data_hash_table(void);

		virtual bool find_data_object_with_hash(const data_object_t*, object_t**, uint64_t*);
		virtual bool find_field_object_with_hash(const field_object_t*, const void*, const std::size_t, object_t**, uint64_t*);
		virtual void write_payload(const void* data, const std::size_t len, const uint64_t offset);

		virtual void write_header(void);
		
	public:
		output_journal_t(void);
		output_journal_t(const char*);
		output_journal_t(const journal_base_t&);

		virtual ~output_journal_t(void);

		virtual void update(std::list< entry_obj_t >&);
		virtual bool write(void);
};
