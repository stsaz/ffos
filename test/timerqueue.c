/** ffos: timerqueue.h tester
2020, Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/timerqueue.h>
#include <FFOS/test.h>

static int tq_func_n;

void tq_func(void *param)
{
	int i = (ffsize)param;
	tq_func_n += i;
}

void test_timerqueue()
{
	fftimerqueue tq;
	fftimerqueue_node node, node2, node3;
	fftimerqueue_init(&tq);

	ffmem_zero_obj(&node);
	fftimerqueue_add(&tq, &node, 1, -2, tq_func, NULL);
	fftimerqueue_remove(&tq, &node);
	xieq(0, tq.tree.len);

	ffmem_zero_obj(&node);
	ffmem_zero_obj(&node2);
	ffmem_zero_obj(&node3);
	fftimerqueue_add(&tq, &node, 1, -2, tq_func, NULL); // one-shot
	fftimerqueue_add(&tq, &node2, 1, -2, tq_func, NULL); // one-shot
	fftimerqueue_add(&tq, &node3, 1, -3, tq_func, NULL); // one-shot
	xieq(0, fftimerqueue_process(&tq, 1));
	xieq(2, fftimerqueue_process(&tq, 3));
	xieq(1, fftimerqueue_process(&tq, 4));
	xieq(0, tq.tree.len);

	ffmem_zero_obj(&node);
	tq_func_n = 0;
	fftimerqueue_add(&tq, &node, 1, 2, tq_func, NULL); // periodic
	xieq(0, fftimerqueue_process(&tq, 2));
	xieq(1, fftimerqueue_process(&tq, 3));
	xieq(1, fftimerqueue_process(&tq, 5));
	xieq(0, fftimerqueue_process(&tq, 6));
	fftimerqueue_remove(&tq, &node);

	// 32-bit overflow
	ffmem_zero_obj(&node);
	ffmem_zero_obj(&node2);
	ffmem_zero_obj(&node3);
	tq_func_n = 0;
	fftimerqueue_add(&tq, &node, 0xfffffffd, -1, tq_func, (void*)1);
	fftimerqueue_add(&tq, &node2, 0xfffffffe, -1, tq_func, (void*)10);
	fftimerqueue_add(&tq, &node3, 1, -2, tq_func, (void*)100);
	xieq(0, fftimerqueue_process(&tq, 0xfffffffd));
	xieq(1, fftimerqueue_process(&tq, 0xfffffffe));
	xieq(1, tq_func_n);
	xieq(0, fftimerqueue_process(&tq, 0xfffffffe));
	xieq(1, fftimerqueue_process(&tq, 1));
	xieq(11, tq_func_n);
	xieq(1, fftimerqueue_process(&tq, 3));
	xieq(111, tq_func_n);
}
