#include "output_journal.hpp"

output_journal_t::output_journal_t(void)
	: journal_base_t(), m_data_hash_table(nullptr), m_field_hash_table(nullptr)
{
	return;
}

output_journal_t::output_journal_t(const char* name)
	: journal_base_t(name), m_data_hash_table(nullptr), m_field_hash_table(nullptr)
{
	return;
}

output_journal_t::output_journal_t(const journal_base_t& journal)
	: journal_base_t(journal), m_data_hash_table(nullptr), m_field_hash_table(nullptr)
{
	return;
}

output_journal_t::~output_journal_t(void)
{
	reset();
	return;
}

bool
output_journal_t::has_data_object(const data_obj_t& obj) const
{
	const std::size_t mdval(m_data_objects.size());

	for (std::size_t idx = 0; idx < mdval; idx++) {
		if (obj.hash() == m_data_objects[idx].hash()) {
				const std::string& left(obj.to_string()), right(m_data_objects[idx].to_string());
				const std::size_t  len(left.length());

				if (len == right.length() && 0 == ::strncasecmp(left.c_str(), right.c_str(), len))
					return true;
		}
	}

	return false;
}

bool
output_journal_t::has_field_object(const field_obj_t& obj) const
{
	const std::size_t mfval(m_field_objects.size());

	for (std::size_t idx = 0; idx < mfval; idx++) {
		if (obj.hash() == m_field_objects[idx].hash()) {
			const std::string left(obj.to_string()), right(m_field_objects[idx].to_string());
			const std::size_t len(left.length());

			if (len == right.length() && 0 == ::strncasecmp(left.c_str(), right.c_str(), len))
				return true;
		}
	}

	return false;
}

bool 
output_journal_t::split_field_value(const std::string& src, std::string& dst) const
{
	std::string sval(src);
	std::size_t pos(0);
	field_obj_t obj(0,0,0);

	/* 
	 * What should be a simple string splitting routine isn't more or less for the same
     * reason as the comment in find_field_value(). A Python script was found to be 
     * appending binary gibberish before the actual values. The methodology in 
     * find_field_value() almost fixed everything, except the ASCII character 
     * following the gibberish in one entry happened to be "=" which broke
     * this. So, now I have to redundantly check to make sure we have a 
     * matching entry (which we always should).
     */

	do {
		pos = sval.find("=");
		
		if (std::string::npos == pos) 
			return false;

		dst = src.substr(0,pos);
		if (true == find_field_value(dst, obj)) 
			return true;

		sval = sval.substr(1);
	} while (0 != sval.length());

	return false;
}

bool 
output_journal_t::find_field_value(const std::string& key, field_obj_t& dst) const
{
	std::string		  	kval(key);
	const std::size_t 	fvmax(m_field_objects.size());
	std::size_t 		len(key.length());

	if (0 == len)
		return false;

	do {
		for (std::size_t idx = 0; idx < fvmax; idx++) {
			const std::string field(m_field_objects[idx].to_string());

			if (len == field.length() && 0 == kval.compare(field)) {
				dst = m_field_objects[idx];
				return true;
			}
		}

		/* 
		 * This is requisite because for whatever reason some log entries I encountered
		 * had a more or less invalid field, namely non-printable characters preceeding
		 * the actual field value. journalctl seems to ignore it but I verified repeatedly
		 * that the value is definitely in the raw log file. So this is a hackish work
		 * around because the entry in question was basically:
		 * "<nonprintable gibberish>(MESSAGE=" with the field obviously being MESSAGE.
		 * 
		 * FTR, it was a python script generating the message.
		 */
		kval 	= kval.substr(1);
		len		= kval.length();

	} while (0 != kval.length());

	return false;
}

bool
output_journal_t::allocate(const uint64_t offset, const uint64_t size)
{
	const uint64_t 	old_header_size(m_header_size), old_arena_size(m_arena_size);
	uint64_t 		old_size(0), new_size(0);

	DEBUG("Allocating ", offset+size, " bytes...");

	if (size > PAGE_ALIGN_DOWN(std::numeric_limits< uint64_t >::max()) - offset)
		return false;

	if (old_arena_size > PAGE_ALIGN_DOWN(std::numeric_limits< uint64_t >::max()) - old_header_size)
		return false;

	DEBUG("older header size: ", old_header_size, " old arena size: ", old_arena_size);

	old_size = old_header_size + old_arena_size;
	new_size = std::max(PAGE_ALIGN(offset+size), old_header_size);

	DEBUG("new size: ", new_size, " old size: ", old_size);

	if (new_size <= old_size)
		return true;

	new_size = DIV_ROUND_UP(new_size, FILE_SIZE_INCREASE) * FILE_SIZE_INCREASE;

	DEBUG("Allocating ", new_size, " bytes after rounding");

	if (false == alloc(new_size))
		throw journal_allocation_error_t("output_journal_t::allocate(): allocation failure");

	if (offset >= new_size)
		throw journal_invalid_logic_error_t("output_journal_t::allocate(): offset >= new_size");

	m_arena_size 	= new_size - old_header_size;

	if (nullptr != m_data_hash_table)
		map_data_hash_table(true);
	if (nullptr != m_field_hash_table)
		map_field_hash_table(true);

	return true;
}

void 
output_journal_t::append_object(const object_type_t& type, const uint64_t size, object_t** ret, uint64_t* ret_offset)
{
	uint64_t 	p(m_tail_object_offset);
	object_t*	tail(nullptr);
	object_t*	o(nullptr);
	void*		t(nullptr);

	DEBUG("Appending object of type: ", obj_hdr_t::type_string(type), " and size: ", to_dec_string(size));

	if (nullptr == m_ptr)
		throw journal_parameter_error_t("output_journal_t::append_object(): invalid object state (m_ptr == nullptr)");
	if (OBJECT_UNUSED > type && OBJECT_TYPE_MAX < type)
		throw journal_invalid_logic_error_t("output_journal_t::append_object(): invalid type specified");
	if (sizeof(object_header_t) > size)
		throw journal_invalid_logic_error_t("output_journal_t::append_object(): invalid object size specified");

	if (0 == p)
		p = m_header_size;
	else {
		uint64_t sz(0);

		move_to_object(object_type_t::OBJECT_UNUSED, p, &tail);
		sz = get_uint64(tail->object.size);

		if (sz > std::numeric_limits< uint64_t >::max() - sizeof(uint64_t) + 1)
			throw journal_overflow_error_t("output_journal_t::append_object(): invalid tail object size encountered");

		sz = ALIGN64(sz);
		
		if (p > std::numeric_limits< uint64_t >::max() - sz)
			throw journal_overflow_error_t("output_journal_t::append_object(): invalid offset encountered");

		p += sz;
	}
	 

	if (false == allocate(p, size))
		throw journal_allocation_error_t("output_journal_t::append_object(): failure during allocation");

	move_to(object_type_t::OBJECT_UNUSED, p, size, &t);

	o 						= static_cast< object_t* >(t);
	o->object.type 			= type;
	o->object.size 			= get_uint64(size);
	m_tail_object_offset 	= p;
	m_n_objects 			+= 1;	

	DEBUG("Appended object of type: ", obj_hdr_t::type_string(type),
			" flags: ", to_dec_string(o->object.flags),
			" size: ", to_dec_string(o->object.size),
			" at offset: ", to_dec_string(p));

	DEBUG("Total number of Objects: ", m_n_objects);

	if (nullptr != ret)
		*ret = o;
	if (nullptr != ret_offset)
		*ret_offset = p;

	return;
}

void 
output_journal_t::append_field(const field_object_t* object, const void* data, const std::size_t len, object_t** ret, uint64_t* ret_offset)
{
	uint64_t 	hash(0), p(0), osize(0);
	object_t*	obj(nullptr);
	signed int	retval(0);

	if (nullptr == object || nullptr == data || 0 >= len)
		throw journal_parameter_error_t("output_journal_t::append_field(): invalid parameter (nullptr == object)");

	hash = get_uint64(object->hash);

	DEBUG("Appending field object of type: ", to_dec_string(object->object.type),
			" flags: ", to_dec_string(object->object.flags),
			" size: ", to_dec_string(object->object.size),
			" hash: ", to_dec_string(object->hash),
			" next_hash_offset: ", to_dec_string(object->next_hash_offset),
			" head_data_offset: ", to_dec_string(object->head_data_offset));

	retval = find_field_object_with_hash(object, data, len, &obj, &p);

	if (0 > retval)
		throw journal_invalid_logic_error_t("output_journal_t::append_field(): error while locating field object with specified hash");
	if (0 < retval) {
		DEBUG("Exisiting field object found at offset: ", to_dec_string(p));
		if (nullptr != ret)
			*ret = obj;
		if (nullptr != ret_offset)
			*ret_offset = p;
		
		return;
	}
	
	DEBUG("Creating new field object");

	osize = offsetof(field_object_t, payload) + get_uint64(object->object.size);
	append_object(object_type_t::OBJECT_FIELD, osize, &obj, &p);

	obj->object.type 			= object->object.type;
	obj->object.size 			= object->object.size;
	obj->object.flags 			= object->object.flags;
	obj->field.hash				= object->hash;
	obj->field.next_hash_offset	= object->next_hash_offset;
	obj->field.head_data_offset	= object->head_data_offset;

	std::memset(&obj->object.res[0], 0, sizeof(obj->object.res));
	write_payload(data, len, p + offsetof(object_t, field.payload));

	link_field(obj, p, hash);
	move_to_object(object_type_t::OBJECT_FIELD, p, &obj);

	if (nullptr != ret)
		*ret = obj;
	if (nullptr != ret_offset)
		*ret_offset = p;
	
	DEBUG("New field object at offset: ", to_dec_string(p));	
	return;
}

void
output_journal_t::append_data(const data_object_t* object, object_t** ret, uint64_t* ret_offset)
{
	uint64_t 	hash(0), p(0), osize(0);
	object_t*	obj(nullptr);
	signed int	retval(0);

	if (nullptr == object)
		throw journal_parameter_error_t("output_journal_t::append_data(): invalid parameters (nullptr == object)");
	if (0 == get_uint64(object->object.size))
		throw journal_invalid_logic_error_t("output_journal_t::append_data(): invalid parameter (object size == 0)");

	DEBUG("Appending data object of type: ", to_dec_string(object->object.type),
			" flags: ", to_dec_string(object->object.flags),
			" size: ", to_dec_string(object->object.size),
			" hash: ", to_dec_string(object->hash),
			" next_hash_offset: ", to_dec_string(object->next_hash_offset),
			" next_field_offset: ", to_dec_string(object->next_field_offset),
			" entry_offset: ", to_dec_string(object->entry_offset),
			" entry_array_offset: ", to_dec_string(object->entry_array_offset),
			" n_entries: ", to_dec_string(object->n_entries));

	hash = get_uint64(object->hash);
	retval = find_data_object_with_hash(object, &obj, &p);

	if (0 > retval)
		throw journal_invalid_logic_error_t("output_journal_t::apend_data(): error while locating data object in data object hash table");
	if (0 < retval) {
		DEBUG("Found existing data item at offset: ", p);

		if (nullptr != ret)
			*ret = obj;

		if (nullptr != ret_offset)
			*ret_offset = p;

		return;
	}

	osize = offsetof(data_object_t, payload) + get_uint64(object->object.size);
	append_object(object_type_t::OBJECT_DATA, osize, &obj, &p);
	// if (JOURNAL_FILE_COMPRESS(f) && size >= f->compress_threshold_bytes) {

	// if (0 == compression)
	obj->object.type 				= object->object.type;
	obj->object.flags 				= object->object.flags;
	obj->object.size				= object->object.size;
	obj->data.hash					= object->hash;
	obj->data.next_hash_offset		= object->next_hash_offset;
	obj->data.next_field_offset		= object->next_field_offset;
	obj->data.entry_offset			= object->entry_offset;
	obj->data.entry_array_offset	= object->entry_array_offset;
	obj->data.n_entries				= object->n_entries;
	std::memset(&obj->object.res[0], 0, sizeof(obj->object.res));

	write_payload(&object->payload[0], get_uint64(object->object.size) - offsetof(data_object_t, payload), p + offsetof(data_object_t, payload));

	link_data(obj, p, hash);
	move_to_object(object_type_t::OBJECT_DATA, p, &obj);

	{
		std::string 	key("");
		std::string		full_value(reinterpret_cast< const char* >(&object->payload[0]), get_uint64(object->object.size) - offsetof(data_object_t, payload));
		field_obj_t 	field_obj(0,0,0);
		field_object_t	fobj({0});
		object_t*		object(nullptr);
		uint64_t		offset(0);

		std::memset(&fobj, 0, sizeof(field_object_t));

		if (false == split_field_value(full_value, key))
			throw journal_invalid_logic_error_t("output_journal_t::append_data(): error locating field value in data object");
		if (false == find_field_value(key, field_obj))
			throw journal_invalid_logic_error_t("output_journal_t::append_data(): error retrieving field object");
		
		DEBUG("Found existing field object in vector: type: ", to_dec_string(field_obj.type()),
				" flags: ", to_dec_string(field_obj.flags()),
				" size: ", to_dec_string(field_obj.object_size()),
				" hash: ", to_dec_string(field_obj.hash()));

		fobj.object.type 		= field_obj.type();
		fobj.object.flags		= get_uint64(field_obj.flags());
		fobj.object.size 		= get_uint64(field_obj.object_size());
		fobj.hash				= get_uint64(field_obj.hash());
		
		fobj.next_hash_offset	= 0;
		fobj.next_hash_offset	= 0;
		fobj.head_data_offset	= 0;

		append_field(&fobj, field_obj.payload().data(), field_obj.payload().size(), &object, &offset);

		obj->data.next_field_offset 	= object->field.head_data_offset;
		object->field.head_data_offset 	= get_uint64(p);	
	}

	if (nullptr != ret)
		*ret = obj;
	if (nullptr != ret_offset)
		*ret_offset = p;

	DEBUG("Linked new data object at offset: ", to_dec_string(p));
	return;
}

void 
output_journal_t::append_entry_internal(const entry_object_t* object, uint64_t* seqnum, object_t** ret, uint64_t* ret_offset)
{
	uint64_t 	np(0), osize(0);
	object_t*	obj(nullptr);

	if (nullptr == object)
		throw journal_parameter_error_t("output_journal_t::append_entry_internal(): invalid parameter(s) encountered");

	osize = offsetof(object_t, entry.items) + (file_entry_n_items(object) * sizeof(entry_item_t));
	append_object(object_type_t::OBJECT_ENTRY, osize, &obj, &np);

	obj->object.type		= get_uint64(object->object.type);
	obj->object.flags		= get_uint64(object->object.flags);
	obj->object.size		= get_uint64(object->object.size);
	obj->entry.seqnum 		= get_uint64(entry_seqnum(seqnum));
	obj->entry.realtime		= get_uint64(object->realtime);
	obj->entry.monotonic	= get_uint64(object->monotonic);
	obj->entry.xor_hash		= get_uint64(object->xor_hash);
	obj->entry.boot_id[0]	= get_uint64(object->boot_id[0]);
	obj->entry.boot_id[1]	= get_uint64(object->boot_id[1]);

	std::memcpy(&obj->entry.items[0], &object->items[0], file_entry_n_items(object) * sizeof(entry_item_t));

	DEBUG("Appending entry: type: ", to_dec_string(obj->object.type),
			" flags: ", to_dec_string(obj->object.flags),
			" size: ", to_dec_string(obj->object.size),
			" seqnum: ", to_dec_string(obj->entry.seqnum),
			" real-time: ", to_dec_string(obj->entry.realtime),
			" monotonic: ", to_dec_string(obj->entry.monotonic),
			" xor_hash: ", to_dec_string(obj->entry.xor_hash),
			" boot_id: ", to_hex_string(obj->entry.boot_id[0]), to_hex_string(obj->entry.boot_id[1]));
	
	// r = journal_file_hmac_put_object(f, OBJECT_ENTRY, o, np);
	link_entry(obj, np);
	
	if (nullptr != ret)
		*ret = obj;
	if (nullptr != ret_offset)
		*ret_offset = np;

	return;
}

void 
output_journal_t::append_entry(const entry_obj_t* object, uint64_t* seqnum, object_t** ret, uint64_t* ret_offset, bool p)
{
	std::size_t		n_items(0);
	entry_item_t*	items(nullptr);
	entry_object_t*	entry(nullptr);

	if (nullptr == object)
		throw journal_parameter_error_t("output_journal_t::append_entry(): invalid parameter encountered (nullptr)");

	DEBUG("Appending entry of type: ", obj_hdr_t::type_string(object->type()),
			" flags: ", to_dec_string(object->flags()),
			" size: ", to_dec_string(object->object_size()),
			" seqnum: ", to_dec_string(object->seqnum()),
			" realtime: ", to_dec_string(object->realtime()),
			" monotonic: ", to_dec_string(object->monotonic()),
			" boot_id: ", to_hex_string(object->boot_id()[0]), to_hex_string(object->boot_id()[1]),
			" xor_hash: ", to_dec_string(object->xor_hash()));

	n_items = std::max(std::size_t(1ULL), object->items().size());

	if (n_items > std::numeric_limits< std::size_t >::max() / sizeof(entry_item_t))
		throw journal_overflow_error_t("output_journal_t::append_entry(): multiplicative overflow encountered in entry item array");

	if (sizeof(entry_object_t) > std::numeric_limits< std::size_t >::max() - (n_items * sizeof(entry_item_t)))
		throw journal_overflow_error_t("output_journal_t::append_entry(): additive overflow encountered in total object size");

	entry = reinterpret_cast< entry_object_t* >(new uint8_t[sizeof(entry_object_t) + (n_items * sizeof(entry_item_t))]);
	std::memset(entry, 0, sizeof(entry_object_t) + (n_items * sizeof(entry_item_t)));

	entry->object.type	= object->type();
	entry->object.flags	= object->flags();
	entry->object.size	= object->object_size();	
	entry->realtime 	= object->realtime();
	entry->monotonic 	= object->monotonic();
	entry->boot_id[0]	= object->boot_id()[0];
	entry->boot_id[1]	= object->boot_id()[1];
	entry->xor_hash		= object->xor_hash();
	
	if (! VALID_REALTIME(entry->realtime))
		throw journal_invalid_logic_error_t("output_journal_t::append_entry(): invalid real-time timestamp in entry");
	if (! VALID_MONOTONIC(entry->monotonic))
		throw journal_invalid_logic_error_t("output_journal_t::append_entry(): invalid monotonic timestamp in entry");

	items = reinterpret_cast< entry_item_t* >(&entry->items[0]);

	for (std::size_t idx = 0; idx < n_items; idx++) {
		uint64_t 		p(0);
		object_t*		o(nullptr);
		obj_hdr_t*		ohdr(object->items()[idx]);
		data_obj_t*		data_object(dynamic_cast< data_obj_t* >(ohdr));
		data_object_t*	dobj(nullptr);

		if (nullptr == ohdr || nullptr == data_object)
			throw journal_invalid_logic_error_t("output_journal_t::append_entry(): invalid item encountered in entry (nullptr)");
		if (object_type_t::OBJECT_DATA != ohdr->type())
			throw journal_invalid_logic_error_t("output_journal_t::append_entry(): invalid item type encountered in entry (!OBJECT_DATA)");

		dobj = reinterpret_cast< data_object_t* >(new uint8_t[sizeof(data_object_t) + data_object->payload().size()]);

		std::memset(dobj, 0, sizeof(data_object_t) + data_object->payload().size());
		std::memcpy(&dobj->payload[0], data_object->payload().data(), data_object->payload().size());

		dobj->object.type	= data_object->type();
		dobj->object.flags	= data_object->flags();
		dobj->object.size	= data_object->object_size(); 
		dobj->hash 			= data_object->hash();
		dobj->n_entries 	= data_object->n_entries();

		append_data(dobj, &o, &p);		

		items[idx].object_offset 	= get_uint64(p);
		items[idx].hash 			= get_uint64(dobj->hash);

		DEBUG("appended data object for item[", to_dec_string(idx), "] at object_offset: ", to_dec_string(p), " hash: ", to_hex_string(dobj->hash));

		delete[] dobj;
		//std::free(dobj);
	}

	// typesafe_qsort(items, n_iovec, entry_item_cmp);

	append_entry_internal(entry, seqnum, ret, ret_offset);
	delete[] entry;
	return;
}

void 
output_journal_t::link_data(object_t* object, const uint64_t offset, const uint64_t hash)
{
	const uint64_t 	dht_size(m_data_hash_table_size / sizeof(hash_item_t));
	const uint64_t 	hash_idx(hash % dht_size);
	uint64_t		p(0);

	if (nullptr == m_data_hash_table || nullptr == object || 0 == offset)
		throw journal_parameter_error_t("output_journal_t::link_data(): invalid parameter(s) encountered");
	if (object_type_t::OBJECT_DATA != object->object.type)
		throw journal_invalid_logic_error_t("output_journal_t::link_data(): invalid object type encountered (!OBJECT_DATA)");
	if (0 >= dht_size)
		throw journal_invalid_logic_error_t("output_journal_t::link_data(): attempted to link data object while missing data hash table");

	object->data.next_hash_offset 	= 0;
	object->data.next_field_offset 	= 0;
	object->data.entry_offset 		= 0;
	object->data.entry_array_offset = 0;
	object->data.n_entries 			= 0;	

	p = m_data_hash_table[hash_idx].tail_hash_offset;
	
	if (0 == p) {
		DEBUG("data hash table, hash_idx: ", to_dec_string(hash_idx), " offset: ", to_dec_string(offset), " get_uint64(offset): ", get_uint64(offset));
		m_data_hash_table[hash_idx].head_hash_offset = get_uint64(offset);
	} else {
		object_t* obj(nullptr);

		move_to_object(object_type_t::OBJECT_DATA, p, &obj);
		obj->data.next_hash_offset = get_uint64(offset);
		DEBUG("Next linking hash offset: ", obj->data.next_hash_offset);
	}

	m_data_hash_table[hash_idx].tail_hash_offset = get_uint64(offset);
	// if (JOURNAL_HEADER_CONTAINS(f->header, n_data))
	m_n_data += 1;
	DEBUG("Tail hash offset for linking: ", m_data_hash_table[hash_idx].tail_hash_offset, " n_data: ", m_n_data);
	return;
}

void
output_journal_t::link_field(object_t* object, const uint64_t offset, const uint64_t hash)
{
	const uint64_t 	fht_size(m_field_hash_table_size / sizeof(hash_item_t));
	const uint64_t 	hash_idx(hash % fht_size);
	uint64_t		p(0);

	if (nullptr == m_field_hash_table || nullptr == object || 0 == offset)
		throw journal_parameter_error_t("output_journal_t::link_field(): invalid parameter(s) encountered");
	if (object_type_t::OBJECT_FIELD != object->object.type)
		throw journal_invalid_logic_error_t("output_journal_t::link_field(): invalid object type encountered (!OBJECT_FIELD)");
	if (0 >= fht_size)
		throw journal_invalid_logic_error_t("output_journal_t::link_field(): attempted to link field object while missing field hash table");

	object->field.next_hash_offset = 0;
	object->field.head_data_offset = 0;

	p = m_field_hash_table[hash_idx].tail_hash_offset;

	DEBUG("link field: tail_hash_offset: ", p, " (", get_uint64(p), ")");

	if (0 == p) 
		m_field_hash_table[hash_idx].head_hash_offset = get_uint64(offset);

	else {
		object_t* obj(nullptr);

		move_to_object(object_type_t::OBJECT_FIELD, p, &obj);
		obj->field.next_hash_offset = get_uint64(offset);
	}

	m_field_hash_table[hash_idx].tail_hash_offset = get_uint64(offset);

	DEBUG("link field, tail hash offset for index: ", to_dec_string(hash_idx), " offset: ", offset, " get_uint64(offset): ", get_uint64(offset)); 	
	// if (JOURNAL_HEADER_CONTAINS(f->header, n_fields))
	m_n_fields += 1;
	return;	
}

void
output_journal_t::link_entry_into_array(uint64_t* first, uint64_t* idx, const uint64_t p)
{
	uint64_t    n(0), ap(0), q(0), i(0), a(0), hidx(0);
	object_t*   object(nullptr);

	if (nullptr == first || nullptr == idx)
		throw journal_parameter_error_t("output_journal_t::link_entry_into_array(): invalid parameter(s) (nullptr)");

	a       = get_uint64(*first);
	i       = get_uint64(*idx);
	hidx    = i;

	DEBUG("link_entry_into_array(0): first: ", to_dec_string(*first), " idx: ", to_dec_string(*idx));

	while (0 < a) {
		move_to_object(object_type_t::OBJECT_ENTRY_ARRAY, a, &object);

		n = file_entry_array_n_items(object);
        
		if (i < n) {
			object->entry_array.items[i] = get_uint64(p);
			*idx = get_uint64(hidx + 1);

			DEBUG("link_entry_into_array(0): object->entry_array.items[", to_dec_string(i), "]: at offset: ", to_dec_string(p));        
			return;
		}

		i   -= n;
		ap  = a;
		a   = get_uint64(object->entry_array.next_entry_array_offset);
	}

	if (hidx > n)
		n = (hidx + 1) * 2;
	else
		n *= 2;

	if (4 > n)
		n = 4;

	append_object(object_type_t::OBJECT_ENTRY_ARRAY, offsetof(object_t, entry_array.items) + n * sizeof(uint64_t), &object, &q);

	// journal_file_hmac_put_object(f, OBJECT_ENTRY_ARRAY, o, q);

	object->entry_array.items[i] = get_uint64(p);
	DEBUG("link_entry_into_array(0): object->entry_array.items[", to_dec_string(i), "]: at offset: ", to_dec_string(p));
	DEBUG("Linked entry into array as item number: ", i, " at offset: ", p);

	if (0 == ap)
		*first = get_uint64(q);
	else {
		move_to_object(object_type_t::OBJECT_ENTRY_ARRAY, ap, &object);

		object->entry_array.next_entry_array_offset = get_uint64(q);
	}

	// if (JOURNAL_HEADER_CONTAINS(f->header, n_entry_arrays))
	m_n_entry_arrays    += 1;
	*idx                = get_uint64(hidx + 1);
	DEBUG("Number of entry arrays: ", m_n_entry_arrays);
	return;
}


void 
output_journal_t::link_entry_into_array(data_object_t* obj, uint64_t* idx, const uint64_t p) 
{
	uint64_t 	n(0), ap(0), q(0), i(0), a(0), hidx(0);
	object_t*	object(nullptr);

	if (nullptr == object || nullptr == idx) 
		throw journal_parameter_error_t("output_journal_t::link_entry_into_array(): invalid parameter(s) (nullptr)");

	a 		= get_uint64(obj->entry_array_offset); 
	i 		= get_uint64(*idx);
	hidx 	= i;

	while (0 < a) {
		move_to_object(object_type_t::OBJECT_ENTRY_ARRAY, a, &object);

		n = file_entry_array_n_items(object);
		
		if (i < n) {
			object->entry_array.items[i] = get_uint64(p);
			*idx = get_uint64(hidx + 1);

			DEBUG("Linking entry into array as item number: ", i, " at offset: ", p);
			return;
		}

		i 	-= n;
		ap 	= a;
		a 	= get_uint64(object->entry_array.next_entry_array_offset);
	}

	if (hidx > n)
		n = (hidx + 1) * 2;
	else
		n *= 2;

	if (4 > n)
		n = 4;

	append_object(object_type_t::OBJECT_ENTRY_ARRAY, offsetof(object_t, entry_array.items) + n * sizeof(uint64_t), &object, &q);
	
	// journal_file_hmac_put_object(f, OBJECT_ENTRY_ARRAY, o, q);

	object->entry_array.items[i] = get_uint64(p);
	DEBUG("Linked entry into array as item number: ", i, " at offset: ", p);
	
	if (0 == ap)
		obj->entry_array_offset = get_uint64(q);
	else {
		move_to_object(object_type_t::OBJECT_ENTRY_ARRAY, ap, &object);

		object->entry_array.next_entry_array_offset = get_uint64(q);
	}
	
	// if (JOURNAL_HEADER_CONTAINS(f->header, n_entry_arrays))
	m_n_entry_arrays 	+= 1;
	*idx 				= get_uint64(hidx + 1);
	DEBUG("Number of entry arrays: ", m_n_entry_arrays);
	return;
}

void 
output_journal_t::link_entry_item(object_t* object, const entry_item_t* item, const uint64_t offset)
{
	uint64_t p(0);

	if (nullptr == object || nullptr == item || 0 >= offset)
		throw journal_parameter_error_t("output_journal_t::link_entry_item(): invalid parameter encountered");

	p = get_uint64(item->object_offset);
	move_to_object(object_type_t::OBJECT_DATA, p, &object);

	link_entry_into_array_plus_one(&object->data, offset); 
	return;
}

void
output_journal_t::link_entry_into_array_plus_one(data_object_t* object, const uint64_t p)
{
	uint64_t hidx(0);

	if (nullptr == object || 0 >= p)
		throw journal_parameter_error_t("output_journal_t::link_entry_into_array_plus_one(): invalid parameter(s) encountered");

	hidx = get_uint64(object->n_entries);

	if (hidx == std::numeric_limits< uint64_t >::max())
		throw journal_invalid_logic_error_t("output_journal_t::link_entry_into_array_plus_one(): invalid index encountered (UINT64_MAX)");

	if (0 == hidx)
		object->entry_offset = get_uint64(p);
	else {
		uint64_t i(get_uint64(hidx - 1));
		link_entry_into_array(object, &i, p);
	}

	object->n_entries = get_uint64(hidx + 1);
	return;
}

void 
output_journal_t::link_entry_into_array_plus_one(uint64_t* extra, uint64_t* first, uint64_t* idx, uint64_t p) 
{
	uint64_t hidx(0);

	if (nullptr == extra || nullptr == first || nullptr == idx || 0 >= p)
		throw journal_parameter_error_t("output_journal_t::link_entry_into_array_plus_one(): invalid parameter(s) encountered");

	hidx = get_uint64(*idx); 

	if (hidx == std::numeric_limits< uint64_t >::max())
		throw journal_invalid_logic_error_t("output_journal_t::link_entry_into_array_plus_one(): invalid index encountered (UINT64_MAX)");

	if (0 == hidx) 
		*extra = get_uint64(p); 
	else {
		uint64_t i(get_uint64(hidx - 1));
		link_entry_into_array(first, &i, p); 
	}

	*idx = get_uint64(hidx+1);
	return;
}

void 
output_journal_t::link_entry(object_t* object, const uint64_t offset)
{
	std::size_t n_items(0);

	if (nullptr == object || 0 >= offset)
		throw journal_parameter_error_t("output_journal_t::link_entry(): invalid parameter(s) encountered");

	if (object_type_t::OBJECT_ENTRY != object->object.type)
		throw journal_invalid_logic_error_t("output_journal_t::link_entry(): invalid object type encountered (!OBJECT_ENTRY)");

	link_entry_into_array(&m_entry_array_offset, &m_n_entries, offset);

	if (0 == m_head_entry_realtime)
		m_head_entry_realtime = object->entry.realtime;

	m_tail_entry_realtime 	= object->entry.realtime;
	m_tail_entry_monotonic	= object->entry.monotonic;

	n_items = file_entry_n_items(object);

	for (uint64_t idx = 0; idx < n_items; idx++) { 
		DEBUG("Linking entry item #", to_dec_string(idx), " object_offset: ", to_dec_string(object->entry.items[idx].object_offset), " hash: ", to_hex_string(object->entry.items[idx].hash));
		link_entry_item(object, &object->entry.items[idx], offset);
	}

	return;	
}

const uint64_t 
output_journal_t::entry_seqnum(uint64_t* seqnum) 
{
	uint64_t r(0);

	r = m_tail_entry_seqnum + 1;

	if (nullptr != seqnum) {
		if (*seqnum + 1 > r)
			r = *seqnum + 1;
		
		*seqnum = r;
	}

	m_tail_entry_seqnum = r;
	
	if (0 == m_head_entry_seqnum)
		m_head_entry_seqnum = r;

	return r;
}

bool 
output_journal_t::next_hash_offset(uint64_t* p, uint64_t* next_hash_offset, uint64_t* depth, uint64_t* header_max_depth)
{
	uint64_t nextp(0);

	if (nullptr == p || nullptr == next_hash_offset || nullptr == depth)
		return false;

	nextp = *next_hash_offset;

	if (0 < nextp) {
		if (nextp <= *p)
			throw journal_invalid_logic_error_t("output_journal_t::next_hash_offset(): hash item loop detected");

		(*depth)++;

		if (nullptr != header_max_depth)
			*header_max_depth = get_uint64(std::max(*depth, get_uint64(*header_max_depth)));
	}

	*p = nextp;
	return true;
}

void
output_journal_t::map_data_hash_table(const bool remap)
{
	uint64_t s(0), p(0);
	void*	 t(nullptr);

	if (false == remap && nullptr != m_data_hash_table)
		return;

	p = m_data_hash_table_offset;	
	s = m_data_hash_table_size;

	move_to(object_type_t::OBJECT_UNUSED, p, s, &t);

	m_data_hash_table = static_cast< hash_item_t* >(t);
	return;
}	

void
output_journal_t::map_field_hash_table(const bool remap)
{
	uint64_t 	s(0), p(0);
	void*		t(nullptr);

	if (false == remap && nullptr != m_field_hash_table)
		return;

	p = m_field_hash_table_offset;
	s = m_field_hash_table_size;

	move_to(object_type_t::OBJECT_UNUSED, p, s, &t);
	
	m_field_hash_table = static_cast< hash_item_t* >(t);
	return;
}

bool
output_journal_t::find_field_object_with_hash(const field_object_t* object, const void* data, const std::size_t len, object_t** ret, uint64_t* ret_offset) 
{
	const uint64_t 	fht_size(m_field_hash_table_size / sizeof(hash_item_t));
	uint64_t		osize(0), hash(0), hash_idx(0), p(0), depth(0);
	uint64_t		nho(0);

	if (nullptr == m_field_hash_table)
		map_field_hash_table();

	if (nullptr == object || 0 == get_uint64(object->object.size) || nullptr == data || 0 >= len)
		return false;

	if (0 >= m_field_hash_table_size)
		return false;

	hash 		= get_uint64(object->hash);
	osize 		= offsetof(object_t, field.payload) + get_uint64(object->object.size);
	hash_idx 	= hash % fht_size;

	p = m_field_hash_table[hash_idx].head_hash_offset;
	
	while (0 < p) {
		object_t* obj(nullptr);

		move_to_object(object_type_t::OBJECT_FIELD, p, &obj);

		if (get_uint64(obj->field.hash) == hash && 
			get_uint64(obj->object.size) == osize && 
			0 == std::memcmp(&obj->field.payload[0], data, len)) {
				if (nullptr != ret)
					*ret = obj;
				if (nullptr != ret_offset)
					*ret_offset = p;

				return true;
		}

		nho = obj->field.next_hash_offset;
		// JOURNAL_HEADER_CONTAINS(f->header, field_hash_chain_depth) ? &f->header->field_hash_chain_depth : NULL)
		if (false == next_hash_offset(&p, &nho, &depth, &m_field_hash_chain_depth))
			return false;
	}

	return false;
}

bool
output_journal_t::setup_field_hash_table(void)
{
	uint64_t 	s(DEFAULT_FIELD_HASH_TABLE_SIZE), p(0);
	object_t*	o(nullptr);

	append_object(object_type_t::OBJECT_FIELD_HASH_TABLE, offsetof(object_t, hash_table.items) + s, &o, &p);
	std::memset(o->hash_table.items, 0, s);

	m_field_hash_table_offset 	= get_uint64(p + offsetof(object_t, hash_table.items));
	m_field_hash_table_size 	= get_uint64(s);

	return true;
}

bool
output_journal_t::setup_data_hash_table(void)
{
	uint64_t 	s(DEFAULT_DATA_HASH_TABLE_SIZE), p(0);
	object_t*	o(nullptr);

	append_object(object_type_t::OBJECT_DATA_HASH_TABLE, offsetof(object_t, hash_table.items) + s, &o, &p);
	std::memset(o->hash_table.items, 0, s);

	m_data_hash_table_offset 	= get_uint64(p + offsetof(object_t, hash_table.items));
	m_data_hash_table_size		= get_uint64(s);

	return true;
}

bool 
output_journal_t::find_data_object_with_hash(const data_object_t* object, object_t** ret, uint64_t* ret_offset) 
{
	const uint64_t 	dht_size(m_data_hash_table_size / sizeof(hash_item_t));
	uint64_t 		osize(0), hash(0), hash_idx(0), p(0), depth(0);
	uint64_t		nho(0);
	
	if (nullptr == m_data_hash_table)
		map_data_hash_table();

	if (nullptr == object || 0 == get_uint64(object->object.size))
		return false;

	if (0 >= m_data_hash_table_size)
		return false;

	hash		= get_uint64(object->hash);
	osize 		= offsetof(object_t, object.payload) + get_uint64(object->object.size);
	hash_idx	= hash % dht_size;

	if (0 == hash_idx)
		return false;

	p = m_data_hash_table[hash_idx].head_hash_offset;

	while (0 < p) {
		object_t* obj(nullptr);

		move_to_object(object_type_t::OBJECT_DATA, p, &obj);
		
		if (get_uint64(obj->data.hash) == hash) {
			// if (o->object.flags & OBJECT_COMPRESSION_MASK) { }

			if (get_uint64(obj->object.size) == osize && 
				0 == std::memcmp(&obj->data.payload[0], &object->payload[0], offsetof(data_object_t, payload) - get_uint64(object->object.size))) 
			{
				if (nullptr != ret)
					*ret = obj;
				if (nullptr != ret_offset)
					*ret_offset = p;

				return true;
			}
		}
		
		nho = obj->data.next_hash_offset;
		// JOURNAL_HEADER_CONTAINS(f->header, data_hash_chain_depth) ? &f->header->data_hash_chain_depth : NULL
		if (false == next_hash_offset(&p, &nho, &depth, &m_data_hash_chain_depth))
			return false;
	}
		
	return false;
}

void 
output_journal_t::write_payload(const void* data, const std::size_t len, const uint64_t offset)
{
	char* ptr(reinterpret_cast< char* >(m_ptr));

	if (nullptr == data || 0 >= len || sizeof(object_header_t) >= offset)
		throw journal_parameter_error_t("output_journal_t::write_payload(): invalid parameter(s)");
	if (offset > std::numeric_limits< std::size_t >::max() - len)
		throw journal_overflow_error_t("output_journal_t::write_payload(): additive overflow in offset+len");
	if (offset+len > m_size)
		throw journal_invalid_logic_error_t("output_journal_t::write_payload(): offset+len > m_size");

	ptr += offset;
	std::memcpy(ptr, data, len);
	return;
}

void
output_journal_t::write_header(void)
{
	header_contents_t* 	hdr(reinterpret_cast< header_contents_t* >(m_ptr));
	const char*			magic("LPKSHHRH");

	if (nullptr == m_ptr || sizeof(header_contents_t) > m_size)
		throw journal_invalid_logic_error_t("output_journal_t::write_header(): invalid memory journal state (nullptr == m_ptr || m_size < sizeof(header)");

	std::memcpy(hdr->signature, magic, 8);
	std::memset(&hdr->reserved[0], 0, sizeof(hdr->reserved));

	hdr->compatible_flags 			= get_uint64(m_compatible_flags);
	hdr->incompatible_flags			= get_uint64(m_incompatible_flags);
	hdr->state						= m_state;
	hdr->file_id[0]					= get_uint64(m_file_id[0]);
	hdr->file_id[1]					= get_uint64(m_file_id[1]);
	hdr->machine_id[0]				= get_uint64(m_machine_id[0]);
	hdr->machine_id[1]				= get_uint64(m_machine_id[1]);
	hdr->boot_id[0]					= get_uint64(m_boot_id[0]);
	hdr->boot_id[1]					= get_uint64(m_boot_id[1]);
	hdr->seqnum_id[0]				= get_uint64(m_seqnum_id[0]);
	hdr->seqnum_id[1]				= get_uint64(m_seqnum_id[1]);
	hdr->header_size				= get_uint64(m_header_size);
	hdr->arena_size					= get_uint64(m_arena_size);
	hdr->data_hash_table_offset		= get_uint64(m_data_hash_table_offset);
	hdr->data_hash_table_size		= get_uint64(m_data_hash_table_size);
	hdr->field_hash_table_offset	= get_uint64(m_field_hash_table_offset);
	hdr->field_hash_table_size		= get_uint64(m_field_hash_table_size);
	hdr->tail_object_offset			= get_uint64(m_tail_object_offset);
	hdr->n_objects					= get_uint64(m_n_objects);
	hdr->n_entries					= get_uint64(m_n_entries);
	hdr->tail_entry_seqnum			= get_uint64(m_tail_entry_seqnum);
	hdr->head_entry_seqnum			= get_uint64(m_head_entry_seqnum);
	hdr->entry_array_offset			= get_uint64(m_entry_array_offset);
	hdr->head_entry_realtime		= get_uint64(m_head_entry_realtime);
	hdr->tail_entry_realtime		= get_uint64(m_tail_entry_realtime);
	hdr->tail_entry_monotonic		= get_uint64(m_tail_entry_monotonic);
	hdr->n_data						= get_uint64(m_n_data);
	hdr->n_fields					= get_uint64(m_n_fields);
	hdr->n_tags						= get_uint64(m_n_tags);
	hdr->n_entry_arrays				= get_uint64(m_n_entry_arrays);
	hdr->data_hash_chain_depth		= get_uint64(m_data_hash_chain_depth);
	hdr->field_hash_chain_depth		= get_uint64(m_field_hash_chain_depth);

	return;	
}

void 
output_journal_t::update(std::list< entry_obj_t >& entries)
{

	if (true == entries.empty())
		return; 

	m_data_objects.clear();
	m_entry_objects.clear();

	entries.sort();

	for (std::list< entry_obj_t >::iterator itr = entries.begin(); itr != entries.end(); itr++) {
		const std::vector< obj_hdr_t* >& 	items(itr->items());
		const std::size_t					mival(items.size()); 

		for (std::size_t eidx = 0; eidx < mival; eidx++) {
			obj_hdr_t* obj(items[eidx]);

			if (nullptr == obj)
				throw journal_invalid_logic_error_t("output_journal_t::update(): invalid entry item encountered (nullptr)");

			switch (obj->type()) {
				case object_type_t::OBJECT_DATA:
				{
					data_obj_t* dobj = dynamic_cast< data_obj_t* >(obj);
	
					if (nullptr == dobj)
						throw dynamic_cast_failure_error_t("output_journal_t::update(): object of correct type failed dynamic_cast (data_obj_t)");
								
					if (false == has_data_object(*dobj))
						m_data_objects.push_back(*dobj);

				}
				break;

				default:
					throw journal_invalid_logic_error_t("output_journal_t::update(): unknown/unsupported entry object item type encountered");
				break;
				
			}
		}

		m_entry_objects.push_back(*itr);
	}

	{
		const std::size_t 			dvmax(m_data_objects.size());
		std::list< field_obj_t >	flist;

		for (std::size_t idx = 0; idx < dvmax; idx++) {
			std::string key("");
			bool		found(false);
			field_obj_t obj(0,0,0);

			if (false == split_field_value(m_data_objects[idx].to_string(), key)) 
				throw journal_invalid_logic_error_t("output_journal_t::update(): failure while splitting data value");

			if (false == find_field_value(key, obj))
				throw journal_invalid_logic_error_t("output_journal_t::update(): faild to find data object name in field object vector");

			for (std::list< field_obj_t >::iterator itr = flist.begin(); itr != flist.end(); itr++) {
				const std::string flist_field(itr->to_string());
				const std::string fvec_field(obj.to_string());
				const std::size_t len(fvec_field.length());

				if (itr->hash() == obj.hash() && len == flist_field.length() && 0 == fvec_field.compare(flist_field)) {
					found = true;
					break;
				}
			}

			if (false == found) 
				flist.push_back(obj);
		}

		m_field_objects.clear();
		m_field_objects.insert(m_field_objects.begin(), flist.begin(), flist.end());
	}

	m_data_hash_table_offset 	= 0;
	m_data_hash_table_size 		= 0;
	m_field_hash_table_offset 	= 0;
	m_field_hash_table_size 	= 0;
	m_tail_object_offset 		= 0;
	m_n_objects 				= 0;
	m_tail_entry_seqnum 		= 0;
	m_head_entry_seqnum 		= 0;
	m_entry_array_offset 		= 0;
	m_head_entry_realtime 		= 0;
	m_tail_entry_realtime 		= 0;
	m_tail_entry_monotonic 		= 0;
	m_data_hash_chain_depth 	= 0;
	m_field_hash_chain_depth 	= 0;

	m_n_tags	= m_tag_objects.size();
	m_n_entries = m_entry_objects.size();
	m_n_data	= m_data_objects.size();
	m_n_fields 	= m_field_objects.size();

	return;
}

bool
output_journal_t::write(void)
{
	const std::size_t 	emval(m_entry_objects.size());
	output_file_t		ofile(m_name.c_str());

	if (nullptr != m_ptr) 
		dealloc();

	m_arena_size				= 0;
	m_data_hash_table_offset    = 0;
	m_data_hash_table_size      = 0;
	m_field_hash_table_offset   = 0;
	m_field_hash_table_size     = 0;
	m_tail_object_offset        = 0;
	m_n_objects                 = 0;
	m_tail_entry_seqnum         = 0;
	m_head_entry_seqnum         = 0;
	m_entry_array_offset        = 0;
	m_head_entry_realtime       = 0;
	m_tail_entry_realtime       = 0;
	m_tail_entry_monotonic      = 0;
	m_data_hash_chain_depth     = 0;
	m_field_hash_chain_depth    = 0;
	m_n_tags					= 0;
	m_n_entries					= 0;
	m_n_data					= 0;
	m_n_fields					= 0;
	m_n_entry_arrays			= 0;

	if (false == allocate(m_header_size, 1))
		return false;

	std::memset(m_ptr, 0, m_size);

	// journal_file_hmac_setup

	if (false == setup_field_hash_table() || false == setup_data_hash_table())
		return false;

	// journal_file_append_first_tag

	for (std::size_t idx = 0; idx < emval; idx++) {
		entry_obj_t e(m_entry_objects[idx]);
		append_entry(&e, nullptr, nullptr, nullptr);
	}

	write_header();
	ofile.open();
	ofile.write(m_ptr, m_size);
	ofile.close();
	return true; 
}

