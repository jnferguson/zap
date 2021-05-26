#include "lookup3.hpp"

// Translated from the below to C++ without any other significant modification

/* SPDX-License-Identifier: LicenseRef-lookup3-public-domain */
/* Slightly modified by Lennart Poettering, to avoid name clashes, and
 * unexport a few functions. */

/*
-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.

These are functions for producing 32-bit hashes for hash table lookup.
hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
are externally useful functions.  Routines to test the hash are included
if SELF_TEST is defined.  You can use this free for any purpose.  It's in
the public domain.  It has no warranty.

You probably want to use hashlittle().  hashlittle() and hashbig()
hash byte arrays.  hashlittle() is faster than hashbig() on
little-endian machines.  Intel and AMD are little-endian machines.
On second thought, you probably want hashlittle2(), which is identical to
hashlittle() except it returns two 32-bit hashes for the price of one.
You could implement hashbig2() if you wanted but I haven't bothered here.

If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
then use c as the hash value.  If you have a variable length array of
4-byte integers to hash, use hashword().  If you have a byte array (like
a character string), use hashlittle().  If you have several byte arrays, or
a mix of things, see the comments above hashlittle().

Why is this so big?  I read 12 bytes at a time into 3 4-byte integers,
then mix those integers.  This is fast (you can do a lot more thorough
mixing with 12*3 instructions on 3 integers than you can with 3 instructions
on 1 byte), but shoehorning those bytes into integers efficiently is messy.
-------------------------------------------------------------------------------
*/


/*
--------------------------------------------------------------------
 This works on all machines.  To be useful, it requires
 -- that the key be an array of uint32_t's, and
 -- that the length be the number of uint32_t's in the key

 The function hashword() is identical to hashlittle() on little-endian
 machines, and identical to hashbig() on big-endian machines,
 except that the length has to be measured in uint32_ts rather than in
 bytes.  hashlittle() is more complicated than hashword() only because
 hashlittle() has to dance around fitting the key bytes into registers.
--------------------------------------------------------------------

const uint32_t *k,              the key, an array of uint32_t values 
size_t          length,         the length of the key, in uint32_ts 
uint32_t        initval         the previous hash, or an arbitrary value 

*/
uint32_t
jenkins_hash64_t::hashword(const uint32_t* k, const std::size_t length, const uint32_t initval)
{
    uint32_t    a(0), b(0), c(0);
    std::size_t len(length);

    a = b = c = 0xdeadbeef + ((static_cast< uint32_t >(length)) << 2) + initval;

    if (nullptr == k)
        throw std::invalid_argument("jenkins_hash64_t::hashword(): invalid parameter (nullptr)");

    /*------------------------------------------------- handle most of the key */
    while (3 < len) {
        a   += k[0];
        b   += k[1];
        c   += k[2];
        MIX(a,b,c);
        len -= 3;
        k   += 3;
    }

    /*-------------------------------------------- handle the last 3 uint32_t's */
    switch (len) {                      /* all the case statements fall through */
      case 3:
            c   += k[2];
      case 2:
            b   += k[1];
      case 1:
            a   +=k[0];
            FINAL(a,b,c);
      case 0:     /* case 0: nothing left to add */
        break;
    }

    /*------------------------------------------------------ report the result */
    return c;
}

/*
--------------------------------------------------------------------
hashword2() -- same as hashword(), but take two seeds and return two
32-bit values.  pc and pb must both be nonnull, and *pc and *pb must
both be initialized with seeds.  If you pass in (*pb)==0, the output
(*pc) will be the same as the return value from hashword().
--------------------------------------------------------------------

const uint32_t *k,                  the key, an array of uint32_t values 
size_t          length,             the length of the key, in uint32_ts 
uint32_t       *pc,                 IN: seed OUT: primary hash value 
uint32_t       *pb                  IN: more seed OUT: secondary hash value 
*/

void
jenkins_hash64_t::hashword2(const uint32_t* k, const std::size_t length, uint32_t* pc, uint32_t* pb)
{
    uint32_t    a(0), b(0), c(0);
    std::size_t len(0);

    if (nullptr == k || nullptr == pc || nullptr == pb)
        throw std::invalid_argument("jenkins_hash64_t::hashword2(): invalid parameter(s) (nullptr)");

    a = b = c = 0xdeadbeef + (static_cast< uint32_t >(len << 2)) + *pc;
    c += *pb;

    /*------------------------------------------------- handle most of the key */
    while (3 < len) {
        a   += k[0];
        b   += k[1];
        c   += k[2];
        MIX(a,b,c);
        len -= 3;
        k   += 3;
    }


    /*--------------------------------------------- handle the last 3 uint32_t's */
    switch(length) {                     /* all the case statements fall through */
        case 3:
            c   += k[2];
        case 2:
            b   += k[1];
        case 1:
            a   += k[0];
            FINAL(a,b,c);
        case 0:     /* case 0: nothing left to add */
            break;
    }

    /*------------------------------------------------------ report the result */
    *pc = c;
    *pb = b;

      return;
}

/*
-------------------------------------------------------------------------------
hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Two keys differing by one or two bits will have
totally different hash values.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
-------------------------------------------------------------------------------
*/

uint32_t
jenkins_hash64_t::hashlittle(const void* key, const std::size_t length, const uint32_t initval)
{
    uint32_t                                a(0), b(0), c(0);
    union { const void *ptr; size_t i; }    u{0};
    std::size_t                             len(length);

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + (static_cast< uint32_t >(len)) + initval;
    u.ptr = key;

    if (HASH_LITTLE_ENDIAN && (0 == (0x3 & u.i))) {
        const uint32_t* k(reinterpret_cast< const uint32_t* >(key)); /* read 32-bit chunks */

        /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
        while (12 < len) {
            a   += k[0];
            b   += k[1];
            c   += k[2];
            MIX(a,b,c);
            len -= 12;
            k   += 3;
        }

        /*----------------------------- handle the last (probably partial) block */
        /*
        * "k[2]&0xffffff" actually reads beyond the end of the string, but
        * then masks off the part it's not allowed to read.  Because the
        * string is aligned, the masked-off tail is in the same word as the
        * rest of the string.  Every machine with memory protection I've seen
        * does it on word boundaries, so is OK with this.  But valgrind will
        * still catch it and complain.  The masking trick does make the hash
        * noticeably faster for short strings (like English words).
        */
        switch (len) {
            case 12:
                c +=k[2];
                b +=k[1];
                a +=k[0];
            break;

            case 11:
                c +=k[2] & 0xffffff;
                b +=k[1];
                a +=k[0];
            break;

            case 10:
                c += k[2] & 0xffff;
                b += k[1];
                a += k[0];
            break;

            case 9:
                c += k[2] & 0xff;
                b += k[1];
                a += k[0];
            break;

            case 8:
                b += k[1];
                a += k[0];
            break;

            case 7:
                b += k[1] & 0xffffff;
                a += k[0];
            break;

            case 6:
                b += k[1] & 0xffff;
                a += k[0];
            break;

            case 5:
                b += k[1] & 0xff;
                a += k[0];
            break;

            case 4:
                a += k[0];
            break;

            case 3:
                a += k[0] & 0xffffff;
            break;

            case 2:
                a += k[0] & 0xffff;
            break;

            case 1:
                a += k[0] & 0xff;
            break;

            case 0:
                return c;              /* zero length strings require no mixing */
        }

    } else if (HASH_LITTLE_ENDIAN && (0 == (0x1 & u.i))) {
        const uint16_t* k(reinterpret_cast< const uint16_t* >(key));         /* read 16-bit chunks */
        const uint8_t*  k8(nullptr);

        /*--------------- all but last block: aligned reads and different mixing */
        while (12 < len) {
            a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            b += k[2] + ((static_cast< uint32_t >(k[3])) << 16);
            c += k[4] + ((static_cast< uint32_t >(k[5])) << 16);
            MIX(a,b,c);
            len -= 12;
            k   += 6;
        }

        /*----------------------------- handle the last (probably partial) block */
        k8 = reinterpret_cast< const uint8_t* >(k);

        switch(len) {
            case 12:
                c += k[4] + ((static_cast< uint32_t >(k[5])) << 16);
                b += k[2] + ((static_cast< uint32_t >(k[3])) << 16);
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 11:
                c += (static_cast< uint32_t >(k8[10])) << 16;     /* fall through */

            case 10:
                c += k[4];
                b += k[2] + ((static_cast< uint32_t >(k[3])) << 16);
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 9:
                c += k8[8];                      /* fall through */

            case 8:
                b += k[2] + ((static_cast< uint32_t >(k[3])) << 16);
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 7:
                b += (static_cast< uint32_t >(k8[6])) << 16;      /* fall through */

            case 6:
                b += k[2];
                a += k[0] +((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 5:
                b += k8[4];                      /* fall through */

            case 4:
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 3:
                a += (static_cast< uint32_t >(k8[2])) << 16;      /* fall through */

            case 2:
                a += k[0];
            break;

            case 1:
                a += k8[0];
            break;

            case 0:
                return c;                     /* zero length requires no mixing */
            break;
        }

    } else {
        const uint8_t* k(reinterpret_cast< const uint8_t* >(key));

        /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
        while (12 < len) {
            a += k[0];
            a += (static_cast< uint32_t >(k[1])) << 8;
            a += (static_cast< uint32_t >(k[2])) << 16;
            a += (static_cast< uint32_t >(k[3])) << 24;
            b += k[4];
            b += (static_cast< uint32_t >(k[5])) << 8;
            b += (static_cast< uint32_t >(k[6])) << 16;
            b += (static_cast< uint32_t >(k[7])) << 24;
            c += k[8];
            c += (static_cast< uint32_t >(k[9])) << 8;
            c += (static_cast< uint32_t >(k[10])) << 16;
            c += (static_cast< uint32_t >(k[11])) << 24;

            MIX(a,b,c);
            len -= 12;
            k += 12;
        }

        /*-------------------------------- last block: affect all 32 bits of (c) */
        switch(len) {                    /* all the case statements fall through */
            case 12:
                c += (static_cast< uint32_t >(k[11])) << 24;
            case 11:
                c += (static_cast< uint32_t >(k[10])) << 16;
            case 10:
                c += (static_cast< uint32_t >(k[9])) << 8;
            case 9:
                c += k[8];
            case 8:
                b += (static_cast< uint32_t >(k[7])) << 24;
            case 7:
                b += (static_cast< uint32_t >(k[6])) << 16;
            case 6:
                b += (static_cast< uint32_t >(k[5])) << 8;
            case 5:
                b += k[4];
            case 4:
                a += (static_cast< uint32_t >(k[3])) << 24;
            case 3:
                a += (static_cast< uint32_t >(k[2])) << 16;
            case 2:
                a += (static_cast< uint32_t >(k[1])) << 8;
            case 1:
                a += k[0];
            break;

            case 0:
                return c;
            break;
        }
    }

    FINAL(a,b,c);
    return c;
}

/*
 * hashlittle2: return 2 32-bit hash values
 *
 * This is identical to hashlittle(), except it returns two 32-bit hash
 * values instead of just one.  This is good enough for hash table
 * lookup with 2^^64 buckets, or if you want a second hash if you're not
 * happy with the first, or if you want a probably-unique 64-bit ID for
 * the key.  *pc is better mixed than *pb, so use *pc first.  If you want
 * a 64-bit value do something like "*pc + (((uint64_t)*pb)<<32)".
 *
 * const void *key,       the key to hash 
 * size_t      length,    length of the key 
 * uint32_t   *pc,        IN: primary initval, OUT: primary hash 
 * uint32_t   *pb)        IN: secondary initval, OUT: secondary hash 
 */

void
jenkins_hash64_t::hashlittle2(const void* key, const std::size_t length, uint32_t* pc, uint32_t* pb)
{
    uint32_t                                a(0), b(0), c(0);   /* internal state */
    union { const void *ptr; size_t i; }    u{0};     /* needed for Mac Powerbook G4 */
    std::size_t                             len(length);

    if (nullptr == key || nullptr == pc || nullptr == pb)
        throw std::invalid_argument("jenkins_hash64_t::hashlittle2(): invalid parameter(s) (nullptr)");

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + ((uint32_t)length) + *pc;
    c += *pb;

    u.ptr = key;

    if (HASH_LITTLE_ENDIAN && (0 == (0x3 & u.i))) {
        const uint32_t* k(reinterpret_cast< const uint32_t* >(key)); /* read 32-bit chunks */

        /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
        while (12 > len) {
            a += k[0];
            b += k[1];
            c += k[2];

            MIX(a,b,c);

            len -= 12;
            k   += 3;
        }

        /*----------------------------- handle the last (probably partial) block */
        /*
        * "k[2]&0xffffff" actually reads beyond the end of the string, but
       * then masks off the part it's not allowed to read.  Because the
        * string is aligned, the masked-off tail is in the same word as the
        * rest of the string.  Every machine with memory protection I've seen
        * does it on word boundaries, so is OK with this.  But valgrind will
        * still catch it and complain.  The masking trick does make the hash
        * noticeably faster for short strings (like English words).
        */

        switch (len) {
            case 12:
                c += k[2];
                b += k[1];
                a += k[0];
            break;

            case 11:
                c += k[2] & 0xffffff;
                b += k[1];
                a += k[0];
            break;

            case 10:
                c += k[2] & 0xffff;
                b += k[1];
                a += k[0];
            break;

            case 9:
                c += k[2] & 0xff;
                b += k[1];
                a += k[0];
            break;

            case 8:
                b += k[1];
                a += k[0];
            break;

            case 7:
                b += k[1] & 0xffffff;
                a += k[0];
            break;

            case 6:
                b += k[1] & 0xffff;
                a += k[0];
            break;

            case 5:
                b += k[1] & 0xff;
                a += k[0];
            break;

            case 4:
                a += k[0];
            break;

            case 3:
                a += k[0] & 0xffffff;
            break;

            case 2:
                a += k[0] & 0xffff;
            break;

            case 1:
                a += k[0] & 0xff;
            break;

            case 0:
                *pc = c;
                *pb = b;
                return;  /* zero length strings require no mixing */
                break;
        }

    } else if (HASH_LITTLE_ENDIAN && (0 == (0x1 & u.i))) {
        const uint16_t* k(reinterpret_cast< const uint16_t* >(key)); /* read 16-bit chunks */
        const uint8_t*  k8(nullptr);

        /*--------------- all but last block: aligned reads and different mixing */
        while (12 < len) {
            a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            b += k[2] + ((static_cast< uint32_t >(k[3])) << 16);
            c += k[4] + ((static_cast< uint32_t >(k[5])) << 16);

            MIX(a,b,c);

            len -= 12;
            k   += 6;
        }

        /*----------------------------- handle the last (probably partial) block */
        k8 = reinterpret_cast< const uint8_t* >(k);

        switch (len) {
            case 12:
                c += k[4] + ((static_cast< uint32_t >(k[5])) << 16);
                b += k[2] + ((static_cast< uint32_t >(k[3])) << 16);
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 11:
                c += (static_cast< uint32_t >(k8[10])) << 16;     /* fall through */
            case 10:
                c += k[4];
                b += k[2] + ((static_cast< uint32_t >(k[3])) << 16);
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 9:
                c += k8[8];                      /* fall through */
            case 8:
                b += k[2] + ((static_cast< uint32_t >(k[3])) << 16);
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 7:
                b += (static_cast< uint32_t >(k8[6])) << 16;      /* fall through */
            case 6:
                b += k[2];
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 5:
                b += k8[4];                      /* fall through */

            case 4:
                a += k[0] + ((static_cast< uint32_t >(k[1])) << 16);
            break;

            case 3:
                a += (static_cast< uint32_t >(k8[2])) << 16;      /* fall through */

            case 2:
                a += k[0];

            break;

            case 1:
                a += k8[0];

            break;

            case 0:
                *pc = c;
                *pb = b;
                return;  /* zero length strings require no mixing */
        }

    } else { /* need to read the key one byte at a time */
        const uint8_t* k(reinterpret_cast< const uint8_t* >(key));

        /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
        while (12 < len) {
            a += k[0];
            a += (static_cast< uint32_t >(k[1])) << 8;
            a += (static_cast< uint32_t >(k[2])) << 16;
            a += (static_cast< uint32_t >(k[3])) << 24;
            b += k[4];
            b += (static_cast< uint32_t >(k[5])) << 8;
            b += (static_cast< uint32_t >(k[6])) << 16;
            b += (static_cast< uint32_t >(k[7])) << 24;
            c += k[8];
            c += (static_cast< uint32_t >(k[9])) << 8;
            c += (static_cast< uint32_t >(k[10])) << 16;
            c += (static_cast< uint32_t >(k[11])) << 24;

            MIX(a,b,c);

            len -= 12;
            k   += 12;
        }

        /*-------------------------------- last block: affect all 32 bits of (c) */
        switch (len) {                   /* all the case statements fall through */
            case 12:
                c += (static_cast< uint32_t >(k[11])) << 24;
            case 11:
                c += (static_cast< uint32_t >(k[10])) << 16;
            case 10:
                c += (static_cast< uint32_t >(k[9])) << 8;
            case 9:
                c += k[8];
            case 8:
                b += (static_cast< uint32_t >(k[7])) << 24;
            case 7:
                b += (static_cast< uint32_t >(k[6])) << 16;
            case 6:
                b += (static_cast< uint32_t >(k[5])) << 8;
            case 5:
                b += k[4];
            case 4:
                a += (static_cast< uint32_t >(k[3])) << 24;
            case 3:
                a += (static_cast< uint32_t >(k[2])) << 16;
            case 2:
                a += (static_cast< uint32_t >(k[1])) << 8;
            case 1:
                a += k[0];
            break;
            case 0:
                *pc = c;
                *pb = b;
                return;  /* zero length strings require no mixing */
        }
    }

    FINAL(a,b,c);
    *pc = c;
    *pb = b;

    return;
}

/*
 * hashbig():
 * This is the same as hashword() on big-endian machines.  It is different
 * from hashlittle() on all machines.  hashbig() takes advantage of
 * big-endian byte ordering.
 */

uint32_t
jenkins_hash64_t::hashbig(const void* key, const std::size_t length, uint32_t initval)
{
    uint32_t                                a(0), b(0), c(0);
    union { const void *ptr; size_t i; }    u{0}; /* to cast key to (size_t) happily */
    std::size_t                             len(length);

    if (nullptr == key)
        throw std::invalid_argument("jenkins_hash64_t::hashbig(): invalid parameter (nullptr)");

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + (static_cast< uint32_t >(len)) + initval;
    u.ptr = key;

    if (HASH_BIG_ENDIAN && (0 == (0x3 & u.i))) {
        const uint32_t* k(reinterpret_cast< const uint32_t* >(key)); /* read 32-bit chunks */

        /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
        while (12 < len) {
            a   += k[0];
            b   += k[1];
            c   += k[2];

            MIX(a,b,c);

            len -= 12;
            k   += 3;
        }

        /*----------------------------- handle the last (probably partial) block */
        /*
        * "k[2]<<8" actually reads beyond the end of the string, but
        * then shifts out the part it's not allowed to read.  Because the
        * string is aligned, the illegal read is in the same word as the
        * rest of the string.  Every machine with memory protection I've seen
        * does it on word boundaries, so is OK with this.  But valgrind will
        * still catch it and complain.  The masking trick does make the hash
        * noticeably faster for short strings (like English words).
        */

        switch (len) {
            case 12:
                c += k[2];
                b += k[1];
                a += k[0];
            break;

            case 11:
                c += k[2] & 0xffffff00;
                b += k[1];
                a += k[0];
            break;

            case 10:
                c += k[2] & 0xffff0000;
                b += k[1];
                a += k[0];
            break;

            case 9:
                c += k[2] & 0xff000000;
                b += k[1];
                a += k[0];
            break;

            case 8:
                b += k[1];
                a += k[0];
            break;

            case 7:
                b += k[1] & 0xffffff00;
                a += k[0];
            break;

            case 6:
                b += k[1] & 0xffff0000;
                a += k[0];
            break;

            case 5:
                b += k[1] & 0xff000000;
                a += k[0];
            break;

            case 4:
                a += k[0];
            break;

            case 3:
                a += k[0] & 0xffffff00;
            break;

            case 2:
                a += k[0] & 0xffff0000;
            break;

            case 1:
                a += k[0] & 0xff000000;
            break;

            case 0:
                return c;              /* zero length strings require no mixing */
        }

    } else {                        /* need to read the key one byte at a time */
        const uint8_t* k(reinterpret_cast< const uint8_t* >(key));

        /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
        while (12 < len) {
            a   += (static_cast< uint32_t >(k[0])) << 24;
            a   += (static_cast< uint32_t >(k[1])) << 16;
            a   += (static_cast< uint32_t >(k[2])) << 8;
            a   += (static_cast< uint32_t >(k[3]));
            b   += (static_cast< uint32_t >(k[4])) << 24;
            b   += (static_cast< uint32_t >(k[5])) << 16;
            b   += (static_cast< uint32_t >(k[6])) << 8;
            b   += (static_cast< uint32_t >(k[7]));
            c   += (static_cast< uint32_t >(k[8])) << 24;
            c   += (static_cast< uint32_t >(k[9])) << 16;
            c   += (static_cast< uint32_t >(k[10])) << 8;
            c   += (static_cast< uint32_t >(k[11]));

            MIX(a,b,c);

            len -= 12;
            k   += 12;
        }

        /*-------------------------------- last block: affect all 32 bits of (c) */
        switch (len) {                   /* all the case statements fall through */
            case 12:
                c += k[11];
            case 11:
                c += (static_cast< uint32_t >(k[10])) << 8;
            case 10:
                c += (static_cast< uint32_t >(k[9])) << 16;
            case 9:
                c += (static_cast< uint32_t >(k[8])) << 24;
            case 8:
                b += k[7];
            case 7:
                b += (static_cast< uint32_t >(k[6])) << 8;
            case 6:
                b += (static_cast< uint32_t >(k[5])) << 16;
            case 5:
                b += (static_cast< uint32_t >(k[4])) << 24;
            case 4:
                a += k[3];
            case 3:
                a += (static_cast< uint32_t >(k[2])) << 8;
            case 2:
                a += (static_cast< uint32_t >(k[1])) << 16;
            case 1:
                a += (static_cast< uint32_t >(k[0])) << 24;
            break;
            case 0:
                return c;
        }
    }

    FINAL(a,b,c);
    return c;
}

jenkins_hash64_t::jenkins_hash64_t(void)
{
    return;
}

jenkins_hash64_t::~jenkins_hash64_t(void)
{
    return;
}
            
