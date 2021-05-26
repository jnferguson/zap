#pragma once
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <cstring>
#include <vector>

#include "global.hpp"
#include "exception.hpp"
#include "journal-def.hpp"
#include "endian.hpp"
#include "intstring.hpp"
#include "log.hpp"

typedef std::array< uint64_t, 2 > uint128_vec_t;
typedef std::array< uint64_t, 8 > object_size_tbl_t;
typedef std::array< uint8_t, 256/8 > tagvec_t;

static inline bool
VALID_REALTIME(uint64_t u)
{
	return u > 0 && u < (1ULL << 55);
}

static inline bool
VALID_MONOTONIC(uint64_t u)
{
	return u < (1ULL << 55);
}

static inline bool
VALID_EPOCH(uint64_t u)
{
	return u < (1ULL << 55);
}

class obj_hdr_t
{
	private:
	protected:
		uint8_t 	m_type;
		uint8_t 	m_flags;
		uint64_t 	m_obj_size;
	
	public:
		obj_hdr_t(const uint8_t t = OBJECT_UNUSED, const uint8_t f = 0, const uint64_t s = 0);
		virtual ~obj_hdr_t(void);

		const uint8_t type(void) const;
		void type(const uint8_t);
	
		const uint8_t flags(void) const;
		void flags(const uint8_t);

		const uint64_t object_size(void) const;
		void object_size(const uint64_t);

		static const std::string type_string(const uint8_t);	
		static const std::string flags_string(const uint8_t);

		const bool compressed_xz(void) const;
		void compressed_xz(const bool v = true);

		const bool compressed_lz4(void) const;
		void compressed_lz4(const bool v = true);

		const bool compressed_zstd(void) const;
		void compressed_zstd(const bool v = true);

		virtual const std::string to_string(void) const = 0;
};

class data_obj_t : public obj_hdr_t
{
	private:
	protected:
		uint64_t		 		m_hash;
		uint64_t 				m_nentries;
		std::vector< uint8_t > 	m_payload;

	public:
		data_obj_t(const uint8_t f = 0, const uint64_t s = 0, const uint64_t& h = 0, const uint64_t& n = 0);

		virtual ~data_obj_t(void);

		virtual const uint64_t hash(void) const;
		virtual void hash(const uint64_t& h);

		virtual const uint64_t n_entries(void) const;
		virtual void n_entries(const uint64_t& n);

		virtual const std::size_t size(void) const;
		virtual std::vector< uint8_t >& payload(void);
		virtual const std::vector< uint8_t >& payload(void) const;
		virtual const std::string to_string(void) const final;
};

class field_obj_t : public obj_hdr_t
{
	private:
	protected:
		uint64_t 				m_hash;
		std::vector< uint8_t >	m_payload;

	public:
		field_obj_t(const uint8_t f = 0, const uint64_t s = 0, const uint64_t h = 0);
		
		virtual ~field_obj_t(void);

		virtual const uint64_t hash(void) const;
		virtual void hash(const uint64_t&);

		virtual const std::size_t size(void) const;
		virtual std::vector< uint8_t >& payload(void);
		virtual const std::vector< uint8_t >& payload(void) const;
		virtual const std::string to_string(void) const final;
		
};

class entry_obj_t : public obj_hdr_t
{
	private:
	protected:
		uint64_t 					m_seqnum;
		uint64_t 					m_realtime;
		uint64_t 					m_monotonic;
		uint128_vec_t				m_boot_id;
		uint64_t					m_xor_hash;
		std::vector< obj_hdr_t* > 	m_items;

	public:
		entry_obj_t(const uint8_t f, const uint64_t sz, const uint64_t& s, const uint64_t& r, 
					const uint64_t& m, const uint128_vec_t& b, const uint64_t& x);

		virtual ~entry_obj_t(void);

		virtual const uint64_t seqnum(void) const;
		virtual void seqnum(const uint64_t&);
		
		virtual const uint64_t realtime(void) const;
		virtual void realtime(const uint64_t&);

		virtual const uint64_t monotonic(void) const;
		virtual void monotonic(const uint64_t&);

		virtual const uint128_vec_t& boot_id(void) const;
		virtual void boot_id(const uint128_vec_t&);

		virtual const uint64_t xor_hash(void) const;
		virtual void xor_hash(const uint64_t&);

		virtual const std::size_t size(void) const;
		virtual std::vector< obj_hdr_t* >& items(void);
		virtual const std::vector< obj_hdr_t* >& items(void) const;
		virtual const std::string to_string(void) const final;

		virtual const bool has_item_hash(const uint64_t) const;
		virtual bool operator<(const entry_obj_t& rhs) const;
};

class hash_tbl_obj_t : public obj_hdr_t
{
	private:
	protected:
		std::vector< hash_item_t > m_items;
	public:
		hash_tbl_obj_t(const uint8_t t = 0, const uint8_t f = 0, const uint64_t s = 0);
		virtual ~hash_tbl_obj_t(void);

		virtual const std::size_t size(void) const;
		virtual std::vector< hash_item_t >& items(void);
		virtual const std::vector< hash_item_t >& items(void) const;
		virtual const std::string to_string(void) const final;
};

class data_hash_tbl_obj_t : public hash_tbl_obj_t
{
	private:
	protected:
	public:
		data_hash_tbl_obj_t(const uint8_t f = 0, const uint64_t s = 0);
		virtual ~data_hash_tbl_obj_t(void) = default;
};

class field_hash_tbl_obj_t : public hash_tbl_obj_t
{
	private:
	protected:
	public:
		field_hash_tbl_obj_t(const uint8_t f = 0, const uint64_t s = 0);
		virtual ~field_hash_tbl_obj_t(void) = default;
};

class entry_array_obj_t : public obj_hdr_t
{
	private:
	protected:
		std::vector< entry_obj_t > m_items;

	public:
		entry_array_obj_t(const uint8_t f = 0, const uint64_t s = 0);
		virtual ~entry_array_obj_t(void);

		virtual const std::size_t size(void) const;
		virtual std::vector< entry_obj_t >& items(void);
		virtual const std::vector< entry_obj_t >& items(void) const;
		virtual const std::string to_string(void) const final;
};


class tag_obj_t : public obj_hdr_t
{
	private:
	protected:
		uint64_t m_seqnum;
		uint64_t m_epoch;
		tagvec_t m_tag;

	public:
		tag_obj_t(const uint8_t f = 0, const uint64_t sz = 0, const uint64_t& s = 0, const uint64_t& e = 0);
		virtual ~tag_obj_t(void);

		virtual const uint64_t seqnum(void) const;
		virtual void seqnum(const uint64_t&);
		virtual const uint64_t epoch(void) const;
		virtual void epoch(const uint64_t&);
		virtual tagvec_t& tag(void);
		virtual void tag(const tagvec_t&);
		virtual const std::string to_string(void) const final;
};

