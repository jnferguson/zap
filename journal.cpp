#include "journal.hpp"

bool 
journal_base_t::alloc(const std::size_t siz)
{
	uint8_t* ptr(nullptr);

	/*const std::size_t 	pgsz(page_size());
	const std::size_t	total(siz + (2 * siz));
	uint8_t*			ptr(nullptr);

	if (siz > std::numeric_limits< std::size_t >::max() - (2 * siz))
		throw std::invalid_argument("journal_base_t::alloc(): additive overflow when allocating requested size plus guard pages");

	ptr = static_cast< uint8_t* >(::memalign(pgsz, total));

	if (nullptr == ptr) 
		return false;

	if (0 != ::mprotect(ptr, pgsz, PROT_NONE)) 
		return false;

	if (0 != ::mprotect(ptr + total - pgsz, pgsz, PROT_NONE)) 
		return false;

	ptr += pgsz;
	std::memset(ptr, 0, siz - pgsz);

	if (nullptr != m_ptr) {
		if (m_size <= siz)
			std::memcpy(ptr, m_ptr, m_size);
		else
			std::memcpy(ptr, m_ptr, siz);

		dealloc();
	}*/

	ptr = new uint8_t[siz];
	std::memset(ptr, 0, siz);

	if (nullptr != m_ptr) {
		if (m_size <= siz)
			std::memcpy(ptr, m_ptr, m_size);
		else
			std::memcpy(ptr, m_ptr, siz);
	
		dealloc();
	}

	m_ptr 	= ptr;
	m_size	= siz;
	return true;
}

void
journal_base_t::dealloc(void)
{
	/*const std::size_t	pgsz(page_size());
	uint8_t* 			ptr(nullptr);

	if (nullptr == m_ptr)
		return;

	ptr = m_ptr;
	ptr -= pgsz;
	
	::free(ptr);*/

	if (nullptr == m_ptr)
		return;

	delete[] m_ptr;
	m_ptr 	= nullptr;
	m_size	= 0;
	return;
}

const uint64_t
journal_base_t::hash_data(const void* data, const std::size_t size) const 
{
	if (nullptr == data || 0 == size)
		throw journal_parameter_error_t("journal_base_t::hash_data(): invalid parameter (nullptr/0 length)");

	if (JOURNAL_HEADER_KEYED_HASH(m_incompatible_flags)) {
		siphash_t 	sh;
		sbarray_t	file_id;

		std::memcpy(&file_id[0], &m_file_id[0], file_id.size());
		return sh.hash(data, size, file_id);
	}

	return jenkins_hash64_t::hash(data, size);
}

uint64_t
journal_base_t::minimum_header_size(const object_t* obj) const
{
    if (nullptr == obj)
        throw journal_parameter_error_t("journal_base_t::minimum_header_size(): invalid parameter (nullptr)");

    if (obj->object.type >= object_type_t::OBJECT_TYPE_MAX || m_object_size_tbl[obj->object.type] <= 0)
        return sizeof(object_header_t);

    return m_object_size_tbl[obj->object.type];
}

void
journal_base_t::check_object(const uint64_t& offset, const object_t* obj) const
{

	DEBUG("journal_base_t::check_object(): Checking object at offset: ", to_hex_string(offset));

	if (nullptr == obj)
		throw journal_parameter_error_t("journal_base_t::check_object(): invalid parameter (nullptr)");

	switch (obj->object.type) {
		case object_type_t::OBJECT_DATA:
			DEBUG("journal_base_t::check_object(): Checking DATA object type");

			if ((0 == get_uint64(obj->data.entry_offset)) ^ (0 == get_uint64(obj->data.n_entries)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid n_entries encountered (entry_offset ^ n_entries)");
			if (get_uint64(obj->object.size) <= offsetof(data_object_t, payload))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid object size encountered (<=offsetof(data_object_t,payload))");

			if (! VALID64(get_uint64(obj->data.next_hash_offset)) ||
				! VALID64(get_uint64(obj->data.next_field_offset)) ||
				! VALID64(get_uint64(obj->data.entry_offset)) ||
				! VALID64(get_uint64(obj->data.entry_array_offset)))
					throw journal_verification_error_t("journal_base_t::check_object(): invalid offset encountered (OBJECT_DATA)");
			
			DEBUG("journal_base_t::check_object(): DATA object is valid");
		break;

		case object_type_t::OBJECT_FIELD:
			DEBUG("journal_base_t::check_object(): Checking FIELD object type");

			if (get_uint64(obj->object.size) <= offsetof(field_object_t, payload))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid object size encountered (<=offset(field_object_t,payload))");

			if (! VALID64(get_uint64(obj->field.next_hash_offset)) ||
				! VALID64(get_uint64(obj->field.head_data_offset)))
					throw journal_verification_error_t("journal_base_t::check_object(): invalid offset encountered (OBJECT_FIELD)");

			DEBUG("journal_base_t::check_object(): FIELD object is valid");
		break;

		case object_type_t::OBJECT_ENTRY:
		{
			const uint64_t sz(obj->object.size);


			DEBUG("journal_base_t::check_object():  Checking ENTRY object type");

			if (sz < offsetof(entry_object_t, items) ||
				0 != ((sz - offsetof(entry_object_t, items)) % sizeof(entry_item_t)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid entry size encountered");

			if (0 >= (sz - offsetof(entry_object_t, items)) / sizeof(entry_item_t))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid number of entries encountered");

			if (0 >= get_uint64(obj->entry.seqnum))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid entry sequence number encountered");

			if (! VALID_REALTIME(get_uint64(obj->entry.realtime)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid entry realtime stamp encountered");

			if (! VALID_MONOTONIC(get_uint64(obj->entry.monotonic)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid entry monotonic stamp encountered");

			DEBUG("journal_base_t::check_object(): ENTRY object is valid");
		}
		break;


		case object_type_t::OBJECT_DATA_HASH_TABLE:
		case object_type_t::OBJECT_FIELD_HASH_TABLE:
		{
			const uint64_t sz(get_uint64(obj->object.size));

			if (OBJECT_DATA_HASH_TABLE == obj->object.type)
				DEBUG("journal_base_t::check_object(): Checking DATA_HASH_TABLE object type");
			else
				DEBUG("journal_base_t::check_object(): Checking FIELD_HASH_TABLE object type");

			if (sz <= offsetof(hash_table_object_t, items))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid hash table size encountered (sz <= offsetof)");

			if (0 != ((sz - offsetof(hash_table_object_t, items)) % sizeof(hash_item_t)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid hash table size encountered (sz-offsetof()%sizeof(hash_item_t))");

			if (0 >= ((sz - offsetof(hash_table_object_t, items)) / sizeof(hash_item_t)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid hash table size encountered (sz-offsetof()/sizeof(hash_item_t))");

			if (OBJECT_DATA_HASH_TABLE == obj->object.type)
				DEBUG("journal_base_t::check_object(): DATA_HASH_TABLE object is valid");
			else
				DEBUG("journal_base_t::check_object(): FIELD_HASH_TABLE object is valid");

		}

		break;

		case object_type_t::OBJECT_ENTRY_ARRAY:
		{
			const uint64_t sz(get_uint64(obj->object.size));

			DEBUG("journal_base_t::check_object(): Checking ENTRY_ARRAY object type");

			if (sz < offsetof(entry_array_object_t, items)  ||
				0 != ((sz - offsetof(entry_array_object_t, items)) % sizeof(uint64_t)) ||
				0 >= ((sz - offsetof(entry_array_object_t, items)) / sizeof(uint64_t)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid object entry array size encountered");

			if (! VALID64(get_uint64(obj->entry_array.next_entry_array_offset)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid next_entry_array_offset encountered");

			DEBUG("journal_base_t::check_object(): ENTRY_ARRAY object is valid");
		}
	
		break;

		case object_type_t::OBJECT_TAG:
			DEBUG("journal_base_t::check_object(): Checking TAG object type");

			if (get_uint64(obj->object.size) != sizeof(tag_object_t))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid tag object size encountered");

			if (! VALID_EPOCH(get_uint64(obj->tag.epoch)))
				throw journal_verification_error_t("journal_base_t::check_object(): invalid tag object epoch timestamp encountered");

			DEBUG("journal_base_t::check_object():  TAG object is valid");
		break;

		default:
			throw journal_verification_error_t("journal_base_t::check_object(): invalid object type encountered (default)");

		break;
	}

	return;
}

void
journal_base_t::move_to_object(const object_type_t& type, const uint64_t offset, object_t** ret) const
{
	void*       tmp(nullptr);
	object_t*   obj(nullptr);
	std::size_t siz(0);

	DEBUG("journal_base_t::move_to_object(): Moving to object of type: ", obj_hdr_t::type_string(type), " at offset: ", to_hex_string(offset));

	if (nullptr == ret)
		throw journal_parameter_error_t("journal_base_t::move_to_object(): Invalid parameter (nullptr)");
	if (! VALID64(offset))
		throw journal_invalid_logic_error_t("journal_base_t::move_to_object(): Attempt to move to object at non-64bit boundary");
	if (offset < m_header_size)
		throw journal_invalid_logic_error_t("journal_base_t::move_to_object(): Attempt to move to object inside of file header");

	move_to(type, offset, sizeof(object_header_t), &tmp);

	obj = static_cast< object_t* >(tmp);
	siz = get_uint64(obj->object.size);

	if (0 == siz)
		throw journal_invalid_logic_error_t("journal_base_t::move_to_object(): uninitialized object encountered");
	if (siz < sizeof(object_header_t))
		throw journal_invalid_logic_error_t("journal_base_t::move_to_object(): invalid object size encountered (<sizeof(object_header_t))");
	if (obj->object.type <= object_type_t::OBJECT_UNUSED)
		throw journal_invalid_logic_error_t("journal_base_t::move_to_object(): invalid object type encountered");
	if (siz < minimum_header_size(obj))
		throw journal_invalid_logic_error_t("journal_base_t::move_to_object(): truncated object encountered");
	if (type > object_type_t::OBJECT_UNUSED && obj->object.type != type)
		throw journal_invalid_logic_error_t("journal_base_t::move_to_object(): unexpected object type encountered");
	if (siz > m_size)
		throw journal_invalid_logic_error_t("journal_base_t::move_to_object(): invalid object size encountered (>m_size)");

	check_object(offset, obj);
	*ret = obj;
	return;
}

void
journal_base_t::move_to(const object_type_t& type, const uint64_t offset, const uint64_t size, void** ret) const
{
	DEBUG("journal_base_t::move_to(): Moving to object of type: ", obj_hdr_t::type_string(type), " at offset: ", to_hex_string(offset));

	if (size <= 0)
		throw journal_invalid_logic_error_t("journal_base_t::move_to(): invalid size encountered (<=0)");
	if (size > std::numeric_limits< uint64_t >::max() - offset)
		throw journal_overflow_error_t("journal_base_t::move_to(): invalid size encountered (>max())");
	if (offset+size > m_size)
		throw journal_invalid_logic_error_t("journal_base_t::move_to(): invalid size/offset encountered (>size)");

	*ret = m_ptr+offset;
	return;
}

const uint64_t
journal_base_t::file_entry_n_items(const object_t* obj) const
{
	uint64_t sz(0);

	if (nullptr == obj)
		throw journal_parameter_error_t("journal_base_t::file_entry_n_items(): invalid parameter encountered (nullptr)");

	if (object_type_t::OBJECT_ENTRY != obj->object.type)
		throw journal_invalid_logic_error_t("journal_base_t::file_entry_n_items(): non ENTRY object encountered in ENTRY specific API");

	sz = get_uint64(obj->object.size);

	if (sz < offsetof(object_t, entry.items))
		return 0;

	return (sz - offsetof(object_t, entry.items)) / sizeof(entry_item_t);
}

const uint64_t
journal_base_t::file_entry_n_items(const entry_object_t* obj) const
{
	uint64_t sz(0);

	if (nullptr == obj)
		throw journal_parameter_error_t("journal_base_t::file_entry_n_items(): invalid parameter encountered (nullptr)");

	sz = get_uint64(obj->object.size);

	if (sz < offsetof(object_t, entry.items))
		return 0;

	return (sz - offsetof(object_t, entry.items)) / sizeof(entry_item_t);
}

const uint64_t
journal_base_t::file_hash_table_n_items(const object_t* obj) const
{
	uint64_t sz(0);

	if (nullptr == obj)
		throw journal_parameter_error_t("journal_base_t::file_hash_table_n_items(): invalid parameter encountered (nullptr)");

	if (object_type_t::OBJECT_DATA_HASH_TABLE != obj->object.type &&
		object_type_t::OBJECT_FIELD_HASH_TABLE != obj->object.type)
		throw journal_invalid_logic_error_t("journal_base_t::file_hash_table_n_items(): non HASH TABLE object encountered in HASH TABLE specific API");

	sz = get_uint64(obj->object.size);

	if (sz < offsetof(object_t, hash_table.items))
		return 0;

	return (sz - offsetof(object_t, hash_table.items)) / sizeof(hash_item_t);
}

const uint64_t
journal_base_t::file_entry_array_n_items(const object_t* obj) const
{
	uint64_t sz(0);

	if (nullptr == obj)
		throw journal_parameter_error_t("journal_base_t::file_entry_array_n_items(): invalid parameter encountered (nullptr)");

	if (object_type_t::OBJECT_ENTRY_ARRAY != obj->object.type)
		throw journal_invalid_logic_error_t("journal_base_t::file_entry_array_n_items(): non ENTRY ARRAY object encountered in ENTRY ARRAY specific API");

	sz = get_uint64(obj->object.size);

	if (sz < offsetof(object_t, entry_array.items))
		return 0;

	return (sz - offsetof(object_t, entry_array.items)) / sizeof(uint64_t);
}


journal_base_t::journal_base_t(void)
	: m_ptr(nullptr), m_size(0), m_name(""), m_seal(false)
{
	reset();

	m_object_size_tbl[object_type_t::OBJECT_UNUSED]				= 0;
	m_object_size_tbl[object_type_t::OBJECT_DATA]				= sizeof(data_object_t);
	m_object_size_tbl[object_type_t::OBJECT_FIELD]				= sizeof(field_object_t);
	m_object_size_tbl[object_type_t::OBJECT_ENTRY]				= sizeof(entry_object_t);
	m_object_size_tbl[object_type_t::OBJECT_DATA_HASH_TABLE]	= sizeof(hash_table_object_t);
	m_object_size_tbl[object_type_t::OBJECT_FIELD_HASH_TABLE]	= sizeof(hash_table_object_t);
	m_object_size_tbl[object_type_t::OBJECT_ENTRY_ARRAY]		= sizeof(entry_array_object_t);
	m_object_size_tbl[object_type_t::OBJECT_TAG]				= sizeof(tag_object_t);

	return;	
}

journal_base_t::journal_base_t(const char* name)
	: m_ptr(nullptr), m_size(0), m_name(name), m_seal(false)
{
	if (nullptr == name)
		throw journal_parameter_error_t("journal_base_t::journal_base_t(): invalid filename (null)");

	reset();

	m_object_size_tbl[object_type_t::OBJECT_UNUSED]				= 0;
	m_object_size_tbl[object_type_t::OBJECT_DATA]				= sizeof(data_object_t);
	m_object_size_tbl[object_type_t::OBJECT_FIELD]				= sizeof(field_object_t);
	m_object_size_tbl[object_type_t::OBJECT_ENTRY]				= sizeof(entry_object_t);
	m_object_size_tbl[object_type_t::OBJECT_DATA_HASH_TABLE]	= sizeof(hash_table_object_t);
	m_object_size_tbl[object_type_t::OBJECT_FIELD_HASH_TABLE]	= sizeof(hash_table_object_t);
	m_object_size_tbl[object_type_t::OBJECT_ENTRY_ARRAY]		= sizeof(entry_array_object_t);
	m_object_size_tbl[object_type_t::OBJECT_TAG]				= sizeof(tag_object_t);

	return;
}

journal_base_t::journal_base_t(const journal_base_t& other)
	: m_ptr(nullptr), m_size(0), m_name(""), m_seal(false)
{
	*this = other;
	return;	
}

journal_base_t&
journal_base_t::operator=(const journal_base_t& other)
{
	if (this == &other)
		return *this;

	reset();

	if (0 == other.m_size) {
		m_ptr 	= nullptr;
		m_size 	= 0;
	} else {

		if (false == alloc(other.m_size))
			throw journal_allocation_error_t("journal_base_t::operator=(): error allocating memory");

		std::memcpy(m_ptr, other.m_ptr, m_size);
	}


	m_name 														= other.m_name;

	m_object_size_tbl[object_type_t::OBJECT_UNUSED] 			= other.m_object_size_tbl[object_type_t::OBJECT_UNUSED];
	m_object_size_tbl[object_type_t::OBJECT_DATA] 				= other.m_object_size_tbl[object_type_t::OBJECT_DATA];
	m_object_size_tbl[object_type_t::OBJECT_FIELD] 				= other.m_object_size_tbl[object_type_t::OBJECT_FIELD];
	m_object_size_tbl[object_type_t::OBJECT_ENTRY] 				= other.m_object_size_tbl[object_type_t::OBJECT_ENTRY];
	m_object_size_tbl[object_type_t::OBJECT_DATA_HASH_TABLE] 	= other.m_object_size_tbl[object_type_t::OBJECT_DATA_HASH_TABLE];
	m_object_size_tbl[object_type_t::OBJECT_FIELD_HASH_TABLE] 	= other.m_object_size_tbl[object_type_t::OBJECT_FIELD_HASH_TABLE];
	m_object_size_tbl[object_type_t::OBJECT_ENTRY_ARRAY] 		= other.m_object_size_tbl[object_type_t::OBJECT_ENTRY_ARRAY];
	m_object_size_tbl[object_type_t::OBJECT_TAG] 				= other.m_object_size_tbl[object_type_t::OBJECT_TAG];

	m_seal 														= other.m_seal;
	m_data_objects 												= other.m_data_objects;
	m_entry_objects 											= other.m_entry_objects;
	m_field_objects 											= other.m_field_objects;
	m_tag_objects 												= other.m_tag_objects;

	m_compatible_flags 											= other.m_compatible_flags;
	m_incompatible_flags 										= other.m_incompatible_flags;
	m_state 													= other.m_state;
	m_file_id 													= other.m_file_id;
	m_machine_id 												= other.m_machine_id;
	m_boot_id 													= other.m_boot_id;
	m_seqnum_id 												= other.m_seqnum_id;
	m_header_size 												= other.m_header_size;
	m_arena_size 												= other.m_arena_size;
	m_data_hash_table_offset 									= other.m_data_hash_table_offset;
	m_data_hash_table_size 										= other.m_data_hash_table_size;
	m_field_hash_table_offset 									= other.m_field_hash_table_offset;
	m_field_hash_table_size 									= other.m_field_hash_table_size;
	m_tail_object_offset 										= other.m_tail_object_offset;
	m_n_objects 												= other.m_n_objects;
	m_n_entries 												= other.m_n_entries;
	m_tail_entry_seqnum 										= other.m_tail_entry_seqnum;
	m_head_entry_seqnum 										= other.m_head_entry_seqnum;
	m_entry_array_offset 										= other.m_entry_array_offset;
	m_head_entry_realtime 										= other.m_head_entry_realtime;
	m_tail_entry_realtime 										= other.m_tail_entry_realtime;
	m_tail_entry_monotonic 										= other.m_tail_entry_monotonic;
	m_n_data 													= other.m_n_data;
	m_n_fields 													= other.m_n_fields;
	m_n_tags 													= other.m_n_tags;
	m_n_entry_arrays 											= other.m_n_entry_arrays;
	m_data_hash_chain_depth 									= other.m_data_hash_chain_depth;
	m_field_hash_chain_depth 									= other.m_field_hash_chain_depth;


	return *this;
}

journal_base_t::~journal_base_t(void)
{
	reset();
	return;
}

void
journal_base_t::reset(void)
{
	m_data_objects.clear();
	m_entry_objects.clear();
	m_field_objects.clear();
	m_tag_objects.clear();

	if (nullptr != m_ptr) 
		dealloc();

	m_size						= 0;
	m_name						= "";
	m_seal						= false;
	m_compatible_flags			= 0;
	m_incompatible_flags		= 0;
	m_state						= 0;
	m_file_id					= {0,0};
	m_machine_id				= {0,0};
	m_boot_id					= {0,0};
	m_seqnum_id					= {0,0};
	m_header_size				= 0;
	m_arena_size				= 0;
	m_data_hash_table_offset	= 0;
	m_data_hash_table_size		= 0;
	m_field_hash_table_offset	= 0;
	m_field_hash_table_size		= 0;
	m_tail_object_offset		= 0;
	m_n_objects					= 0;
	m_n_entries					= 0;
	m_tail_entry_seqnum			= 0;
	m_head_entry_seqnum			= 0;
	m_entry_array_offset		= 0;
	m_head_entry_realtime		= 0;
	m_tail_entry_realtime		= 0;
	m_tail_entry_monotonic		= 0;
	m_n_data					= 0;
	m_n_fields					= 0;
	m_n_tags					= 0;
	m_n_entry_arrays			= 0;
	m_data_hash_chain_depth		= 0;
	m_field_hash_chain_depth	= 0;

	return;
}

std::string 
journal_base_t::name(void) const
{
	return m_name;
}

void 
journal_base_t::name(const char* name)
{
	if (nullptr == name)
		throw journal_parameter_error_t("journal_base_t::name(): invalid filename (nullptr)");

	m_name = name;
	return;
}

bool 
journal_base_t::seal(void) const
{
	return m_seal;
}

void 
journal_base_t::seal(const bool seal)
{
	m_seal = seal;
	return;
}

std::string
journal_base_t::compatible_flags_string(void)
{
	std::string ret("");

	if (IS_HEADER_COMPATIBLE_SEALED(m_compatible_flags))
		ret += "Sealed";
	else
		ret += "None";

	return ret;
}

std::string
journal_base_t::incompatible_flags_string(void)
{
	std::string ret("");

	if (IS_HEADER_INCOMPATIBLE_COMPRESSED_XZ(m_incompatible_flags))
		ret += "XZ Compressed ";
	if (IS_HEADER_INCOMPATIBLE_COMPRESSED_LZ4(m_incompatible_flags))
		ret += "LZ4 Compressed ";
	if (IS_HEADER_INCOMPATIBLE_KEYED_HASH(m_incompatible_flags))
		ret += "Keyed Hash ";
	if (IS_HEADER_INCOMPATIBLE_COMPRESSED_ZSTD(m_incompatible_flags))
		ret += "ZSTD Compressed ";
	if (! IS_HEADER_INCOMPATIBLE_ANY(m_incompatible_flags))
		ret += "None";

	return ret;
}

std::string
journal_base_t::state_string(void)
{
	std::string ret("");

	if (STATE_OFFLINE == m_state)
		ret += "Offline";
	else if (STATE_ONLINE == m_state)
		ret += "Online";
	else if (STATE_ARCHIVED == m_state)
		ret += "Archived";
	else
		ret += "Invalid";

	return ret;
}

uint32_t
journal_base_t::compatible_flags(void) const
{
	return m_compatible_flags;
}

void
journal_base_t::compatible_flags(const uint32_t& flags)
{
	m_compatible_flags = flags;
	return;
}

uint32_t
journal_base_t::incompatible_flags(void) const
{
	return m_incompatible_flags;
}

void
journal_base_t::incompatible_flags(const uint32_t& flags)
{
	m_incompatible_flags = flags;
	return;
}

uint8_t
journal_base_t::state(void) const
{
	return m_state;
}

void
journal_base_t::state(const uint8_t& s)
{
	m_state = s;
	return;
}

uint128_vec_t
journal_base_t::file_id(void) const
{
	return m_file_id;
}

void
journal_base_t::file_id(const uint128_vec_t& id)
{
	m_file_id[0] = id[0];
	m_file_id[1] = id[1];
	return;
}

uint128_vec_t
journal_base_t::machine_id(void) const
{
	return m_machine_id;
}

void
journal_base_t::machine_id(const uint128_vec_t& id)
{
	m_machine_id[0] = id[0];
	m_machine_id[1] = id[1];
	return;
}

uint128_vec_t
journal_base_t::boot_id(void) const
{
	return m_boot_id;
}

void
journal_base_t::boot_id(const uint128_vec_t& id)
{
	m_boot_id[0] = id[0];
	m_boot_id[1] = id[1];
	return;
}

uint128_vec_t
journal_base_t::seqnum_id(void) const
{
	return m_seqnum_id;
}

void
journal_base_t::seqnum_id(const uint128_vec_t& seqnum)
{
	m_seqnum_id[0] = seqnum[0];
	m_seqnum_id[1] = seqnum[1];
	return;
}

uint64_t
journal_base_t::header_size(void) const
{
	return m_header_size;
}

void
journal_base_t::header_size(const uint64_t& siz)
{
	m_header_size = siz;
	return;
}

uint64_t
journal_base_t::arena_size(void) const
{
	return m_arena_size;
}

void
journal_base_t::arena_size(const uint64_t& siz)
{
	m_arena_size = siz;
	return;
}


uint64_t
journal_base_t::data_hash_table_offset(void) const
{
	return m_data_hash_table_offset;
}

void
journal_base_t::data_hash_table_offset(const uint64_t& off)
{
	m_data_hash_table_offset = off;
	return;
}

uint64_t
journal_base_t::data_hash_table_size(void) const
{
	return m_data_hash_table_size;
}

void
journal_base_t::data_hash_table_size(const uint64_t& siz)
{
	m_data_hash_table_size = siz;
	return;
}

uint64_t
journal_base_t::field_hash_table_offset(void) const
{
	return m_field_hash_table_offset;
}

void
journal_base_t::field_hash_table_offset(const uint64_t& off)
{
	m_field_hash_table_offset = off;
	return;
}

uint64_t
journal_base_t::field_hash_table_size(void) const
{
	return m_field_hash_table_size;
}

void
journal_base_t::field_hash_table_size(const uint64_t& siz)
{
	m_field_hash_table_size = siz;
	return;
}

uint64_t
journal_base_t::tail_object_offset(void) const
{
	return m_tail_object_offset;
}

void
journal_base_t::tail_object_offset(const uint64_t& too)
{
	m_tail_object_offset = too;
	return;
}

uint64_t
journal_base_t::n_objects(void) const
{
	return m_n_objects;
}

void
journal_base_t::n_objects(uint64_t& num)
{
	m_n_objects = num;
	return;
}

uint64_t
journal_base_t::n_entries(void) const
{
	return m_n_entries;
}

void
journal_base_t::n_entries(const uint64_t& num)
{
	m_n_entries = num;
	return;
}

uint64_t
journal_base_t::tail_entry_seqnum(void) const
{
	return m_tail_entry_seqnum;
}

void
journal_base_t::tail_entry_seqnum(const uint64_t& tes)
{
	m_tail_entry_seqnum = tes;
	return;
}

uint64_t
journal_base_t::head_entry_seqnum(void) const
{
	return m_head_entry_seqnum;
}

void
journal_base_t::head_entry_seqnum(const uint64_t& hes)
{
	m_head_entry_seqnum = hes;
	return;
}

uint64_t
journal_base_t::entry_array_offset(void) const
{
	return m_entry_array_offset;
}

void
journal_base_t::entry_array_offset(const uint64_t& eao)
{
	m_entry_array_offset = eao;
	return;
}

uint64_t
journal_base_t::head_entry_realtime(void) const
{
	return m_head_entry_realtime;
}

void
journal_base_t::head_entry_realtime(const uint64_t& her)
{
	m_head_entry_realtime = her;
	return;
}

uint64_t
journal_base_t::tail_entry_realtime(void) const
{
	return m_tail_entry_realtime;
}

void
journal_base_t::tail_entry_realtime(const uint64_t& ter)
{
	m_tail_entry_realtime = ter;
	return;
}

uint64_t
journal_base_t::tail_entry_monotonic(void) const
{
	return m_tail_entry_monotonic;
}

void
journal_base_t::tail_entry_monotonic(const uint64_t& tem)
{
	m_tail_entry_monotonic = tem;
	return;
}

uint64_t
journal_base_t::n_data(void) const
{
	return m_n_data;
}

void
journal_base_t::n_data(const uint64_t& num)
{
	m_n_data = num;
	return;
}

uint64_t
journal_base_t::n_fields(void) const
{
	return m_n_fields;
}

void
journal_base_t::n_fields(const uint64_t& num)
{
	m_n_fields = num;
	return;
}

uint64_t
journal_base_t::n_tags(void) const
{
	return m_n_tags;
}

void
journal_base_t::n_tags(const uint64_t& num)
{
	m_n_tags = num;
	return;
}

uint64_t
journal_base_t::n_entry_arrays(void) const
{
	return m_n_entry_arrays;
}

void
journal_base_t::n_entry_arrays(const uint64_t& num)
{
	m_n_entry_arrays = num;
	return;
}

uint64_t
journal_base_t::data_hash_chain_depth(void) const
{
	return m_data_hash_chain_depth;
}

void
journal_base_t::data_hash_chain_depth(const uint64_t& depth)
{
	m_data_hash_chain_depth = depth;
	return;
}

uint64_t
journal_base_t::field_hash_chain_depth(void) const
{
	return m_field_hash_chain_depth;
}

void
journal_base_t::field_hash_chain_depth(const uint64_t& depth)
{
	m_field_hash_chain_depth = depth;
	return;
}

std::vector< data_obj_t >&
journal_base_t::data_objects(void)
{
	return m_data_objects;
}

const std::size_t
journal_base_t::data_objects_size(void) const
{
	return m_data_objects.size();
}

std::vector< entry_obj_t >&
journal_base_t::entry_objects(void)
{
	return m_entry_objects;
}

const std::size_t
journal_base_t::entry_objects_size(void) const
{
	return m_entry_objects.size();
}

std::vector< field_obj_t >&
journal_base_t::field_objects(void)
{
	return m_field_objects;
}

const std::size_t
journal_base_t::field_objects_size(void)
{
	return m_field_objects.size();
}

std::vector< tag_obj_t >&
journal_base_t::tag_objects(void)
{
	return m_tag_objects;
}

const std::size_t
journal_base_t::tag_objects_size(void) const
{
	return m_tag_objects.size();
}

