#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "sim.h"
#include "customer.h"

static const int SEC_AT_BANK_OPEN = SIM_MIL_TO_SEC(9, 0);
static const int SEC_AT_BANK_CLOSE = SIM_MIL_TO_SEC(16, 0);

static const int ARRIVE_LO = MIN_TO_SEC(1);
static const int ARRIVE_HI = MIN_TO_SEC(4);

static const int TRANST_LO = 30;
static const int TRANST_HI = MIN_TO_SEC(6);

static void cust_gen(void);

int main(int argc, char *argv[])
{
	printf("Entered main()\n");

	cust_gen();

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
		/* TODO: Get lock */
		sim_elaps_calc(&thd_stamp, &sim_sec);

		/* Do mutually exclusive work */
		next->enqueue_sec = sim_sec;
		customer_q_push(next);

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		printf("%s customer %03d enters the teller line.\n", thd_buf,
				next->cid);

		/* TODO: Broadcast */
		/* TODO: Release lock */
	}
}

static void teller(int tid)
{
	unsigned int thd_seed;
	struct timespec thd_stamp;
	char thd_buf[40];

	int sim_sec = SEC_AT_BANK_OPEN;

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s teller %d clocks in.\n", thd_buf, tid);

	while ((sec_til_close = SEC_AT_BANK_CLOSE - sim_sec) > 0) {

		/* TODO: See if it is time for break */

		sim_elaps_init(&thd_stamp);
		/* TODO: Get lock */
		int poll_code;
		while ((poll_code = customer_q_can_poll()) == ENOCUS) {
			/* TODO: Wait on condition */
		}
		sim_elaps_calc(&thd_stamp, &sim_sec);

		/* Do mutually exclusive work */

		if (poll_code == EAVAIL) {
			struct customer *cust = customer_q_poll();
			cust->dequeue_sec = sim_sec;
		}

		/* TODO: Release lock */

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		if (poll_code == EAVAIL) {
			printf("%s teller %d initiates transaction with "
					+ "customer %03d.\n", thd_buf, tid,
					cust->cid);
		} else {
			printf("%s teller %d realizes there are no customers "
					+ "left to help.\n", thd_buf, tid);

			break; /* Clock out now if there are no more custs */
		}

		/*
		 * Each customer requires between 30 seconds and 6 minutes
		 * for their transaction with the teller.
		 */
		int transt = sim_choose(&thd_seed, TRANST_LO, TRANST_HI);
		sim_sleep(transt, &sim_sec);

		cust->time_with_teller = transt;

		sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
		printf("%s teller %d completes transaction with "
				+ "customer %03d.\n", thd_buf, tid, cust->cid);
	}

	/* TODO: Message the stats muncher with metrics info */

	sim_fmt_time(thd_buf, sizeof(thd_buf), sim_sec);
	printf("%s teller %d clocks out.\n", thd_buf, tid);
}
