#include "input_journal.hpp"

input_journal_t::input_journal_t(void)
	: journal_base_t(), m_parsed(false)
{
	return;
}

input_journal_t::input_journal_t(const char* name)
	: journal_base_t(name), m_parsed(false)
{
	return;
}

input_journal_t::~input_journal_t(void) 
{ 
	reset();
	return; 
}

void
input_journal_t::verify_file(const header_contents_t* hdr) const
{
	uint64_t 		off(m_header_size);
	object_t*		obj(nullptr);
	uint64_t		nobjects(0), nfields(0), ntags(0), nentries(0), ndata_hash_tables(0), nfield_hash_tables(0);
	uint64_t		nentry_arrays(0), ndata(0);
	uint64_t		last_tag_realtime(0), entry_seqnum(0), entry_monotonic(0), entry_realtime(0), last_tag(0), last_epoch(0);
	bool			entry_seqnum_set(false), entry_monotonic_set(false), entry_realtime_set(false);
	bool			found_main_entry_array(false), found_last(false);
	uint128_vec_t	entry_boot_id({0,0});


	if (nullptr == hdr)
		throw journal_parameter_error_t("input_journal_t::verify_file(): invalid parameter encountered (nullptr)");

	DEBUG("Verifying all objects...");

	for (;;) {
		DEBUG("Moving to offset:              ", to_dec_string(off), " (", to_dec_string(m_tail_object_offset), ")");
		move_to_object(object_type_t::OBJECT_UNUSED, off, &obj);

		if (off > m_tail_object_offset)
			throw journal_verification_error_t("input_journal_t::verify_file(): invalid tail object offset encountered");

		nobjects++;
		DEBUG("Found Object of Type:          ", obj_hdr_t::type_string(obj->object.type), " (", obj_hdr_t::flags_string(obj->object.flags), ")");
		DEBUG("Verifying Object #:            ", to_dec_string(nobjects), " (", to_dec_string(m_n_objects), ")");

		verify_object(off, obj);

		if (1 <
			!!(obj->object.flags & OBJECT_COMPRESSED_XZ) +
			!!(obj->object.flags & OBJECT_COMPRESSED_LZ4) +
			!!(obj->object.flags & OBJECT_COMPRESSED_ZSTD))
				throw journal_verification_error_t("input_journal_t::verify_file(): object has multiple compression type flags specified");

		if ((obj->object.flags & OBJECT_COMPRESSED_XZ) && !(IS_HEADER_INCOMPATIBLE_COMPRESSED_XZ(m_incompatible_flags)))
			throw journal_verification_error_t("input_journal_t::verify_file(): XZ compressed object encountered in file lacking XZ compression");

		if ((obj->object.flags & OBJECT_COMPRESSED_LZ4) && !(IS_HEADER_INCOMPATIBLE_COMPRESSED_LZ4(m_incompatible_flags)))
			throw journal_verification_error_t("input_journal_t::verify_file(): LZ4 compressed object encountered in file lacking LZ4 compression");

		if ((obj->object.flags & OBJECT_COMPRESSED_ZSTD) && !(IS_HEADER_INCOMPATIBLE_COMPRESSED_ZSTD(m_incompatible_flags)))
			throw journal_verification_error_t("input_journal_t::verify_file(): ZSTD compressed object encountered in file lacking ZSTD compression");

		switch (obj->object.type) {
			case object_type_t::OBJECT_DATA:
				ndata++;
			break;
		
			case object_type_t::OBJECT_FIELD:
				nfields++;
			break;

			case object_type_t::OBJECT_ENTRY:
				if (IS_HEADER_COMPATIBLE_SEALED(m_compatible_flags) && 0 == m_n_tags)
					throw journal_verification_error_t("input_journal_t::verify_file(): encountered entry prior to first tag");

				if (get_uint64(obj->entry.realtime) < last_tag_realtime)
					throw journal_verification_error_t("input_journal_t::verify_file(): older entry after new tag encountered");

				if (! entry_seqnum_set && get_uint64(obj->entry.seqnum) != m_head_entry_seqnum)
					throw journal_verification_error_t("input_journal_t::verify_file(): head sequence number incorrect");

				if (true == entry_seqnum_set && entry_seqnum >= get_uint64(obj->entry.seqnum))
					throw journal_verification_error_t("input_journal_t::verify_file(): entry sequence number out of synchronization");

				entry_seqnum		= get_uint64(obj->entry.seqnum);
				entry_seqnum_set	= true;

				if (true == entry_monotonic_set && entry_boot_id[0] == get_uint64(obj->entry.boot_id[0]) &&
					entry_boot_id[1] == get_uint64(obj->entry.boot_id[1]) && entry_monotonic > get_uint64(obj->entry.monotonic)) 
					throw journal_verification_error_t("input_journal_t::verify_file(): entry timestamp out of synchronization");

				entry_monotonic		= get_uint64(obj->entry.monotonic);
				entry_boot_id[0]	= get_uint64(obj->entry.boot_id[0]);
				entry_boot_id[1]	= get_uint64(obj->entry.boot_id[1]);
				entry_monotonic_set	= true;

				if (! entry_realtime_set && get_uint64(obj->entry.realtime) != m_head_entry_realtime)
					throw journal_verification_error_t("input_journal_t::verify_file(): head entry timestamp incorrect");

				entry_realtime		= get_uint64(obj->entry.realtime);
				entry_realtime_set	= true;

				nentries++;
			break;

			case object_type_t::OBJECT_DATA_HASH_TABLE:
				if (1 < ndata_hash_tables)
					throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of data hash tables encountered (>1)");

				if ((m_data_hash_table_offset != off + offsetof(hash_table_object_t, items)) ||
					(m_data_hash_table_size != get_uint64(obj->object.size) - offsetof(hash_table_object_t, items)))
					throw journal_verification_error_t("input_journal_t::verify_file(): invalid header fields for data hash table encountered");

				ndata_hash_tables++;

			break;

			case object_type_t::OBJECT_FIELD_HASH_TABLE:
				if (1 < nfield_hash_tables)
					throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of field hash tables encountered (>1)");

				if ((m_field_hash_table_offset != off + offsetof(hash_table_object_t, items)) ||
					(m_field_hash_table_size != get_uint64(obj->object.size) - offsetof(hash_table_object_t, items)))
					throw journal_verification_error_t("input_journal_t::verify_file(): invalid header fields for field hash table encountered");

				nfield_hash_tables++;
			break;

			case object_type_t::OBJECT_ENTRY_ARRAY:
				if (off == m_entry_array_offset) {
					if (true == found_main_entry_array)
						throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of main entry arrays encountered (>1)");
					
					found_main_entry_array = true;
				}

				nentry_arrays++;
			break;

			case object_type_t::OBJECT_TAG:
				if (! IS_HEADER_COMPATIBLE_SEALED(m_compatible_flags))
					throw journal_verification_error_t("input_journal_t::verify_file(): tag object encountered in unsealed file");

				if (get_uint64(obj->tag.seqnum) != ntags+1)
					throw journal_verification_error_t("input_journal_t::verify_file(): tag sequence numbers out of synchronization");

				if (get_uint64(obj->tag.epoch) < last_epoch)
					throw journal_verification_error_t("input_journal_t::verify_file(): tag epoch sequence numbers out of synchronization");
			
				if (true == m_seal)
					throw journal_verification_error_t("input_journal_t::verify_file(): sealing semantics not yet implemented");

				last_tag    = off + ALIGN64(get_uint64(obj->object.size));
				last_epoch  = get_uint64(obj->tag.epoch);
				
				ntags++;
			break;

			default:
				throw journal_verification_error_t("input_journal_t::verify_file(): invalid/unknown object type encountered");
			break;
		}

		if (off == m_tail_object_offset) {
			found_last = true;
			break;
		}

		off += ALIGN64(get_uint64(obj->object.size));
	}

	// XXX JNF
	last_tag++;

	if (! found_last && 0 != m_tail_object_offset)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid tail object pointer encountered");

	if (nobjects != m_n_objects)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of objects encountered");

	if (nentries != m_n_entries)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of entries encountered");

	if (JOURNAL_HEADER_CONTAINS(hdr, n_data) && ndata != m_n_data)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of data entries encountered");

	if (JOURNAL_HEADER_CONTAINS(hdr, n_fields) && nfields != m_n_fields)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of fields encountered");

	if (JOURNAL_HEADER_CONTAINS(hdr, n_tags) && ntags != m_n_tags)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of tags encountered");

	if (JOURNAL_HEADER_CONTAINS(hdr, n_entry_arrays) && nentry_arrays != m_n_entry_arrays)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid number of entry arrays encountered");

	if (! found_main_entry_array && 0 != m_entry_array_offset)
		throw journal_verification_error_t("input_journal_t::verify_file(): did not encounter an entry array despite one specified in file header");

	if (true == entry_seqnum_set && entry_seqnum != m_tail_entry_seqnum)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid tail sequence number encountered");

	if (true == entry_monotonic_set && entry_boot_id[0] == m_boot_id[0] && entry_boot_id[1] == m_boot_id[1] &&
		entry_monotonic != m_tail_entry_monotonic)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid tail monotonic timestamp encountered");

	if (true == entry_realtime_set && entry_realtime != m_tail_entry_realtime)
		throw journal_verification_error_t("input_journal_t::verify_file(): invalid tail realtime timestamp encountered");

	verify_entry_array();
	verify_hash_array();

	return;
}

void
input_journal_t::verify_offsets(void) const
{
	if (nullptr == m_ptr || 0 == m_size)
		throw journal_verification_error_t("input_journal_t::verify_offsets(): Invalid object state (null/0 size)");

	if (false == m_parsed)
		throw journal_verification_error_t("input_journal_t::verify_offsets(): Invalid object state (!parsed)");

	if (m_header_size > m_size)
		throw journal_verification_error_t("input_journal_t::verify_offsets(): Invalid Header Size");

	if (m_arena_size > m_size)
		throw journal_verification_error_t("input_journal_t::verify_offsets(): Invalid Arena Size");

    if (m_data_hash_table_offset > m_size ||
        m_data_hash_table_offset > std::numeric_limits< uint64_t >::max() - m_data_hash_table_size ||
        m_data_hash_table_size > m_size - m_data_hash_table_offset) {
     	   throw journal_verification_error_t("input_journal_t::verify_offsets(): Invalid Data Hash Table Offset/Size");
	}
	
	if (m_field_hash_table_offset > m_size ||
		m_field_hash_table_offset > std::numeric_limits< uint64_t >::max() - m_field_hash_table_size ||
		m_field_hash_table_size > m_size - m_field_hash_table_offset) {
			throw journal_verification_error_t("input_journal_t::verify_offsets(): Invalid Filed Hash Table Offset/Size");
	}

	if (m_tail_object_offset > m_size)
		throw journal_verification_error_t("input_journal_t::verify_offsets(): Invalid Tail Object Offset");

	if (m_entry_array_offset > m_size)
		throw journal_verification_error_t("input_journal_t::verify_offsets(): Invalid Entry Array Offset");

	return;	
}

void
input_journal_t::verify_object(const uint64_t& offset, const object_t* obj) const
{
	if (nullptr == obj)
		throw journal_parameter_error_t("input_journal_t::verify_object(): invalid parameter encountered (nullptr)");

	if ((obj->object.flags & OBJECT_COMPRESSED_XZ) && OBJECT_DATA != obj->object.type)
		throw journal_verification_error_t("input_journal_t::verify_object(): compressed object not of type DATA encountered");

	switch (obj->object.type) {
		case object_type_t::OBJECT_DATA:
		{
			uint64_t hash1(get_uint64(obj->data.hash)), hash2(0);
			signed int compression(obj->object.flags & OBJECT_COMPRESSION_MASK); //, r(0);

			if (0 == get_uint64(obj->data.entry_offset))
				throw journal_verification_error_t("input_journal_t::verify_object(): unused DATA object encountered");

			if ((0 == get_uint64(obj->data.entry_offset)) ^ (0 == get_uint64(obj->data.n_entries)))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid n_entries encountered in DATA object");
	
			if (0 >= get_uint64(obj->object.size) - offsetof(data_object_t, payload))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid DATA object size encountered");

			if (0 != compression) {
				DEBUG("input_journal_t::verify_object(): unsupported compressed DATA object encountered");
				//throw std::runtime_error("input_journal_t::verify_object(): unsupported compressed DATA object encountered");
			} else
				hash2 = hash_data(obj->data.payload, get_uint64(obj->object.size) - offsetof(object_t, data.payload));

			// This is just here to suppress the warning about unused variables
			// which itself only exists because the commented out code below is
			// non-functional and just waiting on me to debug it to find out 
			// where the hash calculation is incorrect.
			hash2++;
			hash1++;

			//if (hash1 != hash2) {
			//	INFO("verify_object(): non-matching hashes encountered: ", to_hex_string(hash1), " -> ", to_hex_string(hash2));
			//	throw std::runtime_error("input_journal_t::verify_object(): invalid hash encountered in DATA object");
			//}


			if (! VALID64(obj->data.next_hash_offset) ||
				! VALID64(obj->data.next_field_offset) ||
				! VALID64(obj->data.entry_offset) ||
				! VALID64(obj->data.entry_array_offset))
				throw journal_verification_error_t("input_journal_t::verify_object(): one or more invalid offsets in DATA object encountered");
		}
		break;

		case object_type_t::OBJECT_FIELD:
			if (0 >= (get_uint64(obj->object.size) - offsetof(field_object_t, payload)))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid FIELD object size encountered");

			if (! VALID64(obj->field.next_hash_offset) || ! VALID64(obj->field.head_data_offset))
				throw journal_verification_error_t("input_journal_t::verify_object(): one or more invalid offsets in FIELD object encountered");
		break;

		case object_type_t::OBJECT_ENTRY:
			if (0 != ((get_uint64(obj->object.size) - offsetof(entry_object_t, items)) % sizeof(entry_item_t)))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY object size encountered (%)");

			if (0 >= ((get_uint64(obj->object.size) - offsetof(entry_object_t, items)) / sizeof(entry_item_t)))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY object size encountered (/)");

			if (0 >= get_uint64(obj->entry.seqnum))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY sequence number encountered");

			if (! VALID_REALTIME(get_uint64(obj->entry.realtime)))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY realtime timestamp encountered");

			if (! VALID_MONOTONIC(get_uint64(obj->entry.monotonic)))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY monotonic timestamp encountered");

			for (std::size_t idx = 0; idx < file_entry_n_items(obj); idx++) {
				if (0 == get_uint64(obj->entry.items[idx].object_offset) || ! VALID64(obj->entry.items[idx].object_offset)) 
					throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY item encountered");
			}
		break;

		case object_type_t::OBJECT_DATA_HASH_TABLE:
		case object_type_t::OBJECT_FIELD_HASH_TABLE:
			if (0 != ((get_uint64(obj->object.size) - offsetof(hash_table_object_t, items)) % sizeof(hash_item_t)) ||
				0 >= ((get_uint64(obj->object.size) - offsetof(hash_table_object_t, items)) / sizeof(hash_item_t)))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid DATA or FIELD HASH TABLE size encountered");

			for (std::size_t idx = 0; idx < file_hash_table_n_items(obj); idx++) {
				if (0 != obj->hash_table.items[idx].head_hash_offset &&
					! VALID64(get_uint64(obj->hash_table.items[idx].head_hash_offset)))
					throw journal_verification_error_t("input_journal_t::verify_object(): invalid head_hash_offset encountered");

				if (0 != obj->hash_table.items[idx].tail_hash_offset &&
					! VALID64(get_uint64(obj->hash_table.items[idx].tail_hash_offset)))
					throw journal_verification_error_t("input_journal_t::verify_object(): invalid tail_hash_offset encountered");

				if ((0 != obj->hash_table.items[idx].head_hash_offset) != (0 != obj->hash_table.items[idx].tail_hash_offset))
					throw journal_verification_error_t("input_journal_t::verify_object(): invalid hash table item encountered");
			}
		break;

		case object_type_t::OBJECT_ENTRY_ARRAY:
			if (0 != ((get_uint64(obj->object.size) - offsetof(entry_array_object_t, items))  % sizeof(uint64_t)) ||
				(0 >= ((get_uint64(obj->object.size) - offsetof(entry_array_object_t, items)) / sizeof(uint64_t))))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY ARRAY object size encountered");

			if (! VALID64(obj->entry_array.next_entry_array_offset))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY ARRAY object next_entry_array_offset encountered");

			for (std::size_t idx = 0; idx < file_entry_array_n_items(obj); idx++) {
				if (0 != get_uint64(obj->entry_array.items[idx]) && ! VALID64(get_uint64(obj->entry_array.items[idx])))
					throw journal_verification_error_t("input_journal_t::verify_object(): invalid ENTRY ARRAY item encountered");
			}
			
		break;

		case object_type_t::OBJECT_TAG:
			if (sizeof(tag_object_t) != get_uint64(obj->object.size))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid TAG object size encountered");

			if (! VALID_EPOCH(get_uint64(obj->tag.epoch)))
				throw journal_verification_error_t("input_journal_t::verify_object(): invalid TAG epoch timestamp encountered");

		break;

		default:
			throw journal_verification_error_t("input_journal_t::verify_object(): unknown/invalid object type encountered");
		break;
	}

	return;
}

void 
input_journal_t::verify_entry_array(void) const
{
		uint64_t idx(0), array_off(0), nentries(0), last(0);
	
		nentries 	= m_n_entries;
		array_off 	= m_entry_array_offset;

		DEBUG("Verifying Entry Arrays...");	
		while (idx < nentries) {
			uint64_t 	next(0), m(0);
			object_t*	obj(nullptr);

			move_to_object(OBJECT_ENTRY_ARRAY, array_off, &obj);
			next = get_uint64(obj->entry_array.next_entry_array_offset);

			m = file_entry_array_n_items(obj);

			DEBUG("Found Entry Array at offset ", to_dec_string(array_off), " with ", to_dec_string(m), " entries of ", to_dec_string(nentries), " total entries");
			for (uint64_t j = 0; idx < nentries && j < m; idx++, j++) {
				uint64_t p(get_uint64(obj->entry_array.items[j]));

				if (p <= last)
					throw journal_verification_error_t("input_journal_t::verify_entry_array(): unsorted entry array encountered");

				last = p;
				
				move_to_object(OBJECT_ENTRY, p, &obj);
				DEBUG("Found Entry Object #: ", to_dec_string(idx), " of size: ", to_dec_string(get_uint64(obj->object.size)));
				verify_object(p, obj);
				move_to_object(OBJECT_ENTRY_ARRAY, array_off, &obj);
			}

			array_off = next;
		}
		return;
}

void 
input_journal_t::verify_hash_array(void) const
{
	const uint64_t 	nitems(m_data_hash_table_size / sizeof(hash_item_t));
	hash_item_t*	htable(nullptr);

	DEBUG("Verifying Hash Array...");

	move_to(OBJECT_DATA_HASH_TABLE, m_data_hash_table_offset, m_data_hash_table_size, reinterpret_cast< void** >(&htable));

	DEBUG("Found Data Hash Table Object at offset: ", to_dec_string(m_data_hash_table_offset), " with ", to_dec_string(nitems), " items");

	for (uint64_t idx = 0; idx < nitems; idx++) {
		uint64_t last(0), hash_off(get_uint64(htable[idx].head_hash_offset));

		while (0 != hash_off) {
			object_t* 	obj(nullptr);
			uint64_t 	next(0);

			move_to_object(OBJECT_DATA, hash_off, &obj);

			DEBUG("Found Data Object #: ", to_dec_string(idx), " at offset: ", to_hex_string(hash_off), " of size: ", to_dec_string(get_uint64(obj->object.size)));
			next = get_uint64(obj->data.next_hash_offset);

			if (0 != next && next <= hash_off)
				throw journal_verification_error_t("input_journal_t::verify_hash_array(): hash chain cycle encountered");

			if ((get_uint64(obj->data.hash) % nitems) != idx)
				throw journal_verification_error_t("input_journal_t::verify_hash_array(): hash value mismatch encountered");

			last 		= hash_off;
			hash_off 	= next;
		}

		if (last != get_uint64(htable[idx].tail_hash_offset)) 
			throw journal_verification_error_t("input_journal_t::verify_hash_array(): tail hash pointer mismatch in hash table encountered");
	}

	return;
}

obj_hdr_t*
input_journal_t::to_cpp_object(const object_t* obj) const
{
	if (nullptr == obj)
		throw journal_parameter_error_t("input_journal_t::to_cpp_object(): invalid parameter encountered (nullptr)");

	switch (obj->object.type) {
        case object_type_t::OBJECT_DATA:
        {
            data_obj_t* dobj(new data_obj_t(obj->object.flags, obj->object.size, get_uint64(obj->data.hash), get_uint64(obj->data.n_entries)));

            dobj->payload().resize(get_uint64(obj->object.size) - offsetof(data_object_t, payload));
            std::memcpy(dobj->payload().data(), &obj->data.payload[0], get_uint64(obj->object.size) - offsetof(data_object_t, payload));
			return dobj;
        }
		break;

		case object_type_t::OBJECT_FIELD:
        {
            field_obj_t* fobj(new field_obj_t(obj->object.flags, obj->object.size, get_uint64(obj->data.hash)));

            fobj->payload().resize(get_uint64(obj->object.size) - offsetof(field_object_t, payload));
            std::memcpy(fobj->payload().data(), &obj->field.payload[0], get_uint64(obj->object.size) - offsetof(field_object_t, payload));
			return fobj;
        }
		break;

        case object_type_t::OBJECT_ENTRY:
        {
            entry_obj_t* eobj(new entry_obj_t(obj->object.flags, obj->object.size, get_uint64(obj->entry.seqnum), 
												get_uint64(obj->entry.realtime), get_uint64(obj->entry.monotonic),
                            {get_uint64(obj->entry.boot_id[0]), get_uint64(obj->entry.boot_id[1])}, get_uint64(obj->entry.xor_hash)));

            for (std::size_t idx = 0; idx < file_entry_n_items(obj); idx++) {
                object_t* tmp(nullptr);
                move_to_object(object_type_t::OBJECT_UNUSED, get_uint64(obj->entry.items[idx].object_offset), &tmp);
            	eobj->items().push_back(to_cpp_object(tmp));
			}

			return eobj;
        }
		break;

        case object_type_t::OBJECT_TAG:
        {
            tag_obj_t* tobj(new tag_obj_t(obj->object.flags, obj->object.size, get_uint64(obj->tag.seqnum), get_uint64(obj->tag.epoch)));

            std::memcpy(&tobj->tag()[0], &obj->tag.tag[0], tobj->tag().size());
        	return tobj;
		}
        break;

		default:
			break;
	}

	throw journal_parameter_error_t("input_journal_t::to_cpp_object(): invalid/unknown object type provided");
	return nullptr;
}

void
input_journal_t::reset(void)
{
	m_parsed = false;
	journal_base_t::reset();
	return;
}

void
input_journal_t::parse(const char* name)
{
	input_file_t 		inf;
	header_contents_t*	hdr(nullptr);
	std::size_t			siz(0);
	//uint8_t*			ptr(nullptr);
	uint64_t			off(0);
	object_t*			obj(nullptr);

	if (nullptr == name)
		throw journal_parameter_error_t("input_journal_t::parse(): invalid filename (null)");
		
	inf.open(name);
		
	siz = static_cast< std::size_t >(inf.size());

	if (0 > static_cast< off_t >(siz)) 
		throw journal_parse_error_t("input_journal_t::parse(): invalid file size");

	reset();

	if (false == alloc(siz))
		throw std::runtime_error("input_journal_t::parse(): error allocating memory");

	try {
		inf.read(m_ptr, siz);
	} catch (std::exception& e) {
		throw e;
	}

	inf.close();
	hdr = reinterpret_cast< header_contents_t* >(m_ptr);
			
	if (0 != std::memcmp(&hdr->signature[0], HEADER_SIGNATURE, 8))
		throw journal_parse_error_t("input_journal_t::parse(): invalid file magic"); 

	DEBUG("File Header: ");
	DEBUG("===============================================================");
	m_compatible_flags 			= get_uint32(hdr->compatible_flags);
	DEBUG("Compatible Flags:              ", to_hex_string(m_compatible_flags), " (", compatible_flags_string(), ")");
	
	m_incompatible_flags 		= get_uint32(hdr->incompatible_flags);
	DEBUG("Incompatible Flags:            ", to_hex_string(m_incompatible_flags), " (", incompatible_flags_string(), ")");

	m_state						= hdr->state;
	DEBUG("State:                         ", to_hex_string(m_state), " (", state_string(), ")");

	m_file_id[0]				= get_uint64(hdr->file_id[0]);
	m_file_id[1]				= get_uint64(hdr->file_id[1]);
	DEBUG("File ID:                       ", std::string(to_hex_string(m_file_id[0]) + to_hex_string(m_file_id[1])));

	m_machine_id[0]				= get_uint64(hdr->machine_id[0]);
	m_machine_id[1]				= get_uint64(hdr->machine_id[1]);
	DEBUG("Machine ID:                    ", std::string(to_hex_string(m_machine_id[0]) + to_hex_string(m_machine_id[1])));

	m_boot_id[0]				= get_uint64(hdr->boot_id[0]);
	m_boot_id[1]				= get_uint64(hdr->boot_id[1]);
	DEBUG("Boot ID:                       ", std::string(to_hex_string(m_boot_id[0]) + to_hex_string(m_boot_id[1])));

	m_seqnum_id[0]				= get_uint64(hdr->seqnum_id[0]);
	m_seqnum_id[1]				= get_uint64(hdr->seqnum_id[1]);
	DEBUG("Sequence Number ID:            ", std::string(to_hex_string(m_seqnum_id[0]) + to_hex_string(m_seqnum_id[1])));

	m_header_size				= get_uint64(hdr->header_size);
	DEBUG("Header Size:                   ", to_dec_string(m_header_size));

	m_arena_size				= get_uint64(hdr->arena_size);
	DEBUG("Arena Size:                    ", to_dec_string(m_arena_size));

	m_data_hash_table_offset	= get_uint64(hdr->data_hash_table_offset);
	DEBUG("Data Hash Table Offset:        ", to_hex_string(m_data_hash_table_offset));

	m_data_hash_table_size		= get_uint64(hdr->data_hash_table_size);
	DEBUG("Data Hash Table Size:          ", to_dec_string(m_data_hash_table_size));

	m_field_hash_table_offset	= get_uint64(hdr->field_hash_table_offset);
	DEBUG("Field Hash Table Offset:       ", to_hex_string(m_field_hash_table_offset));

	m_field_hash_table_size		= get_uint64(hdr->field_hash_table_size);
	DEBUG("Field Hash Table Size:         ", to_dec_string(m_field_hash_table_size));

	m_tail_object_offset		= get_uint64(hdr->tail_object_offset);
	DEBUG("Tail Object Offset:            ", to_hex_string(m_tail_object_offset));

	m_n_objects					= get_uint64(hdr->n_objects);
	DEBUG("Number of Objects:             ", to_dec_string(m_n_objects));

	m_n_entries					= get_uint64(hdr->n_entries);
	DEBUG("Number of Entries:             ", to_dec_string(m_n_entries));
	
	m_tail_entry_seqnum			= get_uint64(hdr->tail_entry_seqnum);
	DEBUG("Tail Entry Sequence Number:    ", to_hex_string(m_tail_entry_seqnum));

	m_head_entry_seqnum			= get_uint64(hdr->head_entry_seqnum);
	DEBUG("Head Entry Sequence Number:    ", to_hex_string(m_head_entry_seqnum));

	m_entry_array_offset		= get_uint64(hdr->entry_array_offset);
	DEBUG("Entry Array Offset:            ", to_hex_string(m_entry_array_offset));

	m_head_entry_realtime		= get_uint64(hdr->head_entry_realtime);
	DEBUG("Head Entry Real Time:          ", to_dec_string(m_head_entry_realtime));

	m_tail_entry_realtime		= get_uint64(hdr->tail_entry_realtime);
	DEBUG("Tail Entry Real Time:          ", to_dec_string(m_tail_entry_realtime));

	m_tail_entry_monotonic		= get_uint64(hdr->tail_entry_monotonic);
	DEBUG("Tail Entry Monotonic Time:     ", to_dec_string(m_tail_entry_monotonic));

	m_n_data					= get_uint64(hdr->n_data);
	DEBUG("Number of Data:                ", to_dec_string(m_n_data));

	m_n_fields					= get_uint64(hdr->n_fields);
	DEBUG("Number of Fields:              ", to_dec_string(m_n_fields));

	m_n_tags					= get_uint64(hdr->n_tags);
	DEBUG("Number of Tags:                ", to_dec_string(m_n_tags));

	m_n_entry_arrays			= get_uint64(hdr->n_entry_arrays);
	DEBUG("Nunmber of Entry Arrays:       ", to_dec_string(m_n_entry_arrays));

	m_data_hash_chain_depth		= get_uint64(hdr->data_hash_chain_depth);
	DEBUG("Data Hash Chain Depth:         ", to_dec_string(m_data_hash_chain_depth));

	m_field_hash_chain_depth	= get_uint64(hdr->field_hash_chain_depth);
	DEBUG("Field Hash Chain Depth:        ", to_dec_string(m_field_hash_chain_depth));
	DEBUG("===============================================================");

	m_parsed = true;
	verify_offsets();

	if (0 == m_tail_object_offset)
		return;

	verify_file(hdr);

	DEBUG("Parsing Objects...");

	off = m_header_size;

	for (;;) {
		DEBUG("Parsing Object at offset: ", to_dec_string(off));

		move_to_object(object_type_t::OBJECT_UNUSED, off, &obj);

		switch (obj->object.type) {
			case object_type_t::OBJECT_DATA:
			{
				data_obj_t dobj(obj->object.flags, obj->object.size, get_uint64(obj->data.hash), get_uint64(obj->data.n_entries));

				dobj.payload().resize(get_uint64(obj->object.size) - offsetof(data_object_t, payload));
				std::memcpy(dobj.payload().data(), &obj->data.payload[0], get_uint64(obj->object.size) - offsetof(data_object_t, payload));

				DEBUG("Parsed Data Object: ");
				DEBUG("  Hash:                        ", to_hex_string(dobj.hash()));
				DEBUG("  Number of Entries:           ", to_dec_string(dobj.n_entries()));
				DEBUG("  Payload:                     ", std::string((char*)dobj.payload().data(), dobj.payload().size()));
				m_data_objects.push_back(dobj);	
			}
			break;

			case object_type_t::OBJECT_FIELD:
			{
				field_obj_t fobj(obj->object.flags, obj->object.size, get_uint64(obj->data.hash));
				
				fobj.payload().resize(get_uint64(obj->object.size) - offsetof(field_object_t, payload));
				std::memcpy(fobj.payload().data(), &obj->field.payload[0], get_uint64(obj->object.size) - offsetof(field_object_t, payload));
		
				DEBUG("Parsed Field Object: ");
				DEBUG("  Hash:                        ", to_hex_string(fobj.hash())); 
				DEBUG("  Payload:                     ", std::string((char*)fobj.payload().data(), fobj.payload().size()));
				m_field_objects.push_back(fobj);
			}
			break;
			
			case object_type_t::OBJECT_ENTRY:
			{
				entry_obj_t eobj(obj->object.flags, obj->object.size, get_uint64(obj->entry.seqnum), 
								get_uint64(obj->entry.realtime), get_uint64(obj->entry.monotonic), 
								{get_uint64(obj->entry.boot_id[0]), get_uint64(obj->entry.boot_id[1])}, get_uint64(obj->entry.xor_hash));

				DEBUG("Parsed Entry Object: ");
				DEBUG("  Sequence Number:             ", to_hex_string(eobj.seqnum()));
				DEBUG("  Realtime Timestamp:          ", to_dec_string(eobj.realtime()));
				DEBUG("  Monotonic Timestamp:         ", to_dec_string(eobj.monotonic()));
				DEBUG("  Boot ID:                     ", to_hex_string(eobj.boot_id()[0]), to_hex_string(eobj.boot_id()[1]));
				DEBUG("  XOR Hash:                    ", to_hex_string(eobj.xor_hash()));

				for (std::size_t idx = 0; idx < file_entry_n_items(obj); idx++) {
					object_t* tmp(nullptr);

					move_to_object(object_type_t::OBJECT_UNUSED, get_uint64(obj->entry.items[idx].object_offset), &tmp);
					eobj.items().push_back(to_cpp_object(tmp));
				}

				m_entry_objects.push_back(eobj);
			}
			break;

			case object_type_t::OBJECT_TAG:
			{
				tag_obj_t tobj(obj->object.flags, obj->object.size, get_uint64(obj->tag.seqnum), get_uint64(obj->tag.epoch));

				std::memcpy(&tobj.tag()[0], &obj->tag.tag[0], tobj.tag().size());
				DEBUG("Parsed Tag Object: ");
				DEBUG("  Sequence Number:             ", to_hex_string(tobj.seqnum()));
				DEBUG("  Epoch Timestamp:             ", to_dec_string(tobj.epoch()));
				DEBUG("  Tag:                         ", std::string((char*)&tobj.tag()[0], tobj.tag().size()));
				m_tag_objects.push_back(tobj);
			}
			break;

			default:
			break;
		}

		if (off == m_tail_object_offset)
			break;

		off += ALIGN64(get_uint64(obj->object.size));
	}

	DEBUG("Parsed ", to_dec_string(m_data_objects.size()), " Data Objects");
	DEBUG("Parsed ", to_dec_string(m_field_objects.size()), " Field Objects");
	DEBUG("Parsed ", to_dec_string(m_entry_objects.size()), " Entry Objects");
	DEBUG("Parsed ", to_dec_string(m_tag_objects.size()), " Tag Objects");
	
	return;			
}

const bool 
input_journal_t::has_field(const std::string& field_name) const
{
	const std::size_t 	mfval(m_field_objects.size());

	if (0 != field_name.length()) {
		for (std::size_t idx = 0; idx < mfval; idx++) {
			if (! ::strcasecmp(field_name.c_str(), m_field_objects[idx].to_string().c_str())) 
				return true;
		}	
	}

	return false;
}

const bool
input_journal_t::has_field_value(const std::string& field_value) const
{
	const std::size_t 	mdval(m_data_objects.size());

	if (0 != field_value.length()) {
		for (std::size_t idx = 0; idx < mdval; idx++) {
			std::string field_data(m_data_objects[idx].to_string());

			do {
				if (field_data.length() < field_value.length())
					break;

				if (! ::strncasecmp(field_value.c_str(), field_data.c_str(), field_value.length()))
					return true;
			
				field_data = field_data.substr(1);
			} while (field_data.length() >= field_value.length() && 0 != field_data.length());
		}
	}
			
	return false;
}

const uint64_t
input_journal_t::get_field_hash(const std::string& field_name) const
{
	const std::size_t	mfval(m_field_objects.size());

	for (std::size_t idx = 0; idx < mfval; idx++) {
		if (! ::strcasecmp(field_name.c_str(), m_field_objects[idx].to_string().c_str()))
			return m_field_objects[idx].hash();
	}

	throw journal_invalid_logic_error_t("input_journal_t::get_field_hash(): called on nonexistent field");
	return uint64_t(0);
}

const uint64_t 
input_journal_t::get_field_value_hash(const std::string& field_value) const
{
	const std::size_t   	mdval(m_data_objects.size());

	for (std::size_t idx = 0; idx < mdval; idx++) {
		std::string field_data(m_data_objects[idx].to_string());

		do {
			if (! ::strncasecmp(field_value.c_str(), field_data.c_str(), field_value.length()))
				return m_data_objects[idx].hash();

			field_data = field_data.substr(1);

		} while (field_data.length() >= field_data.length() && 0 != field_data.length());

	}

	throw journal_invalid_logic_error_t("input_journal_t::get_field_value_hash(): no such field value identified");
	return uint64_t(0);
}

