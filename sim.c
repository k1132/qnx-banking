#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> /* For random call */
#include <time.h>
#include "sim.h"

/*
 * Proj: 4
 * File: sim.c
 * Date: 29 October 2014
 * Auth: Steven Kroh (skk8768)
 *
 * Description:
 *
 * This file contains the implementation of the public simulation interface.
 * It provides functions to chose a number at random in a range, functions
 * to format time strings, and functions to determine an elapsed time.
 */

/**
 * This function returns a pseudo-random number in the range from lo to hi.
 * If SIM_CHOOSE_HI_EXCLUSIVE is set to 0, then numbers will be returned all
 * the way up through hi. Otherwise, numbers will be returned all the way
 * up through hi - 1.
 *
 * Each thread using sim_choose needs to maintain its own seed such that the
 * random numbers emitted are not corrupted across calls to this method.
 *
 * Params: seed - a thread-local unit of storage
 *         lo   - the lower bound of random numbers returned
 *         hi   - the upper bound of random numbers returned
 * Return: The randomized number within the specified range
 */
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

/**
 * Sleeps the calling thread for the number of simulated seconds provided,
 * and updates the calling thread's simulation accounting (via the pointer
 * to sim_sec).
 *
 * Params: sim_seconds - the number of simulated seconds to sleep for
 *         sim_sec     - a pointer to the calling threads simulation accounting
 * Return: void
 */
void sim_sleep(int sim_seconds, int *sim_sec)
{
	struct timespec rqtp;
	rqtp.tv_sec = 0;
	rqtp.tv_nsec = SIM_SEC_TO_NSC(sim_seconds);

	clock_nanosleep(CLOCK_REALTIME, 0, &rqtp, NULL);

	*sim_sec += sim_seconds;
}

/*
 * The following function is a modified version of the code provided in GNU's
 * documentation online:
 *
 * http://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
 *
 * This function performs Result = X - Y on the provided timespecs.
 *
 * Params: result - The difference
 *         x      - The minuend
 *         y      - The subtrahend
 * Return: 1      - If difference is negative
 *         0      - If difference is positive
 */
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

/**
 * Initializes a thread-local timespec to the current time. This function is
 * used in conjunction with sim_elaps_calc to determine an elapsed time in a
 * thread safe manner.
 *
 * Params: t0 - the timespec to initialize
 * Return: void
 */
void sim_elaps_init(struct timespec *t0)
{
	clock_gettime(CLOCK_REALTIME, t0);
}

/**
 * Calculates the elapsed time based on the provided timespec and the current
 * time. This functino also updates the calling thread's simulation accounting
 * via the provided sim_sec pointer.
 *
 * Params: t0      - the initial time
 *         sim_sec - the calling thread's simulation accounting
 * Return: void
 */
void sim_elaps_calc(struct timespec *t0, int *sim_sec)
{
	struct timespec t1, el;
	clock_gettime(CLOCK_REALTIME, &t1);

	if (timespec_subtract(&el, &t1, t0)) {
	}

	int inter = SEC_TO_SIM_SEC(el.tv_sec) + NSC_TO_SIM_SEC(el.tv_nsec);

	*sim_sec += inter;
}
/*
 * This function formats the provided number of seconds as a time string (in
 * AM/PM notation). The result is stored in the provided buffer
 *
 * Params: thd_buf - allocated memory to store formatted string in
 *         count   - num chars in thd_buf
 *         sim_s   - the number of seconds sim_s == 0 => t = 12:00:00 AM
 * Return: void
 */
void sim_fmt_time(char* thd_buf, size_t count, int sim_s)
{
	int mil_h, mil_m, mil_s;
	mil_m = sim_s / 60;
	mil_s = sim_s % 60;
	mil_h = mil_m / 60;
	mil_m = mil_m % 60;

	char *xm;
	if (mil_h > 12) {
		mil_h -= 12;
		xm = "PM";
	} else if (mil_h == 12) {
		xm = "PM";
	} else {
		xm = "AM";
	}

	snprintf(thd_buf, count, "SIM> %02d:%02d:%02d %s", mil_h, mil_m, mil_s,
			xm);
}
