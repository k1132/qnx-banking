/*
 * Proj: 4
 * File: customer.c
 * Date: 29 October 2014
 * Auth: Steven Kroh (skk8768)
 *
 * Description:
 *
 * Implements the public interface contained in customer.h. This module
 * contains: code to both instantiate and free customer structs, code to
 * manipulate the customer queue, and code to determine the queue's maximum
 * length over time.
 *
 * The queue backing this module is implemented in a naive way. It is simply
 * an array of the worst-case size. This is fine, as this lab is not about
 * queuing theory.
 */

#include <stdlib.h> /* For malloc */
#include "customer.h"

/**
 * This function allocates memory for and initializes the fields of a new
 * customer struct.
 *
 * Params: cid - the customer id
 * Return: the malloc'ed and initialized customer struct
 */
struct customer *customer_make(int cid)
{
	struct customer *cust = malloc(sizeof(struct customer));

	cust->cid = cid;
	cust->enqueue_sec = 0;
	cust->dequeue_sec = 0;
	cust->time_with_teller = 0;

	return cust;
}

/**
 * This function frees the memory associated with an existing customer struct.
 *
 * Params: cust - the customer struct to free
 * Return: void
 */
void customer_free(struct customer* cust)
{
	free(cust);
}

/**
 * The queue array is an array of (struct customer *). The queue can hold up
 * to MAX_CUSTOMERS_PER_DAY, which is the worst case the queue might encounter.
 *
 * The indices below, poll_slot and push_slot, indicate at any given time
 * which slot in the queue array is to be set or returned.
 *
 * Note: External code must guarantee mutually exclusive access to the data
 * structures below.
 */
static struct customer *queue[MAX_CUSTOMERS_PER_DAY];
static volatile int poll_slot = 0;
static volatile int push_slot = 0;

/**
 * Frees all the customers allocated throughout the simulation
 *
 * Params: void
 * Return: void
 */
void customer_free_all(void)
{
	int i;
	for(i = 0; i < push_slot; i++) {
		customer_free(queue[i]);
	}
}

/**
 * This function indicates to the customer module that no more entries to the
 * queue will be made. This is an important addition, as the tellers need to
 * know when to stop blocking (they stop blocking when there are no more
 * customers in line, and there cannot be any more customers added to the line.
 *
 * Note: External code must guarantee mutually exclusive access to the data
 * structures below.
 *
 * Params: void
 * Return: void
 */
static volatile int q_plugged = 0;
void customer_q_plug()
{
	q_plugged = 1;
}

/*
 * Every time customer_q_push or customer_q_poll are called, those funcitons
 * update the max_depth invariant. That is, with every manipulation of this
 * module, max_depth will always represent the maximum depth the queue has ever
 * been.
 *
 * Params: void
 * return: The maximum depth the queue has ever been, up to this point in time
 */
static volatile int max_depth = 0;
int customer_q_max_depth()
{
	return max_depth;
}

/**
 * Add a customer to the end of the line.
 *
 * Params: cust - the customer structure to add to the end of the line
 * Return: void
 */
void customer_q_push(struct customer *cust)
{
	queue[push_slot++] = cust;

	int depth = push_slot - poll_slot;
	if (depth > max_depth) max_depth = depth;
}

/**
 * Removes and returns a customer from the front of the line
 *
 * Params: void
 * Return: The customer removed from the front of the line
 */
struct customer *customer_q_poll()
{
	int depth = push_slot - poll_slot;
	if (depth > max_depth) max_depth = depth;

	return queue[poll_slot++];
}

/**
 * Determines if a teller can poll something out of the queue of customers.
 * A customer can be polled out of the list if the push_slot is ahead of the
 * poll_slot by at least one. If this is the case, EAVAIL is returned.
 *
 * If there are no customers to poll, and the bank is not allowing any more
 * customers into the line (the bank is plugged), then EEMPTY is returned.
 *
 * If the bank is not yet plugged, and there are no customers to poll, ENOCUS
 * is returned.
 *
 * Params: void
 * Return: EAVAIL - customers are available to poll
 *         EEMPTY - the line is empty and no more customers will be added
 *         ENOCUS - the line currently empty, but more will be added soon
 */
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
