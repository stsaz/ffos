/** ffos: timer queue
2020, Simon Zolin
*/

/*
fftimerqueue_init
fftimerqueue_add fftimerqueue_addnode
fftimerqueue_remove
fftimerqueue_process
*/

#pragma once

#include <ffbase/rbtree.h>

typedef void (*fftimerqueue_func)(void *param);

typedef struct fftimerqueue_node {
	ffrbt_node rbtnode;
	int interval_msec;
	int active;
	fftimerqueue_func func;
	void *param;
} fftimerqueue_node;

typedef struct fftimerqueue {
	ffrbtree tree;
} fftimerqueue;

/** Insert node into timer-queue tree, accounting for 32-bit overflow each 49 days */
static inline void _fftmrq_node_insert(ffrbt_node *node, ffrbt_node **root, ffrbt_node *sentl)
{
	if (*root == sentl) {
		*root = node; // set root node
		node->parent = sentl;

	} else {
		ffrbt_node **pchild;
		ffrbt_node *parent = *root;

		// find parent node and the pointer to its left/right node
		for (;;) {
			if ((int)(node->key - parent->key) < 0)
				pchild = &parent->left;
			else
				pchild = &parent->right;

			if (*pchild == sentl)
				break;
			parent = *pchild;
		}

		*pchild = node; // set parent's child
		node->parent = parent;
	}

	node->left = node->right = sentl;
}

static inline void fftimerqueue_init(fftimerqueue *tq)
{
	ffrbt_init(&tq->tree);
	tq->tree.insert = &_fftmrq_node_insert;
}

/** Remove timer node
Return 1 if removed */
static inline int fftimerqueue_remove(fftimerqueue *tq, fftimerqueue_node *node)
{
	if (node->active) {
		ffrbt_rm(&tq->tree, &node->rbtnode);
		node->active = 0;
		return 1;
	}
	return 0;
}

/** Add new node
Re-add node if it already exists */
static inline void fftimerqueue_addnode(fftimerqueue *tq, fftimerqueue_node *node)
{
	fftimerqueue_remove(tq, node);
	ffrbt_insert(&tq->tree, &node->rbtnode, NULL);
	node->active = 1;
}

static inline void fftimerqueue_add(fftimerqueue *tq, fftimerqueue_node *node, ffuint now_msec, int interval_msec, fftimerqueue_func func, void *param)
{
	node->interval_msec = interval_msec;
	node->rbtnode.key = now_msec + ffint_abs(interval_msec);
	node->func = func;
	node->param = param;
	fftimerqueue_addnode(tq, node);
}

/** Call the functions of expired events in timer queue.
For one-shot event: remove the node from the queue before calling the associated function.
For periodic event: re-add the node to the queue before calling the associated function.
If new nodes are added during this process, they will be processed:
  * immediately if the new node is newer than the current
  * next time if the new node is older than the current
  Note: another approach of immediate processing of all new events in the past
   may lead to high CPU usage (i.e. hanging) in case the event interval is too small than clock time.
Return the number of processed events */
static inline ffuint fftimerqueue_process(fftimerqueue *tq, ffuint now_msec)
{
	ffuint n = 0;
	fftimerqueue_node *node;
	ffrbt_node *it;
	FFRBT_FOR(&tq->tree, it) {

		ffuint key = it->key;
		/* Check if the time of this event is in the past and thus the event must be processed,
		 accounting for overflow each 49 days of milliseconds stored as a 32-bit value.

		It works for the intervals less than 24 days:
		  * 0xff-0xfe=0x01 -> (1 > 0)? -> stop
		  * 0xff-0xff=0x00 -> (0 > 0)? -> process
		  * 0xff-0x01=0xfe -> (-2 > 0)? -> process

		It doesn't work if interval value is too large or the timer has been suspended for too long:
		  * 0x80-0x00=0x80 -> (-128 > 0)? -> process (incorrect)
		  * 0x01-0x85=0x7c -> (124 > 0)? -> stop (incorrect)
		*/
		if ((int)(key - now_msec) > 0)
			break;

		node = FF_STRUCTPTR(fftimerqueue_node, rbtnode, it);
		it = ffrbt_node_successor(it, &tq->tree.sentl);
		ffrbt_rm(&tq->tree, &node->rbtnode);
		node->active = 0;

		if (node->interval_msec > 0) {
			node->rbtnode.key = ffmax(key + node->interval_msec, now_msec);
			ffrbt_insert(&tq->tree, &node->rbtnode, NULL);
			node->active = 1;
		}

		node->func(node->param);
		n++;
	}
	return n;
}
