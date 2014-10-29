#ifndef CUSTOMER_H_
#define CUSTOMER_H_

struct customer
{
	int cid;
	int enqueue_sec;
	int dequeue_sec;
	int time_with_teller;
};

struct customer *customer_make(int cid);
void customer_free(struct customer* cust);

/*
 * The maximum size of the queue of customers. This project doesn't deal with
 * queuing theory. We simply create an array to hold each possible customer that
 * will arrive throughout the day.
 *
 * The actual worst case maximum is 420. The bank is open for 420 minutes, and
 * a customer could arrive every single minute (in the worst case). The actual
 * maximum is 500 so we can detect lapses in timing logic.
 */
#define MAX_CUSTOMERS_PER_DAY 500

int customer_q_max_depth(void);
void customer_q_push(struct customer *cust);
struct customer *customer_q_poll();

void customer_q_plug();

#define EAVAIL 0
#define EEMPTY 1
#define ENOCUS 2
int customer_q_can_poll();

#endif
