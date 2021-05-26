#include "siphash.hpp"

	// translated into C++ from:
	/*
	 *	SipHash reference C implementation
	 *
	 *	Written in 2012 by
	 *	Jean-Philippe Aumasson <jeanphilippe.aumasson@gmail.com>
	 *	Daniel J. Bernstein <djb@cr.yp.to>
	 *
	 *	To the extent possible under law, the author(s) have dedicated all copyright
	 *	and related and neighboring rights to this software to the public domain
	 *	worldwide. This software is distributed without any warranty.
	 *
	 *	You should have received a copy of the CC0 Public Domain Dedication along with
	 *	this software. If not, see <https://creativecommons.org/publicdomain/zero/1.0/>.
	 *
	 *	(Minimal changes made by Lennart Poettering, to make clean for inclusion in systemd)
	 *	(Refactored by Tom Gundersen to split up in several functions and follow systemd
	 *	coding style)
	 */

siphash_t::siphash_t(void)
	: m_v0(0), m_v1(0), m_v2(0), m_v3(0), m_padding(0), m_inlen(0)
{
	return;
}

siphash_t::~siphash_t(void)
{
	m_v0 		= 0;
	m_v1 		= 0;
	m_v2 		= 0;
	m_v3 		= 0;
	m_padding 	= 0;
	m_inlen		= 0;
	return;
}

void 
siphash_t::init(const sbarray_t& k)
{
	uint64_t k0(get_uint64(*reinterpret_cast< const uint64_t* >(&k[0])));
	uint64_t k1(get_uint64(*reinterpret_cast< const uint64_t* >(&k[sizeof(uint64_t)])));

	/* "somepseudorandomlygeneratedbytes" */
	m_v0 		= 0x736f6d6570736575ULL ^ k0;
	m_v1 		= 0x646f72616e646f6dULL ^ k1;
	m_v2 		= 0x6c7967656e657261ULL ^ k0;
	m_v3 		= 0x7465646279746573ULL ^ k1;
	m_padding 	= 0;
	m_inlen 	= 0;

	return;
}

void 
siphash_t::compress(const void* _in, const std::size_t inlen)
{
	const uint8_t* 	in(reinterpret_cast< const uint8_t* >(_in));
	const uint8_t* 	end(in+inlen);
	std::size_t		left(m_inlen & 7);
	uint64_t		m(0);

	if (nullptr == _in)
		throw std::invalid_argument("siphash_t::compress(): invalid parameter (nullptr)");

	if (0 == inlen)
		return;

	/* Update total length */
	m_inlen += inlen;

	/* If padding exists, fill it out */
	if (0 < left) {
		for (; in < end && 8 > left; in++, left++) 
			m_padding |= (static_cast< uint64_t >(*in)) << (left * 8);

		if (in == end && 8 > left)
			/* We did not have enough input to fill out the padding completely */
			return;

		
		DEBUG("SIPHash compressing: v0: ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v0 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v0)));
		DEBUG("SIPHash compressing: v1: ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v1 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v1))); 
		DEBUG("SIPHash compressing: v2: ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v2 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v2)));
		DEBUG("SIPHash compressing: v3: ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v3 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v3)));
		DEBUG("SIPHash compressing: padding: ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_padding >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_padding)));

		m_v3	 	^= m_padding;
		round();
		round();
		m_v0 		^= m_padding;
		m_padding 	= 0;

		end -= (m_inlen % sizeof(uint64_t));
	}

	for (; in < end; in += 8) {
		m = get_uint64(*reinterpret_cast< const uint64_t * >(in));

		DEBUG("SIPHash compressing: v0 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v0 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v0)));
		DEBUG("SIPHash compressing: v1 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v1 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v1)));
		DEBUG("SIPHash compressing: v2 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v2 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v2)));
		DEBUG("SIPHash compressing: v3 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v3 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v3)));
		DEBUG("SIPHash compressing: padding: ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_padding >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_padding)));
	
		m_v3 ^= m;
		round();
		round();
		m_v0 ^= m;
	}

	left = m_inlen & 7;

	switch (left) {
		case 7:
			m_padding |= (static_cast< uint64_t >(in[6])) << 48;
		case 6:
			m_padding |= (static_cast< uint64_t >(in[5])) << 40;
		case 5:
			m_padding |= (static_cast< uint64_t >(in[4])) << 32;
		case 4:
			m_padding |= (static_cast< uint64_t >(in[3])) << 24;
		case 3:
			m_padding |= (static_cast< uint64_t >(in[2])) << 16;
		case 2:
			m_padding |= (static_cast< uint64_t >(in[1])) << 8;
		case 1:
			m_padding |= (static_cast< uint64_t >(in[0]));
		default:
			break;
	}

	return;
}

uint64_t 
siphash_t::finalise(void)
{
	const uint64_t b(m_padding | ((static_cast< uint64_t >(m_inlen)) << 56));

	DEBUG("SIPHash finalising: v0 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v0 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v0)));
	DEBUG("SIPHash finalising: v1 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v1 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v1)));
	DEBUG("SIPHash finalising: v2 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v2 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v2)));
	DEBUG("SIPHash finalising: v3 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v3 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v3)));
	DEBUG("SIPHash finalising: padding: ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_padding >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_padding)));

	m_v3 ^= b;
	round();
	round();
	m_v0 ^= b;

	DEBUG("SIPHash finalising: v0 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v0 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v0)));
	DEBUG("SIPHash finalising: v1 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v1 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v1)));
	DEBUG("SIPHash finalising: v2 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v2 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v2)));
	DEBUG("SIPHash finalising: v3 ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_v3 >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_v3)));
	DEBUG("SIPHash finalising: padding: ", to_dec_string(m_inlen), " ", to_hex_string(static_cast< uint32_t >(m_padding >> 32)), " ", to_hex_string(static_cast< uint32_t >(m_padding)));

	m_v2 ^= 0xFF;
	round();
	round();
	round();
	round();

	return uint64_t(m_v0 ^ m_v1 ^ m_v2 ^ m_v3);
}

uint64_t 
siphash_t::hash(const void* in, const std::size_t inlen, const sbarray_t& k)
{
	init(k);
	compress(in, inlen);
	return finalise();
}


