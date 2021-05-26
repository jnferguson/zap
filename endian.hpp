#pragma once
#include <cstdint>
#include "byteswap.hpp"

#if defined(__BYTE_ORDER__)&&(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    inline const uint16_t
    get_uint16(const uint16_t& val)
    {
        return byteswap(val);
    }

    inline const uint32_t
    get_uint32(const uint32_t& val)
    {
        return byteswap(val);
    }

    inline const uint64_t
    get_uint64(const uint64_t& val)
    {
        return byteswap(val);
    }
#else
    inline const uint16_t
    get_uint16(const uint16_t& val)
    {
        return val;
    }

    inline const uint32_t
    get_uint32(const uint32_t& val)
    {
        return val;
    }

    inline const uint64_t
    get_uint64(const uint64_t& val)
    {
        return val;
    }
#endif
