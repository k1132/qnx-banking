#include <stdlib.h>
#include <stdio.h>
#include "sim.h"

static void cust_gen(void);

static const int SEC_AT_BANK_OPEN = SIM_MIL_TO_SEC(9, 0);
static const int SEC_AT_BANK_CLOSE = SIM_MIL_TO_SEC(16, 0);

static const int SEC_CUST_ARRIVE_LO = MIN_TO_SEC(1);
static const int SEC_CUST_ARRIVE_HI = MIN_TO_SEC(4);

int main(int argc, char *argv[])
{
	printf("Entered main()\n");

	int i;
	for (i = 0; i < 1000; i++) {
		/* Each new customer arrives every one to four minutes */
		unsigned int choice_sec;
		choice_sec = sim_choose(MIN_TO_SEC(1), MIN_TO_SEC(4));

		printf("Choice (s): %u\n", choice_sec);
	}

	return EXIT_SUCCESS;
}



static void cust_gen()
{
//	int sim_sec = SEC_AT_BANK_OPEN;
//
//	int sec_til_close;
//	while((sec_til_close = SEC_AT_BANK_CLOSE - sim_sec) > 0) {
//		int next = sim_choose(SEC_CUST_ARRIVE_LO, SEC_CUST_ARRIVE_HI);
//
//		clock_nanosleep()
//		usleep(MIC_PER_SIM_SEC(next));
//
//
//	}
}
