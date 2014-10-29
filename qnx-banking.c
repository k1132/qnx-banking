/*
 * Proj: 4
 * File: qnx-banking.c
 * Date: 29 October 2014
 * Auth: Steven Kroh (skk8768)
 *
 * Description:
 *
 * This file contains the implementation of project 4. Using a QNX neutrino
 * system, this program simulates the workflow in a typical banking environment.
 * That is, there is a single queue leading to a "multi-threaded" teller
 * "server".
 *
 * The architecture involves one thread to generate customers (as passive
 * data structures), three threads acting as the tellers, and one thread
 * which accumulates business statistics.
 *
 * This lab applies the following concurrent structures:
 *  - pthreads
 *  - mutexes
 *  - condition variables and broadcasting
 *  - message passing (QNX pulses)
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <sys/neutrino.h>
#include "sim.h"
#include "customer.h"

/*
 * The second at which the bank opens: 9:00 AM converted to seconds.
 */
static const int SEC_AT_BANK_OPEN = SIM_MIL_TO_SEC(9, 0);
/*
 * The second at which the bank closes: 4:00 PM converted to seconds.
 */
static const int SEC_AT_BANK_CLOSE = SIM_MIL_TO_SEC(16, 0);

static const int ARRIVE_LO = MIN_TO_SEC(1); /* The lower cust arrival bound */
static const int ARRIVE_HI = MIN_TO_SEC(4); /* The upper cust arrival bound */

static const int TBREAK_LO = MIN_TO_SEC(30); /* The lower teller break bound */
static const int TBREAK_HI = MIN_TO_SEC(60); /* The upper teller break bound */

static const int LBREAK_LO = MIN_TO_SEC(1); /* The lower teller break length */
static const int LBREAK_HI = MIN_TO_SEC(4); /* The upper teller break length */

static const int TRANST_LO = 30; /* The lower transaction bound */
static const int TRANST_HI = MIN_TO_SEC(6); /* The upper transaction bound */

static const int NUM_TELLERS = 3; /* The number of tellers in the system */

/*
 * This mutex assists the threads in maintaining mutually exclusive access
 * to the structures in the customer module. The mutex is allocated and
 * initialized statically - avoiding a call to pthread_mutex_init().
 */
static pthread_mutex_t queue_mutex =
PTHREAD_MUTEX_INITIALIZER;

/*
 * This condition variable assists the threads in blocking on the customer
 * module when nothing is available to poll. The condvar is allocated and
 * initialized statically - avoiding a call to pthread_cond_init().
 */
static pthread_cond_t queue_cond =
PTHREAD_COND_INITIALIZER;

/*
 * The channel id which will be allocated in main(). The represented channel
 * facilitates communication of statistics from domain threads to the stats
 * muncher thread.
 */
static int chid;
#define MET_CUST_Q_ELAPSED 2 /* Pulse code indicating a queue wait time */
#define MET_CUST_T_ELAPSED 3 /* Pulse code indicating a transaction time */
#define MET_TELL_C_ELAPSED 4 /* Pulse code indicating a teller wait time */

static void cust_gen(void); /* Thread function for the customer generator */
static void teller(int *tid_ptr); /* Thread function for the tellers */
static void stat_muncher(void); /* Thread function for the stats manager */

/**
 * Creates all the threads in the system. This function joins on all spawned
 * pthreads. Furthermore, the statistics pulse channel is allocated here.
 */
int main(int argc, char *argv[])
{
	printf("CON> Entered main().\n");

	printf("CON> Created statistics channel.\n");
	chid = ChannelCreate(_NTO_CHF_DISCONNECT);

	pthread_attr_t thd_attr;
	pthread_attr_init(&thd_attr);

	/* Get current thread's scheduler parameters */
	int thd_policy;
	struct sched_param thd_sched;
	pthread_getschedparam(pthread_self(), &thd_policy, &thd_sched);

	thd_sched.sched_priority--;
	pthread_attr_setschedparam(&thd_attr, &thd_sched);

	/* Create the stats muncher thread */
	pthread_t stat_muncher_thd;
	pthread_create(&stat_muncher_thd, &thd_attr, (void *) stat_muncher,
			NULL);
	printf("CON> stat_muncher_thd created.\n");

	/* Create the customer generator thread */
	pthread_t cust_gen_thd;
	pthread_create(&cust_gen_thd, &thd_attr, (void *) cust_gen, NULL);
	printf("CON> cust_gen_thd created.\n");

	/* Create the teller threads */
	pthread_t teller_thd[NUM_TELLERS];
	int tid;
	int tids[NUM_TELLERS];
	for (tid = 0; tid < NUM_TELLERS; tid++) {
		tids[tid] = tid;
		pthread_create(&teller_thd[tid], &thd_attr, (void *) teller,
				&tids[tid]);

		printf("CON> teller_thd[%d] created.\n", tid);
	}

	/* Join on all threads */
	pthread_join(cust_gen_thd, NULL);
	printf("CON> cust_gen_thd joined.\n");

	for (tid = 0; tid < NUM_TELLERS; tid++) {
		pthread_join(teller_thd[tid], NULL);
		printf("CON> teller_thd[%d] joined.\n", tid);
	}

	pthread_join(stat_muncher_thd, NULL);
	printf("CON> stat_muncher_thd joined.\n");

	/* Free the condition variable and mutex */
	pthread_cond_destroy(&queue_cond);
	pthread_mutex_destroy(&queue_mutex);

	/* Free all customers allocated through the simulation */
	customer_free_all();

	return EXIT_SUCCESS;
}

/*
 * The cust_gen function backs the customer generator thread. Between the time
 * of bank open and close, it continually tries to add more customers to the
 * queue. It does this every 1 to 4 minutes. Whenever a new customer is pushed
 * to the queue, this thread must notify the tellers such that they wake up.
 *
 * At the end of the day, this thread is responsible for waking the tellers up
 * one more time. Otherwise, the tellers will get stuck waiting (when no more
 * customers will show up).
 */
static void cust_gen()
{
	unsigned int thd_seed; /* thread storage for sim_choice() */
	struct timespec thd_stamp; /* thread storage for sim_elaps... */
	char thd_buf[40]; /* thread storage for sim_fmt_time() */

	int sim_sec = SEC_AT_BANK_OPEN; /* current second of the simulation */

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s bank opens.\n", thd_buf);

	int cur_cid = 0;
	int sec_til_close;
	while ((sec_til_close = SEC_AT_BANK_CLOSE - sim_sec) > 0) {
		/* Wait for the next customer to arrive */
		int arrival = sim_choose(&thd_seed, ARRIVE_LO, ARRIVE_HI);
		sim_sleep(arrival, &sim_sec);

		struct customer *next = customer_make(cur_cid++);

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		printf("%s customer %03d enters the bank.\n", thd_buf,
				next->cid);

		/* Gain access to the queue and push the newly arrived cust */
		sim_elaps_init(&thd_stamp);
		pthread_mutex_lock(&queue_mutex); /* Get lock */
		sim_elaps_calc(&thd_stamp, &sim_sec);

		/* Do mutually exclusive work - enqueue the customer */
		next->enqueue_sec = sim_sec;
		customer_q_push(next);

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		printf("%s customer %03d enters the teller line.\n", thd_buf,
				next->cid);

		pthread_cond_broadcast(&queue_cond); /* Broadcast */
		pthread_mutex_unlock(&queue_mutex); /* Release lock */
	}

	/*
	 * The bank is about to close. Plug the queue and notify tellers such
	 * that blocked tellers don't wait forever!
	 */
	pthread_mutex_lock(&queue_mutex);
	customer_q_plug();
	pthread_cond_broadcast(&queue_cond);
	pthread_mutex_unlock(&queue_mutex);

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s bank closes.\n", thd_buf);
}

/*
 * The teller function backs each of the the teller threads. Between the time
 * of bank open and close, it continually tries to pull customers off the queue.
 * After obtaining a customer, the teller will perform the transaction (by
 * sleeping). While doing all this, the teller will communicate with the stats
 * muncher thread, updating its accounting of measurements.
 *
 * Params: tid_ptr - A pointer to this threads' id
 */
static void teller(int *tid_ptr)
{
	int tid = *tid_ptr + 1; /* Print out a tid indexed at 1 */

	/* Attach to the stat_muncher's channel */
	int coid = ConnectAttach(0, (pid_t) 0, chid, 0 | _NTO_SIDE_CHANNEL, 0);

	unsigned int thd_seed; /* thread storage for sim_choice() */
	struct timespec thd_stamp; /* thread storage for sim_elaps... */
	char thd_buf[40]; /* thread storage for sim_fmt_time() */

	int sim_sec = SEC_AT_BANK_OPEN; /* current second of the simulation */

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s teller %d clocks in.\n", thd_buf, tid);

	/* Schedule first break */
	int next_break = sim_sec + sim_choose(&thd_seed, TBREAK_LO, TBREAK_HI);
	/* If blocked, wake after this number of seconds to take a break */
	int wake_after = 0;

	int sec_til_close;
	while ((sec_til_close = SEC_AT_BANK_CLOSE - sim_sec) > 0) {

		/* See if it is time for break */
		take_break: if (sim_sec >= next_break) {
			/* Schedule next break now (at start of break now) */
			next_break = sim_sec + sim_choose(&thd_seed, TBREAK_LO,
					TBREAK_HI);

			sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
			printf("%s teller %d went on break.\n", thd_buf, tid);

			/* Nap for the duration of the break */
			int nap = sim_choose(&thd_seed, LBREAK_LO, LBREAK_HI);
			sim_sleep(nap, &sim_sec);

			sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
			printf("%s teller %d is back at work.\n", thd_buf, tid);
		}

		int twait_t0 = sim_sec; /* Start waiting for the customer */

		sim_elaps_init(&thd_stamp);
		pthread_mutex_lock(&queue_mutex); /* Get lock */
		sim_elaps_calc(&thd_stamp, &sim_sec);

		int poll_code;
		while (((poll_code = customer_q_can_poll()) == ENOCUS)
				&& (wake_after = next_break - sim_sec) > 0) {

			sim_elaps_init(&thd_stamp);

			/* Keep sleeping unless we need to break */
			struct timespec wake_after_ts;
			clock_gettime(CLOCK_REALTIME, &wake_after_ts);

			long my_nanos = SIM_SEC_TO_NSC(min(wake_after, 300));
			long rt_nanos = wake_after_ts.tv_nsec;

			/* Manage overflow */
			if (rt_nanos + my_nanos > 1000000000) {
				wake_after_ts.tv_nsec = (rt_nanos + my_nanos)
						- 1000000000;
				wake_after_ts.tv_sec++;
			} else {
				wake_after_ts.tv_nsec += my_nanos;
			}

			/* Wake at least every 5 minutes to check for a break */
			pthread_cond_timedwait(&queue_cond, &queue_mutex,
					&wake_after_ts);

			sim_elaps_calc(&thd_stamp, &sim_sec);
		}
		/* Break-forcing happens after the lock is released */

		int twait_t1 = sim_sec; /* End of time waiting for a customer */

		/* Do mutually exclusive work - poll the queue */

		/*
		 * The condition will unblock for EAVAIL and EEMPTY. If
		 * customers are available (EAVAIL), then we can poll. However,
		 * It is possible for the bank to close while the teller is
		 * waiting (EEMPTY). In this case, we unblock - but we don't
		 * poll.
		 */
		struct customer *cust = NULL;
		if (poll_code == EAVAIL) {
			cust = customer_q_poll();
			cust->dequeue_sec = sim_sec;
		}
		pthread_mutex_unlock(&queue_mutex); /* Release lock */

		/* Take a break if no customer was polled and it's scheduled */
		if (poll_code != EAVAIL && sim_sec >= next_break) {
			goto take_break;
		}

		/*
		 * Send measurements to the statistics engine - only if there
		 * was a customer polled
		 */
		if (poll_code == EAVAIL) {
			int elaps;
			/* Time customer spent waiting in the queue */
			elaps = cust->dequeue_sec - cust->enqueue_sec;
			MsgSendPulse(coid, -1, MET_CUST_Q_ELAPSED, elaps);

			/* Time teller spent waiting for a new customer */
			elaps = twait_t1 - twait_t0;
			MsgSendPulse(coid, -1, MET_TELL_C_ELAPSED, elaps);
		}

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		if (poll_code == EAVAIL) {
			printf("%s teller %d initiates transaction with "
				"customer %03d.\n", thd_buf, tid, cust->cid);
		} else {
			printf("%s teller %d realizes there are no customers "
				"left to help.\n", thd_buf, tid);

			break; /* Clock out now if there are no more custs */
		}

		/*
		 * Each customer requires between 30 seconds and 6 minutes
		 * for their transaction with the teller.
		 */
		int transt = sim_choose(&thd_seed, TRANST_LO, TRANST_HI);
		sim_sleep(transt, &sim_sec);

		/* Time customer and teller spent in the transaction */
		cust->time_with_teller = transt;
		MsgSendPulse(coid, -1, MET_CUST_T_ELAPSED, transt);

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		printf("%s teller %d completes transaction with "
			"customer %03d.\n", thd_buf, tid, cust->cid);
	}

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s teller %d clocks out.\n", thd_buf, tid);

	/*
	 * Disconnect from the stat_muncher's channel. When all tellers have
	 * disconnected, the stat_muncher will complete.
	 */
	ConnectDetach(coid);
}

#define MET_MAX_DATA_POINTS 500 /* The max number of data points for metrics */

/*
 * The stat_muncher function backs the statistics engine thread. It spins up
 * a channel for communication of new statistics measurements. This occurs over
 * various pulse messages.
 *
 * Once all threads connected to the channel have disconnected, this function
 * will calculate each statistic and print the report out.
 */
static void stat_muncher()
{
	int acc_c = 0; /* Counts the number of customers encountered */
	int max_depth = 0; /* Maximum depth of the customer queue */

	/* Holds elapsed times customers spent waiting in the queue */
	int cust_q_waits[MET_MAX_DATA_POINTS];
	int ind_q = 0;
	/* Holds elapsed times customers spent in transaction with a teller */
	int cust_t_waits[MET_MAX_DATA_POINTS];
	int ind_t = 0;
	/* Holds elapsed times tellers spent waiting for a new customer */
	int tell_c_waits[MET_MAX_DATA_POINTS];
	int ind_c = 0;

	struct _pulse pul;
	int res;
	while (1) {
		res = MsgReceivePulse(chid, &pul, sizeof(struct _pulse), NULL);
		if (res == -1) {
			printf("CON> Error receiving pulse!\n");
			perror(NULL);
		}

		switch (pul.code)
		{
		case _PULSE_CODE_DISCONNECT:
			goto dcon;
			/* When all tellers have disconnected */
		case MET_CUST_Q_ELAPSED:
			cust_q_waits[ind_q++] = pul.value.sival_int;
			acc_c++;
			break;
		case MET_CUST_T_ELAPSED:
			cust_t_waits[ind_t++] = pul.value.sival_int;
			break;
		case MET_TELL_C_ELAPSED:
			tell_c_waits[ind_c++] = pul.value.sival_int;
			break;
		}
	}

	dcon: ChannelDestroy(chid);

	/* Hack: no mutual exclusion to the customer. All other threads done. */
	max_depth = customer_q_max_depth();

	/* Calculate the average of each array */

	int i;
	int avg_q = 0;
	int max_q = 0;
	for (i = 0; i < ind_q; i++) {
		avg_q += cust_q_waits[i];
		if (cust_q_waits[i] > max_q) max_q = cust_q_waits[i];
	}
	avg_q /= ind_q + 1;

	int avg_t = 0;
	int max_t = 0;
	for (i = 0; i < ind_t; i++) {
		avg_t += cust_t_waits[i];
		if (cust_t_waits[i] > max_t) max_t = cust_t_waits[i];
	}
	avg_t /= ind_t + 1;

	int avg_c = 0;
	int max_c = 0;
	for (i = 0; i < ind_c; i++) {
		avg_c += tell_c_waits[i];
		if (tell_c_waits[i] > max_c) max_c = tell_c_waits[i];
	}
	avg_c /= ind_c + 1;

	/* Sleep 1s before printing out the result metrics */
	struct timespec sleep;
	sleep.tv_sec = 1;
	sleep.tv_nsec = 0;
	clock_nanosleep(CLOCK_REALTIME, 0, &sleep, NULL);

	puts("");
	printf("MET> The list of buisness metrics follow:\n");
	printf("MET>\t1 | %25s     | %d\n", "Total customers serviced", acc_c);

	printf("MET>\t2 | %25s (s) | %d\n", "Average queue time", avg_q);
	printf("MET>\t3 | %25s (s) | %d\n", "Average transaction time", avg_t);
	printf("MET>\t4 | %25s (s) | %d\n", "Average teller wait time", avg_c);

	printf("MET>\t5 | %25s (s) | %d\n", "Maximum queue time", max_q);
	printf("MET>\t6 | %25s (s) | %d\n", "Maximum teller wait time", max_c);
	printf("MET>\t7 | %25s (s) | %d\n", "Maximum transaction time", max_t);

	printf("MET>\t8 | %25s     | %d\n", "Maximum queue depth", max_depth);
	puts("");
}
