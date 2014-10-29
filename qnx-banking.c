#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <sys/neutrino.h>
#include "sim.h"
#include "customer.h"

static const int SEC_AT_BANK_OPEN = SIM_MIL_TO_SEC(9, 0);
static const int SEC_AT_BANK_CLOSE = SIM_MIL_TO_SEC(16, 0);

static const int ARRIVE_LO = MIN_TO_SEC(1);
static const int ARRIVE_HI = MIN_TO_SEC(4);

static const int TRANST_LO = 30;
static const int TRANST_HI = MIN_TO_SEC(6);

static const int NUM_TELLERS = 3;

static pthread_mutex_t queue_mutex =
PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t queue_cond =
PTHREAD_COND_INITIALIZER;

static int chid;
#define MET_CUST_Q_ELAPSED 2
#define MET_CUST_T_ELAPSED 3
#define MET_TELL_C_ELAPSED 4

static void cust_gen(void);
static void teller(int *tid_ptr);
static void stat_muncher(void);

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

	pthread_cond_destroy(&queue_cond);
	pthread_mutex_destroy(&queue_mutex);

	return EXIT_SUCCESS;
}

static void cust_gen()
{
	unsigned int thd_seed;
	struct timespec thd_stamp;
	char thd_buf[40];

	int sim_sec = SEC_AT_BANK_OPEN;

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s bank opens.\n", thd_buf);

	int cur_cid = 0;
	int sec_til_close;
	while ((sec_til_close = SEC_AT_BANK_CLOSE - sim_sec) > 0) {
		int arrival = sim_choose(&thd_seed, ARRIVE_LO, ARRIVE_HI);
		sim_sleep(arrival, &sim_sec);

		struct customer *next = customer_make(cur_cid++);

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		printf("%s customer %03d enters the bank.\n", thd_buf,
				next->cid);

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

static void teller(int *tid_ptr)
{
	int tid = *tid_ptr + 1;

	int coid = ConnectAttach(0, (pid_t) 0, chid, 0 | _NTO_SIDE_CHANNEL, 0);

	unsigned int thd_seed;
	struct timespec thd_stamp;
	char thd_buf[40];

	int sim_sec = SEC_AT_BANK_OPEN;

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s teller %d clocks in.\n", thd_buf, tid);

	int sec_til_close;
	while ((sec_til_close = SEC_AT_BANK_CLOSE - sim_sec) > 0) {

		/* TODO: See if it is time for break */

		int twait_t0 = sim_sec;

		sim_elaps_init(&thd_stamp);
		pthread_mutex_lock(&queue_mutex); /* Get lock */
		int poll_code;
		while ((poll_code = customer_q_can_poll()) == ENOCUS) {
			/*printf("%s teller %d wakes with code %d.\n", thd_buf,
			 tid, poll_code);*/

			/* Wait on condition - until custs are available */
			pthread_cond_wait(&queue_cond, &queue_mutex);
		}
		sim_elaps_calc(&thd_stamp, &sim_sec);

		int twait_t1 = sim_sec;

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

		if(poll_code == EAVAIL) {
			int elaps;
			elaps = cust->dequeue_sec - cust->enqueue_sec;
			MsgSendPulse(coid, -1, MET_CUST_Q_ELAPSED, elaps);

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

		cust->time_with_teller = transt;
		MsgSendPulse(coid, -1, MET_CUST_T_ELAPSED, transt);

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		printf("%s teller %d completes transaction with "
			"customer %03d.\n", thd_buf, tid, cust->cid);
	}

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s teller %d clocks out.\n", thd_buf, tid);

	ConnectDetach(coid);
}

#define MET_MAX_DATA_POINTS 500
static void stat_muncher()
{
	int acc_c = 0;
	int max_depth = 0;

	int cust_q_waits[MET_MAX_DATA_POINTS];
	int ind_q = 0;
	int cust_t_waits[MET_MAX_DATA_POINTS];
	int ind_t = 0;
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
