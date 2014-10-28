
struct customer {
	int id;
	int enqueue_sec;
	int dequeue_sec;
	int time_with_teller;
};

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

/*
 * The queue of customers. This structure is manipulated only by functions
 * exposed via the interface in customer.h
 */
static struct customer *queue[MAX_CUSTOMERS_PER_DAY];
static volatile int poll_slot = 0;
static volatile int push_slot = 0;

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
	return push_slot > poll_slot;
}



