/**
 *    Name:        ops.h
 *    Description: what the translated program's C calls for the 68000's memory and arithmetic: the bus cycles
 *                 of a byte, a word and a long, the flags of additions and subtractions, and the helpers of
 *                 the instructions the IR does not spell out
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Every helper does what tools/core/m68000/interpreter.py does for the same IR, which
 *    tools/core/konami/reference/compare_lift_with_mame.py checks against MAME's 68000 core (Olivier
 *    Galibert's: alu_abcd8, alu_sbcd8 and its microcode for DIVU, the shifts a bit at a time).
 *
 *    The bus is the board's (cave/machine.h), 16 bits wide: a byte is the word at its even address with that
 *    half's mem_mask, written on both halves as the 68000 drives it; a long is two words, the high one first.
 */
#ifndef M68000_OPS_H
#define M68000_OPS_H

#include "m68000/cpu.h"
#include "cave/machine.h"

#include <stdint.h>

/* ---- memory ---- */

static inline uint32_t mem_read8(uint32_t address)
{
	const uint16_t word = cave_read16(address & 0xFFFFFEu, (address & 1u) ? 0x00FFu : 0xFF00u);
	return (address & 1u) ? (uint32_t)(word & 0xFFu) : (uint32_t)(word >> 8);
}

static inline uint32_t mem_read16(uint32_t address)
{
	return cave_read16(address, 0xFFFFu);
}

static inline uint32_t mem_read32(uint32_t address)
{
	const uint32_t high = cave_read16(address, 0xFFFFu);
	return high << 16 | cave_read16(address + 2u, 0xFFFFu);
}

static inline void mem_write8(uint32_t address, uint8_t value)
{
	cave_write16(address & 0xFFFFFEu, (uint16_t)(value << 8 | value), (address & 1u) ? 0x00FFu : 0xFF00u);
}

static inline void mem_write16(uint32_t address, uint16_t value)
{
	cave_write16(address, value, 0xFFFFu);
}

static inline void mem_write32(uint32_t address, uint32_t value)
{
	cave_write16(address, (uint16_t)(value >> 16), 0xFFFFu);
	cave_write16(address + 2u, (uint16_t)value, 0xFFFFu);
}

/* ---- shifts as the IR takes them: the count masked like x86 does ---- */

static inline uint32_t shl32(uint32_t value, int count)
{
	return value << (count & 31);
}

static inline uint32_t shr32(uint32_t value, int count)
{
	return value >> (count & 31);
}

static inline int32_t sar32(int32_t value, int count)
{
	return value >> (count & 31);
}

static inline uint64_t shl64(uint64_t value, int count)
{
	return value << (count & 63);
}

static inline uint64_t shr64(uint64_t value, int count)
{
	return value >> (count & 63);
}

/* ---- the flags of a + b + carry and a - b - borrow, at 8, 16 or 32 bits ---- */

#define M68K_FLAGS(bits, mask)                                                                            \
	static inline uint32_t m68k_carry_add##bits(uint32_t a, uint32_t b, uint32_t carry)                   \
	{                                                                                                     \
		return (uint32_t)((((uint64_t)a + b + carry) >> bits) & 1u);                                     \
	}                                                                                                     \
	static inline uint32_t m68k_overflow_add##bits(uint32_t a, uint32_t b, uint32_t carry)                \
	{                                                                                                     \
		const uint32_t result = (uint32_t)(((uint64_t)a + b + carry) & (mask));                           \
		return (((a ^ result) & (b ^ result)) >> (bits - 1)) & 1u;                                       \
	}                                                                                                     \
	static inline uint32_t m68k_carry_sub##bits(uint32_t a, uint32_t b, uint32_t borrow)                  \
	{                                                                                                     \
		return (uint64_t)a < (uint64_t)b + borrow;                                                        \
	}                                                                                                     \
	static inline uint32_t m68k_overflow_sub##bits(uint32_t a, uint32_t b, uint32_t borrow)               \
	{                                                                                                     \
		const uint32_t result = (uint32_t)(((uint64_t)a - b - borrow) & (mask));                          \
		return (((a ^ b) & (a ^ result)) >> (bits - 1)) & 1u;                                            \
	}

M68K_FLAGS(8, 0xFFu)
M68K_FLAGS(16, 0xFFFFu)
M68K_FLAGS(32, 0xFFFFFFFFu)

#undef M68K_FLAGS

/* ---- DIVU.W and DIVS.W: the remainder in the high word and the quotient in the low one; the dividend
   unchanged when the quotient does not fit, and by zero (the exception follows). N and Z as the core's
   microcode leaves them: by zero, of the dividend's high word for DIVU and of the divisor for DIVS (Z set);
   on overflow N set, Z clear; else of the quotient ---- */

/* ---- a timed program's cycles that its data decides (tools/core m68000/timing.py, the lift's m68k_cycles) ---- */

/* MULU: 2 a 1 bit of the source; MULS: 2 a change from 0 to 1 or 1 to 0 along the source with a 0 after its lowest
   bit (the Motorola manual's n) */
static inline uint32_t m68k_mulu_cycles(uint32_t source)
{
	uint32_t ones = 0;

	for (source &= 0xFFFFu; source; source &= source - 1u)
		ones++;
	return 2u * ones;
}

static inline uint32_t m68k_muls_cycles(uint32_t source)
{
	uint32_t bits = (source & 0xFFFFu) << 1, changes = 0;

	for (uint32_t pairs = (bits ^ bits >> 1) & 0xFFFFu; pairs; pairs &= pairs - 1u)
		changes++;
	return 2u * changes;
}

/* DIVU, DIVS: the instruction's time but its operand's, as the 68000's microcode takes it (Jorge Cwik's
   algorithm, which MAME's core and other cycle-exact cores follow); a divisor of 0: the exception's, counted apart */
static inline uint32_t m68k_divu_cycles(uint32_t dividend, uint32_t divisor)
{
	uint32_t cycles = 38, high;

	divisor &= 0xFFFFu;
	if (!divisor)
		return 0;
	if ((dividend >> 16) >= divisor)
		return 10;    /* (overflow) */
	high = divisor << 16;
	for (int bit = 0; bit < 15; bit++)
	{
		const uint32_t before = dividend;

		dividend <<= 1;
		if ((int32_t)before < 0)
			dividend -= high;
		else
		{
			cycles += 2;
			if (dividend >= high)
			{
				dividend -= high;
				cycles--;
			}
		}
	}
	return cycles * 2u;
}

static inline uint32_t m68k_divs_cycles(uint32_t dividend, uint32_t divisor)
{
	const int32_t signed_dividend = (int32_t)dividend;
	const int16_t signed_divisor = (int16_t)divisor;
	uint32_t cycles = 6, quotient, absolute_dividend, absolute_divisor;

	if (!signed_divisor)
		return 0;
	if (signed_dividend < 0)
		cycles++;
	absolute_dividend = signed_dividend < 0 ? 0u - (uint32_t)signed_dividend : (uint32_t)signed_dividend;
	absolute_divisor = signed_divisor < 0 ? (uint32_t)(-(int32_t)signed_divisor) : (uint32_t)signed_divisor;
	if ((absolute_dividend >> 16) >= absolute_divisor)
		return (cycles + 2u) * 2u;    /* (overflow) */
	quotient = absolute_dividend / absolute_divisor;
	cycles += 55;
	if (signed_divisor >= 0)
		cycles += signed_dividend >= 0 ? -1 : 1;
	for (int bit = 0; bit < 15; bit++)
	{
		if ((int16_t)quotient >= 0)
			cycles++;
		quotient <<= 1;
	}
	return cycles * 2u;
}

static inline uint32_t m68k_divu(uint32_t dividend, uint32_t divisor)
{
	uint32_t quotient;

	if (divisor == 0)
		return dividend;
	quotient = dividend / divisor;
	return quotient > 0xFFFFu ? dividend : (dividend % divisor) << 16 | quotient;
}

static inline uint32_t m68k_divu_s(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return dividend >> 31;
	return dividend / divisor > 0xFFFFu ? 1u : (dividend / divisor) >> 15 & 1u;
}

static inline uint32_t m68k_divu_z(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return dividend >> 16 == 0;
	return dividend / divisor > 0xFFFFu ? 0u : dividend / divisor == 0;
}

static inline uint32_t m68k_divu_ov(uint32_t dividend, uint32_t divisor)
{
	return divisor != 0 && dividend / divisor > 0xFFFFu;
}

/* (quotient, remainder) of DIVS.W, and whether the quotient fits in 16 bits */
static inline int m68k_divs_parts(uint32_t dividend, uint32_t divisor, int64_t *quotient, int64_t *remainder)
{
	const int64_t a = (int32_t)dividend;
	const int64_t b = (int16_t)divisor;

	*quotient = a / b;
	*remainder = a - *quotient * b;
	return *quotient >= -0x8000 && *quotient <= 0x7FFF;
}

static inline uint32_t m68k_divs(uint32_t dividend, uint32_t divisor)
{
	int64_t quotient, remainder;

	if (divisor == 0 || !m68k_divs_parts(dividend, divisor, &quotient, &remainder))
		return dividend;
	return ((uint32_t)remainder & 0xFFFFu) << 16 | ((uint32_t)quotient & 0xFFFFu);
}

static inline uint32_t m68k_divs_s(uint32_t dividend, uint32_t divisor)
{
	int64_t quotient, remainder;

	if (divisor == 0)
		return 0;
	if (!m68k_divs_parts(dividend, divisor, &quotient, &remainder))
		return 1u;
	return ((uint32_t)quotient >> 15) & 1u;
}

static inline uint32_t m68k_divs_z(uint32_t dividend, uint32_t divisor)
{
	int64_t quotient, remainder;

	if (divisor == 0)
		return 1;
	if (!m68k_divs_parts(dividend, divisor, &quotient, &remainder))
		return 0u;
	return quotient == 0;
}

static inline uint32_t m68k_divs_ov(uint32_t dividend, uint32_t divisor)
{
	int64_t quotient, remainder;

	return divisor != 0 && !m68k_divs_parts(dividend, divisor, &quotient, &remainder);
}

/* ---- ABCD and SBCD (NBCD is SBCD from 0): the core's alu_abcd8 and alu_sbcd8; X is C ---- */

static inline uint32_t m68k_abcd_raw(uint32_t destination, uint32_t source, uint32_t extend, uint32_t *first)
{
	const uint32_t low = ((destination & 0xFu) + (source & 0xFu) + extend) & 0xFFu;
	uint32_t result;

	*first = (destination + source + extend) & 0xFFFFu;
	result = (*first + (low > 9 ? 6u : 0u)) & 0xFFFFu;
	if (result > 0x9Fu)
		result = (result + 0x60u) & 0xFFFFu;
	return result;
}

static inline uint32_t m68k_abcd(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t first;
	return m68k_abcd_raw(destination, source, extend, &first) & 0xFFu;
}

static inline uint32_t m68k_abcd_cy(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t first;
	return (m68k_abcd_raw(destination, source, extend, &first) & 0x300u) != 0;
}

static inline uint32_t m68k_abcd_ov(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t first;
	const uint32_t result = m68k_abcd_raw(destination, source, extend, &first);
	return (result & 0x80u) && !(first & 0x80u);
}

static inline uint32_t m68k_sbcd_raw(uint32_t destination, uint32_t source, uint32_t extend, uint32_t *first)
{
	const uint32_t low = ((destination & 0xFu) - (source & 0xFu) - extend) & 0xFFu;
	uint32_t result;

	*first = (destination - source - extend) & 0xFFFFu;
	result = (*first - ((low & 0x10u) ? 6u : 0u)) & 0xFFFFu;
	if (*first & 0x100u)
		result = (result - 0x60u) & 0xFFFFu;
	return result;
}

static inline uint32_t m68k_sbcd(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t first;
	return m68k_sbcd_raw(destination, source, extend, &first) & 0xFFu;
}

static inline uint32_t m68k_sbcd_cy(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t first;
	return (m68k_sbcd_raw(destination, source, extend, &first) & 0x300u) != 0;
}

static inline uint32_t m68k_sbcd_ov(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t first;
	const uint32_t result = m68k_sbcd_raw(destination, source, extend, &first);
	return !(result & 0x80u) && (first & 0x80u);
}

/* ---- CHK.W: N of the register against the bound; Z, V and C as the lift has them ---- */

static inline uint32_t m68k_chk_s(uint32_t value, uint32_t bound)
{
	(void)bound;
	return (int16_t)value < 0;
}

static inline uint32_t m68k_chk_z(uint32_t value, uint32_t bound)
{
	(void)bound;
	return (value & 0xFFFFu) == 0;
}

static inline uint32_t m68k_chk_ov(uint32_t value, uint32_t bound)
{
	(void)value;
	(void)bound;
	return 0;
}

static inline uint32_t m68k_chk_cy(uint32_t value, uint32_t bound)
{
	(void)value;
	(void)bound;
	return 0;
}

/* ---- shifts and rotates by a count of 0-63, a bit at a time as the core does them: C the last bit out (0 with
   no shift, but X for ROXL and ROXR), V for ASL any change of the sign bit ---- */

enum { M68K_ASL, M68K_ASR, M68K_LSL, M68K_LSR, M68K_ROL, M68K_ROR, M68K_ROXL, M68K_ROXR };

static inline uint32_t m68k_shift_all(int kind, uint32_t value, uint32_t count, uint32_t extend, int bits,
	uint32_t *carry, uint32_t *overflow)
{
	const uint32_t top = 1u << (bits - 1);
	const uint32_t full = bits == 32 ? 0xFFFFFFFFu : (1u << bits) - 1u;
	uint32_t index;

	*carry = (kind == M68K_ROXL || kind == M68K_ROXR) ? extend : 0u;
	*overflow = 0;
	for (index = 0; index < count; index++)
	{
		uint32_t out, shifted;

		if (kind == M68K_ASL || kind == M68K_LSL || kind == M68K_ROL || kind == M68K_ROXL)
		{
			out = (value & top) ? 1u : 0u;
			shifted = (value << 1) & full;
			if (kind == M68K_ROL)
				shifted |= out;
			else if (kind == M68K_ROXL)
			{
				shifted |= extend;
				extend = out;
			}
			if (kind == M68K_ASL && ((shifted ^ value) & top))
				*overflow = 1;
		}
		else
		{
			out = value & 1u;
			shifted = value >> 1;
			if (kind == M68K_ASR)
				shifted |= value & top;
			else if (kind == M68K_ROR)
				shifted |= out << (bits - 1);
			else if (kind == M68K_ROXR)
			{
				shifted |= extend << (bits - 1);
				extend = out;
			}
		}
		value = shifted;
		*carry = out;
	}
	return value;
}

static inline uint32_t m68k_shift(int kind, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	uint32_t carry, overflow;
	return m68k_shift_all(kind, value, count, extend, bits, &carry, &overflow);
}

static inline uint32_t m68k_shift_carry(int kind, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	uint32_t carry, overflow;
	m68k_shift_all(kind, value, count, extend, bits, &carry, &overflow);
	return carry;
}

static inline uint32_t m68k_shift_overflow(int kind, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	uint32_t carry, overflow;
	m68k_shift_all(kind, value, count, extend, bits, &carry, &overflow);
	return overflow;
}

#endif
