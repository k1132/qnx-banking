#ifndef SIM_H_
#define SIM_H_

/*
 * Proj: 4
 * File: sim.h
 * Date: 29 October 2014
 * Auth: Steven Kroh (skk8768)
 *
 * Description:
 *
 * This file contains the public interface to the simulation module. This module
 * provides macros and functions useful for implementing structures related
 * to the simulation of time.
 */

#include <unistd.h> /* For size_t */

/*
 * Number of nanoseconds per simulated second
 */
#define NSC_PER_SIM_SEC ((long )(100000000/60))

/*
 * Number of simulated seconds per real life second
 */
#define SIM_SEC_PER_SEC ((long )(600))

/*
 * Converts the hour and minute of a military time representation to a number
 * of seconds (starting at second 0).
 */
#define SIM_MIL_TO_SEC(HR, MI) ((3600 * HR) + (60 * MI))

/*
 * Converts a number of simulated seconds to real life nanoseconds
 */
#define SIM_SEC_TO_NSC(SEC) ((NSC_PER_SIM_SEC * SEC))

/**
 * Converts a number of real life nanoseconds to real life seconds
 */
#define NSC_TO_SIM_SEC(NSC) ((NSC / NSC_PER_SIM_SEC))

/**
 * Converts a number of real life seconds to a number of simulated seconds
 */
#define SEC_TO_SIM_SEC(SEC) ((SEC * SIM_SEC_PER_SEC))

/**
 * Converts a number of simulated minutes to a number of simulated seconds
 */
#define MIN_TO_SEC(MI) (60 * MI)

#define SIM_CHOOSE_HI_EXCLUSIVE 0
int sim_choose(unsigned int *seed, unsigned int lo, unsigned int hi);

void sim_sleep(int sim_seconds, int *sim_sec);

void sim_elaps_init(struct timespec *t0);
void sim_elaps_calc(struct timespec *t0, int *sim_sec);

void sim_fmt_time(char* thd_buf, size_t count, int sim_s);

#endif
