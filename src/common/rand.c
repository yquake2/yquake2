/*
 * KISS PRNG (c) 2011 Shinobu
 *
 * This file was optained from zuttobenkyou.wordpress.com
 * and modified by the Yamagi Quake II developers.
 *
 * LICENSE: Public domain
 *
 * =======================================================================
 *
 * KISS PRNG, as devised by Dr. George Marsaglia
 *
 * =======================================================================
 */

#include <stdint.h>

#define QSIZE 0x200000
#define CNG (cng = 6906969069ULL * cng + 13579)
#define XS (xs ^= (xs << 13), xs ^= (xs >> 17), xs ^= (xs << 43))
#define KISS (B64MWC() + CNG + XS)

static uint64_t QARY[QSIZE];
static int j;
static uint64_t carry;
static uint64_t xs;
static uint64_t cng;

uint64_t
B64MWC(void)
{
	uint64_t t, x;

	j = (j + 1) & (QSIZE - 1);
	x = QARY[j];
	t = (x << 28) + carry;
	carry = (x >> 36) - (t < x);
	return QARY[j] = t - x;
}

/* 
 * Generate a pseudorandom 
 * signed integer. 
 */
int
randk(void)
{
	return (int)KISS;
}
 
void
randk_seed(void)
{
	uint64_t i;

	/* Seed QARY[] with CNG+XS: */
	for (i = 0; i < QSIZE; i++)
	{
		QARY[i] = CNG + XS;
	}

	/* Run through several rounds 
	   to warm up the state */
	for (i = 0; i < 256; i++)
	{
		randk();
	}
}

