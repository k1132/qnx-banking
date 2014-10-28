#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> /* For random call */
#include <time.h>
#include "sim.h"

static const unsigned int DOWNSCALER = 0x8000;
static const unsigned int UINT16_MAX = 0xFFFF;
static const unsigned int UINT16_MIN = 0x0000;

int sim_choose(unsigned int *seed, unsigned int lo, unsigned int hi)
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
	 *                  \  RAND_MAX - 0         /
	 *
	 * The QNX call random() returns a long. The cast to an unsigned int
	 * as performed below is not portable!
	 *
	 * Furthermore, the random number must be downscaled to the UINT16
	 * range for the function above to work. QNX just can't avoid overflow
	 * when working with huge numbers.
	 */
	unsigned int x = ((unsigned int) rand_r(seed));
	unsigned int y = ((x * (hi - lo)) / (RAND_MAX)) + lo;

	return (int) y;
}

void sim_sleep(int sim_seconds, int *sim_sec)
{
	struct timespec rqtp;
	rqtp.tv_sec = 0;
	rqtp.tv_nsec = SIM_SEC_TO_NSC(sim_seconds);

	clock_nanosleep(CLOCK_REALTIME, 0, &rqtp, NULL);

	*sim_sec += sim_seconds;
}

//static int timespec_subtract(struct timespec *result, struct timespec *t1,
//		struct timespec *t0)
//{
//	if ((t1->tv_nsec - t0->tv_nsec) < 0) {
//		result->tv_sec = t1->tv_sec - t0->tv_sec - 1;
//		result->tv_nsec = 1000000000 + t1->tv_nsec - t0->tv_nsec;
//	} else {
//		result->tv_sec = t1->tv_sec - t0->tv_sec;
//		result->tv_nsec = t1->tv_nsec - t0->tv_nsec;
//	}
//
//	return 0;
//}

/* Subtract the ‘struct timespec’ values X and Y,
 storing the result in RESULT.
 Return 1 if the difference is negative, otherwise 0. */
static int timespec_subtract(result, x, y)
	struct timespec *result, *x, *y;
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_nsec < y->tv_nsec) {
		int nums = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
		y->tv_nsec -= 1000000000 * nums;
		y->tv_sec += nums;
	}
	if (x->tv_nsec - y->tv_nsec > 1000000000) {
		int nums = (x->tv_nsec - y->tv_nsec) / 1000000000;
		y->tv_nsec += 1000000000 * nums;
		y->tv_sec -= nums;
	}

	/* Compute the time remaining to wait.
	 tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_nsec = x->tv_nsec - y->tv_nsec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

void sim_elaps_init(struct timespec *t0)
{
	clock_gettime(CLOCK_REALTIME, t0);
}

void sim_elaps_calc(struct timespec *t0, int *sim_sec)
{
	struct timespec t1, el;
	clock_gettime(CLOCK_REALTIME, &t1);

	if (timespec_subtract(&el, &t1, t0)) {
	}

	int inter = SEC_TO_SIM_SEC(el.tv_sec) + NSC_TO_SIM_SEC(el.tv_nsec);

	*sim_sec += inter;
}

void sim_fmt_time(char* thd_buf, size_t count, int sim_s)
{
	int mil_h, mil_m, mil_s;
	mil_m = sim_s / 60;
	mil_s = sim_s % 60;
	mil_h = mil_m / 60;
	mil_m = mil_m % 60;

	snprintf(thd_buf, count, "SIM> %02d:%02d:%02d", mil_h, mil_m, mil_s);
}
