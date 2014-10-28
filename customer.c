#include <stdlib.h>
#include "customer.h"

struct customer *customer_make(int cid)
{
	struct customer *cust = malloc(sizeof(struct customer));
	cust->cid = cid;
	cust->enqueue_sec = 0;
	cust->dequeue_sec = 0;
	cust->time_with_teller = 0;

	return cust;
}

void customer_free(struct customer* cust)
{
	free(cust);
}

/*
 * The queue of customers. This structure is manipulated only by functions
 * exposed via the interface in customer.h
 */
static struct customer *queue[MAX_CUSTOMERS_PER_DAY];
static volatile int poll_slot = 0;
static volatile int push_slot = 0;
static volatile int q_plugged = 0;

void customer_q_push(struct customer *cust)
{
	queue[push_slot++] = cust;
}

struct customer *customer_q_poll()
{
	return queue[poll_slot++];
}

int customer_q_can_poll()
{
	if (push_slot > poll_slot) {
		return EAVAIL;
	} else if (q_plugged) {
		return EEMPTY;
	} else {
		return ENOCUS;
	}
}

