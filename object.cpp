#include "object.hpp"

obj_hdr_t::obj_hdr_t(uint8_t t, uint8_t f, uint64_t s) 
	: m_type(t), m_flags(f), m_obj_size(s) 
{
	if (object_type_t::OBJECT_TYPE_MAX <= t)
		throw std::invalid_argument("obj_hdr_t::obj_hdr_t(): invalid object type specified");
	
	return;
}

obj_hdr_t::~obj_hdr_t(void) 
{ 
	m_type 		= object_type_t::OBJECT_UNUSED; 
	m_flags 	= 0; 
	m_obj_size	= 0;
	return; 
}

const uint8_t 
obj_hdr_t::type(void) const 
{ 
	return m_type; 
}

void 
obj_hdr_t::type(const uint8_t t) 
{ 
	m_type = t; 
	return; 
}

const uint64_t
obj_hdr_t::object_size(void) const
{
	return m_obj_size;
}

void
obj_hdr_t::object_size(const uint64_t s)
{
	m_obj_size = s;
	return;
}

const std::string
obj_hdr_t::type_string(const uint8_t t)
{
	std::string ret("");

    switch (t) {
		case object_type_t::OBJECT_UNUSED:
			ret = "OBJECT_UNUSED";
		break;

        case object_type_t::OBJECT_DATA:
			ret = "OBJECT_DATA";
        break;

        case object_type_t::OBJECT_FIELD:
			ret = "OBJECT_FIELD";
        break;

        case object_type_t::OBJECT_ENTRY:
			ret = "OBJECT_ENTRY";
        break;

        case OBJECT_DATA_HASH_TABLE:
			ret = "OBJECT_DATA_HASH_TABLE";
        break;

        case OBJECT_FIELD_HASH_TABLE:
			ret = "OBJECT_FIELD_HASH_TABLE";
        break;

        case OBJECT_ENTRY_ARRAY:
			ret = "OBJECT_ENTRY_ARRAY";
        break;

        case OBJECT_TAG:
			ret = "OBJECT_TAG";
        break;

        default:
            throw std::runtime_error("obj_hdr_t::type_string(): invalid object type encountered");
        break;
    }

	return ret;
}

const std::string 
obj_hdr_t::flags_string(const uint8_t f)
{
	std::string ret("");

	if ((f & object_compression_type_t::OBJECT_COMPRESSED_XZ) == object_compression_type_t::OBJECT_COMPRESSED_XZ)
		ret += "XZ ";
	if ((f & object_compression_type_t::OBJECT_COMPRESSED_LZ4) == object_compression_type_t::OBJECT_COMPRESSED_LZ4)
		ret += "LZ4 ";
	if ((f & object_compression_type_t::OBJECT_COMPRESSED_ZSTD) == object_compression_type_t::OBJECT_COMPRESSED_ZSTD)
		ret += "ZSTD ";
	
	return ret;
}

const uint8_t 
obj_hdr_t::flags(void) const 
{ 
	return m_flags; 
}

void 
obj_hdr_t::flags(const uint8_t f) 
{ 
	m_flags = f; 
	return; 
}

const bool 
obj_hdr_t::compressed_xz(void) const
{
	return true == ((m_flags & object_compression_type_t::OBJECT_COMPRESSED_XZ) == object_compression_type_t::OBJECT_COMPRESSED_XZ);
}

void 
obj_hdr_t::compressed_xz(const bool v)
{
	if (true == v)
		m_flags |= object_compression_type_t::OBJECT_COMPRESSED_XZ;
	else
		m_flags &= ~(object_compression_type_t::OBJECT_COMPRESSED_XZ);

	return;
}

const bool 
obj_hdr_t::compressed_lz4(void) const
{
	return true == ((m_flags & object_compression_type_t::OBJECT_COMPRESSED_LZ4) == object_compression_type_t::OBJECT_COMPRESSED_LZ4);
}

void 
obj_hdr_t::compressed_lz4(const bool v)
{
	if (true == v)
		m_flags |= object_compression_type_t::OBJECT_COMPRESSED_LZ4;
	else
		m_flags &= ~(object_compression_type_t::OBJECT_COMPRESSED_LZ4);

	return;
}

const bool 
obj_hdr_t::compressed_zstd(void) const
{
	return true == ((m_flags & object_compression_type_t::OBJECT_COMPRESSED_ZSTD) == object_compression_type_t::OBJECT_COMPRESSED_ZSTD);
}

void 
obj_hdr_t::compressed_zstd(const bool v)
{
	if (true == v)
		m_flags |= object_compression_type_t::OBJECT_COMPRESSED_ZSTD;
	else
		m_flags &= ~(object_compression_type_t::OBJECT_COMPRESSED_ZSTD);

	return;
}

data_obj_t::data_obj_t(const uint8_t f, const uint64_t s, const uint64_t& h, const uint64_t& n)
	: obj_hdr_t(object_type_t::OBJECT_DATA, f, s), m_hash(h), m_nentries(n)
{
	return;
}

data_obj_t::~data_obj_t(void)
{
	m_hash 		= 0;
	m_nentries 	= 0;
	m_payload.clear();
	return;
}

const uint64_t 
data_obj_t::hash(void) const 
{ 
	return m_hash; 
}

void 
data_obj_t::hash(const uint64_t& h) 
{ 
	m_hash = h; 
	return; 
}

const uint64_t 
data_obj_t::n_entries(void) const 
{ 
	return m_nentries; 
}

void 
data_obj_t::n_entries(const uint64_t& n) 
{ 
	m_nentries = n; 
	return; 
}

const std::size_t 
data_obj_t::size(void) const 
{ 
	return m_payload.size(); 
}

std::vector< uint8_t >& 
data_obj_t::payload(void) 
{ 
	return m_payload; 
}

const std::vector< uint8_t >& 
data_obj_t::payload(void) const 
{ 
	return m_payload; 
}

const std::string 
data_obj_t::to_string(void) const
{
	std::string ret(m_payload.begin(), m_payload.end());
	return ret;
}

field_obj_t::field_obj_t(const uint8_t f, const uint64_t s, const uint64_t h) 
	: obj_hdr_t(object_type_t::OBJECT_FIELD, f, s), m_hash(h) 
{
	return;
}

field_obj_t::~field_obj_t(void) 
{ 
	m_hash = 0; 
	m_payload.clear(); 
	return; 
}

const uint64_t 
field_obj_t::hash(void) const 
{ 
	return m_hash; 
}

void 
field_obj_t::hash(const uint64_t& h) 
{ 
	m_hash = h; 
	return; 
}

const std::size_t 
field_obj_t::size(void) const 
{ 
	return m_payload.size(); 
}

std::vector< uint8_t >& 
field_obj_t::payload(void) 
{ 
	return m_payload; 
}

const std::vector< uint8_t >& 
field_obj_t::payload(void) const 
{ 
	return m_payload; 
}

const std::string 
field_obj_t::to_string(void) const
{
	std::string ret(m_payload.begin(), m_payload.end());
	return ret;
}

entry_obj_t::entry_obj_t(const uint8_t f, const uint64_t sz, const uint64_t& s, const uint64_t& r, const uint64_t& m, const uint128_vec_t& b, const uint64_t& x)
	: obj_hdr_t(object_type_t::OBJECT_ENTRY, f, sz), m_seqnum(s), m_realtime(r), m_monotonic(m), m_boot_id(b), m_xor_hash(x)
{
	return;
}

entry_obj_t::~entry_obj_t(void)
{
	m_seqnum 		= 0;
	m_realtime 		= 0;
	m_monotonic 	= 0;
	m_boot_id[0] 	= 0;
	m_boot_id[1] 	= 0;
	m_xor_hash 		= 0;
	m_items.clear();
	return;
}

const uint64_t 
entry_obj_t::seqnum(void) const 
{ 
	return m_seqnum; 
}

void 
entry_obj_t::seqnum(const uint64_t& s) 
{ 
	m_seqnum = s; 
	return; 
}

const uint64_t 
entry_obj_t::realtime(void) const 
{ 
	return m_realtime; 
}

void 
entry_obj_t::realtime(const uint64_t& r) 
{ 
	m_realtime = r; 
	return; 
}

const uint64_t 
entry_obj_t::monotonic(void) const 
{ 
	return m_monotonic; 
}

void 
entry_obj_t::monotonic(const uint64_t& m) 
{ 
	m_monotonic = m; 
	return; 
}

const uint128_vec_t& 
entry_obj_t::boot_id(void) const 
{ 
	return m_boot_id; 
}

void 
entry_obj_t::boot_id(const uint128_vec_t& b) 
{ 
	m_boot_id[0] = b[0]; 
	m_boot_id[1] = b[1]; 
	return; 
}

const uint64_t 
entry_obj_t::xor_hash(void) const 
{ 
	return m_xor_hash; 
}

void 
entry_obj_t::xor_hash(const uint64_t& x) 
{ 
	m_xor_hash = x; 
	return; 
}

const std::size_t 
entry_obj_t::size(void) const 
{ 
	return m_items.size(); 
}

std::vector< obj_hdr_t* >& 
entry_obj_t::items(void) 
{ 
	return m_items; 
}

const std::vector< obj_hdr_t* >& 
entry_obj_t::items(void) const 
{ 
	return m_items; 
}

const std::string 
entry_obj_t::to_string(void) const
{
	std::string ret("RT: ");
	ret += to_dec_string(realtime());
	ret += " MT: ";
	ret += to_dec_string(monotonic());
	ret += " BID: ";
	ret += to_hex_string(boot_id()[0]), to_hex_string(boot_id()[1]);

	for (std::size_t idx = 0; idx < m_items.size(); idx++) {
		ret += m_items[idx]->to_string();
		ret += " ";
	}

	return ret;
}

const bool 
entry_obj_t::has_item_hash(const uint64_t hash) const
{
	const std::size_t 	imax(m_items.size());

	for (std::size_t idx = 0; idx < imax; idx++) {
		obj_hdr_t* obj(m_items[idx]);

		if (nullptr == obj) 
			throw std::runtime_error("entry_obj_t::has_item_hash(): entry object with null item encountered");

		switch (obj->type()) {
			case object_type_t::OBJECT_DATA:
			{
				data_obj_t* dobj(dynamic_cast< data_obj_t* >(obj));

				if (nullptr == dobj)
					throw std::runtime_error("entry_obj_t::has_item_hash(): dynamic_cast failure when item->type matches");

				if (hash == dobj->hash())
					return true;

			}
			break;

			case object_type_t::OBJECT_FIELD:
			{
				field_obj_t* fobj(dynamic_cast< field_obj_t* >(obj));

				if (nullptr == fobj)
					throw std::runtime_error("entry_obj_t::has_item_hash(): dynamic_cast failure when item->type matches");

				if (hash == fobj->hash())
					return true;
			}
			break;

			default:
				throw std::runtime_error("entry_obj_t::has_item_hash(): invalid item->type encountered");
			break;
		}
		
	}

	return false;	
}

bool 
entry_obj_t::operator<(const entry_obj_t& rhs) const
{
	return m_seqnum < rhs.m_seqnum;
}

hash_tbl_obj_t::hash_tbl_obj_t(const uint8_t t, const uint8_t f, const uint64_t s) 
	: obj_hdr_t(t, f, s) 
{
	return;
}

hash_tbl_obj_t::~hash_tbl_obj_t(void) 
{ 
	m_items.clear(); 
	return; 
}

const std::size_t 
hash_tbl_obj_t::size(void) const 
{ 
	return m_items.size(); 
}

std::vector< hash_item_t >& 
hash_tbl_obj_t::items(void) 
{ 
	return m_items; 
}

const std::vector< hash_item_t >& 
hash_tbl_obj_t::items(void) const 
{ 
	return m_items; 
}

const std::string 
hash_tbl_obj_t::to_string(void) const
{
	std::string ret("");

	throw std::runtime_error("hash_tbl_obj_t::to_string(): unimplemented method called");
	return ret;
}	

data_hash_tbl_obj_t::data_hash_tbl_obj_t(const uint8_t f, const uint64_t s)
	: hash_tbl_obj_t(object_type_t::OBJECT_DATA_HASH_TABLE, f, s)
{
	return;
}

field_hash_tbl_obj_t::field_hash_tbl_obj_t(const uint8_t f, const uint64_t s)
	: hash_tbl_obj_t(object_type_t::OBJECT_FIELD_HASH_TABLE, f, s)
{
	return;
}

entry_array_obj_t::entry_array_obj_t(const uint8_t f, const uint64_t s) 
	: obj_hdr_t(object_type_t::OBJECT_ENTRY_ARRAY, f, s) 
{
	return;
}

entry_array_obj_t::~entry_array_obj_t(void) 
{ 
	m_items.clear(); 
	return; 
}

const std::size_t 
entry_array_obj_t::size(void) const 
{ 
	return m_items.size(); 
}

std::vector< entry_obj_t >& 
entry_array_obj_t::items(void) 
{ 
	return m_items; 
}

const std::vector< entry_obj_t >& 
entry_array_obj_t::items(void) const 
{ 
	return m_items; 
}

const std::string
entry_array_obj_t::to_string(void) const
{
	std::string ret("");

	throw std::runtime_error("entry_array_obj_t::to_string(): unimplemented method encountered");
	return ret;
}	

tag_obj_t::tag_obj_t(const uint8_t f, const uint64_t sz, const uint64_t& s, const uint64_t& e) 
	: obj_hdr_t(object_type_t::OBJECT_TAG, f, sz) 
{
	return;
}

tag_obj_t::~tag_obj_t(void) 
{ 
	m_seqnum 	= 0; 
	m_epoch 	= 0; 
	std::memset(&m_tag[0], 0, m_tag.size() * sizeof(uint8_t)); 
	return; 
}

const uint64_t 
tag_obj_t::seqnum(void) const 
{ 
	return m_seqnum; 
}

void 
tag_obj_t::seqnum(const uint64_t& s) 
{ 
	m_seqnum = s; 
	return; 
}

const uint64_t 
tag_obj_t::epoch(void) const 
{ 
	return m_epoch; 
}

void 
tag_obj_t::epoch(const uint64_t& e) 
{ 
	m_epoch = e; 
	return; 
}

tagvec_t& 
tag_obj_t::tag(void) 
{ 
	return m_tag; 
}

void 
tag_obj_t::tag(const tagvec_t& t) 
{ 
	std::memcpy(&m_tag[0], &t[0], m_tag.size() * sizeof(uint8_t)); 
	return; 
}

const std::string 
tag_obj_t::to_string(void) const
{
	std::string ret("SN: ");

	ret += to_hex_string(seqnum());
	ret += " ET: ";
	ret += to_dec_string(epoch());
	ret += " TAG: ";
	ret += std::string((const char*)&m_tag[0], m_tag.size() * sizeof(uint8_t));
	return ret;
}

