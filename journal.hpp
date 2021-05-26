#pragma once
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <cstring>
#include <vector>
#include <limits>

#include "global.hpp"
#include "file.hpp"
#include "exception.hpp"
#include "object.hpp"
#include "journal-def.hpp"
#include "endian.hpp"
#include "intstring.hpp"
#include "log.hpp"
#include "siphash.hpp"
#include "lookup3.hpp"

class journal_base_t
{
	private:
	protected:
        uint8_t*					m_ptr;
		std::size_t					m_size;
		std::string                 m_name;
        object_size_tbl_t           m_object_size_tbl;
        bool                        m_seal;
        std::vector< data_obj_t >   m_data_objects;
        std::vector< entry_obj_t >  m_entry_objects;
        std::vector< field_obj_t >  m_field_objects;
        std::vector< tag_obj_t >    m_tag_objects;


		uint32_t					m_compatible_flags;
		uint32_t					m_incompatible_flags;
		uint8_t						m_state;
		uint128_vec_t				m_file_id;
		uint128_vec_t				m_machine_id;
		uint128_vec_t				m_boot_id;
		uint128_vec_t				m_seqnum_id;
		uint64_t					m_header_size;
		uint64_t					m_arena_size;
		uint64_t					m_data_hash_table_offset;
		uint64_t					m_data_hash_table_size;
		uint64_t					m_field_hash_table_offset;
		uint64_t					m_field_hash_table_size;
		uint64_t					m_tail_object_offset;
		uint64_t					m_n_objects;
		uint64_t					m_n_entries;
		uint64_t					m_tail_entry_seqnum;
		uint64_t					m_head_entry_seqnum;
		uint64_t					m_entry_array_offset;
		uint64_t					m_head_entry_realtime;
		uint64_t					m_tail_entry_realtime;
		uint64_t					m_tail_entry_monotonic;
		uint64_t					m_n_data;
		uint64_t					m_n_fields;
		uint64_t					m_n_tags;
		uint64_t					m_n_entry_arrays;
		uint64_t					m_data_hash_chain_depth;
		uint64_t					m_field_hash_chain_depth;

		virtual bool alloc(const std::size_t);
		virtual void dealloc(void);

		virtual const uint64_t hash_data(const void*, const std::size_t) const;
		virtual uint64_t minimum_header_size(const object_t*) const;
		virtual void check_object(const uint64_t& offset, const object_t* obj) const;
		virtual void move_to(const object_type_t&, const uint64_t, const uint64_t, void**) const;
		virtual void move_to_object(const object_type_t&, const uint64_t, object_t**) const;

		virtual const uint64_t file_entry_n_items(const object_t*) const;
		virtual const uint64_t file_entry_n_items(const entry_object_t*) const;
		virtual const uint64_t file_hash_table_n_items(const object_t*) const;
		virtual const uint64_t file_entry_array_n_items(const object_t*) const;

	public:
		journal_base_t(void);
		journal_base_t(const char*);
		journal_base_t(const journal_base_t&);

		journal_base_t& operator=(const journal_base_t&);
		
		virtual ~journal_base_t(void);
		
		virtual void reset(void);

		virtual std::string name(void) const;
		virtual void name(const char*);

		virtual bool seal(void) const;
		virtual void seal(const bool);

		virtual std::string compatible_flags_string(void);
		virtual std::string incompatible_flags_string(void);

		virtual std::string state_string(void);

		virtual uint32_t compatible_flags(void) const;
		virtual void compatible_flags(const uint32_t&);

		virtual uint32_t incompatible_flags(void) const;
		virtual void incompatible_flags(const uint32_t&);

		virtual uint8_t state(void) const;
		virtual void state(const uint8_t&);

		virtual uint128_vec_t file_id(void) const;
		virtual void file_id(const uint128_vec_t&);

		virtual uint128_vec_t machine_id(void) const;
		virtual void machine_id(const uint128_vec_t&);

		virtual uint128_vec_t boot_id(void) const;
		virtual void boot_id(const uint128_vec_t&);

		virtual uint128_vec_t seqnum_id(void) const;
		virtual void seqnum_id(const uint128_vec_t&);

		virtual uint64_t header_size(void) const;
		virtual void header_size(const uint64_t&);

		virtual uint64_t arena_size(void) const;
		virtual void arena_size(const uint64_t&);

		virtual uint64_t data_hash_table_offset(void) const;
		virtual void data_hash_table_offset(const uint64_t&);

		virtual uint64_t data_hash_table_size(void) const;
		virtual void data_hash_table_size(const uint64_t&);

		virtual uint64_t field_hash_table_offset(void) const;
		virtual void field_hash_table_offset(const uint64_t&);

		virtual uint64_t field_hash_table_size(void) const;
		virtual void field_hash_table_size(const uint64_t&);

		virtual uint64_t tail_object_offset(void) const;
		virtual void tail_object_offset(const uint64_t&);

		virtual uint64_t n_objects(void) const;
		virtual void n_objects(uint64_t&);

		virtual uint64_t n_entries(void) const;
		virtual void n_entries(const uint64_t&);

		virtual uint64_t tail_entry_seqnum(void) const;
		virtual void tail_entry_seqnum(const uint64_t&);

		virtual uint64_t head_entry_seqnum(void) const;
		virtual void head_entry_seqnum(const uint64_t&);

		virtual uint64_t entry_array_offset(void) const;
		virtual void entry_array_offset(const uint64_t&);

		virtual uint64_t head_entry_realtime(void) const;
		virtual void head_entry_realtime(const uint64_t&);

		virtual uint64_t tail_entry_realtime(void) const;
		virtual void tail_entry_realtime(const uint64_t&);

		virtual uint64_t tail_entry_monotonic(void) const;
		virtual void tail_entry_monotonic(const uint64_t&);

		virtual uint64_t n_data(void) const;
		virtual void n_data(const uint64_t&);

		virtual uint64_t n_fields(void) const;
		virtual void n_fields(const uint64_t&);

		virtual uint64_t n_tags(void) const;
		virtual void n_tags(const uint64_t&);

		virtual uint64_t n_entry_arrays(void) const;
		virtual void n_entry_arrays(const uint64_t&);
	
		virtual uint64_t data_hash_chain_depth(void) const;
		virtual void data_hash_chain_depth(const uint64_t&);

		virtual uint64_t field_hash_chain_depth(void) const;
		virtual void field_hash_chain_depth(const uint64_t&);

		virtual std::vector< data_obj_t >& data_objects(void);
		virtual const std::size_t data_objects_size(void) const;

		virtual std::vector< entry_obj_t >& entry_objects(void);
		virtual const std::size_t entry_objects_size(void) const;

		virtual std::vector< field_obj_t >& field_objects(void);
		virtual const std::size_t field_objects_size(void);

		virtual std::vector< tag_obj_t >& tag_objects(void);
		virtual const std::size_t tag_objects_size(void) const;
};

