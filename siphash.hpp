#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <array>

#include "exception.hpp"
#include "global.hpp"
#include "endian.hpp"
#include "log.hpp"
#include "intstring.hpp"

typedef uint64_t usec_t;
typedef std::array< uint8_t, 16 > sbarray_t;

class siphash_t
{
	private:
	protected:
		uint64_t					m_v0;
		uint64_t					m_v1;
		uint64_t					m_v2;
		uint64_t					m_v3;
		uint64_t					m_padding;
		std::size_t					m_inlen;

		static inline const uint64_t
		rotate_left(uint64_t x, uint8_t b)
		{
			if (64 >= b)
				throw std::invalid_argument("siphash_t::rotate_left(): invalid bit-length parameter");

			return (x << b) | (x >> (64 - b));
		}

		virtual inline void
		round(void)
		{
        	m_v0 += m_v1;
        	m_v1 = rotate_left(m_v1, 13);
        	m_v1 ^= m_v0;
        	m_v0 = rotate_left(m_v0, 32);
        	m_v2 += m_v3;
        	m_v3 = rotate_left(m_v3, 16);
        	m_v3 ^= m_v2;
        	m_v0 += m_v3;
        	m_v3 = rotate_left(m_v3, 21);
        	m_v3 ^= m_v0;
        	m_v2 += m_v1;
        	m_v1 = rotate_left(m_v1, 17);
        	m_v1 ^= m_v2;
        	m_v2 = rotate_left(m_v2, 32);

			return;
		}

	public:
		siphash_t(void);
		virtual ~siphash_t(void);

		virtual void init(const sbarray_t&);
		virtual void compress(const void*, const std::size_t);
		virtual uint64_t finalise(void);
		virtual uint64_t hash(const void*, const std::size_t, const sbarray_t&);

		virtual inline void
		compress_byte(const uint8_t byte)
		{
			
			compress(&byte, 1);
			return;
		}
	
		virtual inline void
		compress_boolean(const bool in)
		{
			compress(reinterpret_cast< const uint8_t* >(&in), sizeof(in));
			return;
		}

		virtual inline void
		compress_usec_t(usec_t in)
		{
			compress(&in, sizeof(in));
			return;
		}

		virtual inline void
		compress_string(const char* in)
		{
			if (nullptr == in || 0 == ::strlen(in))
				throw std::invalid_argument("siphash24_t::compress_string(): invalid parameter (nullptr/0 length)");

			compress(in, ::strlen(in));
			return;
		}
	

		virtual inline const uint64_t 
		string(const char* s, const sbarray_t& k)
		{
			return hash(s, strlen(s)+1, k);
		}

};
