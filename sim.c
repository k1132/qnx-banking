#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> /* For random call */
#include "sim.h"

static const unsigned int DOWNSCALER = 0x8000;
static const unsigned int UINT16_MAX = 0xFFFF;
static const unsigned int UINT16_MIN = 0x0000;

int sim_choose(unsigned int lo, unsigned int hi)
{
#if SIM_CHOOSE_HI_EXCLUSIVE
	hi--;
#endif
	/*
	 * The chosen random second is 'y' in the following linear function,
	 * where 'x' is a random unsigned integer produced by the QNX system.
	 *
	 * The implementation actually multiplies x by the numerator such that
	 * the final division results in a number with a whole part.
	 *
	 *                  /        hi - lo        \
	 * 	y = f(x) =  | --------------------- | x + lo
	 *                  \  UINT_MAX - UINT_MIN  /
	 *
	 * The QNX call random() returns a long. The cast to an unsigned int
	 * as performed below is not portable!
	 *
	 * Furthermore, the random number must be downscaled to the UINT16
	 * range for the function above to work. QNX just can't avoid overflow
	 * when working with huge numbers.
	 */
	unsigned int x = ((unsigned int) random()) / DOWNSCALER;
	unsigned int y = ((x * (hi - lo)) / (UINT16_MAX - UINT16_MIN)) + lo;

	return (int)y;
}
