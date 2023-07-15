/** ffos: kernel call queue
2022, Simon Zolin */

/*
ffkcallq_process_sq
ffkcallq_process_cq
ffkcall_cancel
fffile_open_async
fffile_info_async
fffile_read_async fffile_write_async
*/

#pragma once
#include <FFOS/file.h>
#include <FFOS/semaphore.h>
#include <FFOS/queue.h>
#include <ffbase/ringqueue.h>

struct ffkcallqueue {
	ffringqueue *sq; // submission queue
	ffsem sem; // triggerred on sq submit to wake sq reader thread (optional)

	ffringqueue *cq; // completion queue
	ffkq_postevent kqpost; // triggerred on cq submit to wake cq reader thread (optional)
	void *kqpost_data;
};

typedef void (*kcall_func)(void *obj);

struct ffkcall {
	struct ffkcallqueue *q;

	ffushort op; // enum FFKCALL_OP
	ffushort state; // 0, 1: in SQ, 2: in CQ
	union {
		struct {
			ffuint error;
			ffint64 result;
		};
		struct {
			ffuint flags;
			const char *name;
		};
		struct {
			fffd fd;
			void *buf;
			ffsize size;
		};
	};
	fffileinfo *finfo;
	kcall_func handler;
	void *param;
};

enum FFKCALL_OP {
	FFKCALL_FILE_OPEN = 1,
	FFKCALL_FILE_INFO,
	FFKCALL_FILE_READ,
	FFKCALL_FILE_WRITE,
};

/** Process the submission queue, perform operations, store result in completion queue */
static inline void ffkcallq_process_sq(ffringqueue *sq)
{
	struct ffkcall *kc;
	for (;;) {
		if (0 != ffrq_fetch(sq, (void**)&kc))
			break;

		switch (kc->op) {
		case FFKCALL_FILE_OPEN:
			kc->result = fffile_open(kc->name, kc->flags);
			break;

		case FFKCALL_FILE_INFO:
			kc->result = fffile_info(kc->fd, kc->finfo);
			break;

		case FFKCALL_FILE_READ:
			kc->result = fffile_read(kc->fd, kc->buf, kc->size);
			break;

		case FFKCALL_FILE_WRITE:
			kc->result = fffile_write(kc->fd, kc->buf, kc->size);
			break;

		case 0:
			kc->state = 0;
			continue;
		}

		kc->error = fferr_last();
		kc->state = 2;

		ffuint unused;
		if (0 != ffrq_add(kc->q->cq, kc, &unused)) {
			FF_ASSERT(0);
		}

		if (unused == kc->q->cq->cap && kc->q->kqpost != FFKQ_NULL) {
			ffcpu_fence_release();
			ffkq_post(kc->q->kqpost, kc->q->kqpost_data);
		}
	}
}

/** Process the complettion queue, call a result handling function */
static inline void ffkcallq_process_cq(ffringqueue *cq)
{
	struct ffkcall *kc;
	for (;;) {
		if (0 != ffrq_fetch_sr(cq, (void**)&kc))
			break;

		kc->state = 0;
		if (kc->handler != NULL)
			kc->handler(kc->param);
	}
}

static inline void ffkcall_cancel(struct ffkcall *kc)
{
	kc->op = 0;
	kc->handler = NULL;
}

static inline void _ffkcall_add(struct ffkcall *kc, int op)
{
	kc->op = op;
	ffuint unused;
	if (0 != ffrq_add(kc->q->sq, kc, &unused)) {
		kc->op = 0;
		fferr_set(EAGAIN);
		return;
	}
	if (kc->q->sem != FFSEM_NULL) {
		ffcpu_fence_release();
		ffsem_post(kc->q->sem);
	}
	fferr_set(EINPROGRESS);
}

static inline fffd fffile_open_async(const char *name, ffuint flags, struct ffkcall *kc)
{
	if (kc->state != 0) {
		// the previous operation is still active in SQ or CQ
		fferr_set(EBUSY);
		return FFFILE_NULL;
	}

	if (kc->op != 0) {
		kc->op = 0;
		fferr_set(kc->error);
		return kc->result;
	}

	kc->name = name;
	kc->flags = flags;
	_ffkcall_add(kc, FFKCALL_FILE_OPEN);
	return FFFILE_NULL;
}

static inline int fffile_info_async(fffd fd, fffileinfo *fi, struct ffkcall *kc)
{
	if (kc->state != 0) {
		// the previous operation is still active in SQ or CQ
		fferr_set(EBUSY);
		return -1;
	}

	if (kc->op != 0) {
		kc->op = 0;
		fferr_set(kc->error);
		return kc->result;
	}

	kc->fd = fd;
	kc->finfo = fi;
	_ffkcall_add(kc, FFKCALL_FILE_INFO);
	return -1;
}

static inline ffssize fffile_read_async(fffd fd, void *buf, ffsize size, struct ffkcall *kc)
{
	if (kc->state != 0) {
		// the previous operation is still active in SQ or CQ
		fferr_set(EBUSY);
		return -1;
	}

	if (kc->op != 0) {
		kc->op = 0;
		fferr_set(kc->error);
		return kc->result;
	}

	kc->fd = fd;
	kc->buf = buf;
	kc->size = size;
	_ffkcall_add(kc, FFKCALL_FILE_READ);
	return -1;
}

static inline ffssize fffile_write_async(fffd fd, const void *buf, ffsize size, struct ffkcall *kc)
{
	if (kc->state != 0) {
		// the previous operation is still active in SQ or CQ
		fferr_set(EBUSY);
		return -1;
	}

	if (kc->op != 0) {
		kc->op = 0;
		fferr_set(kc->error);
		return kc->result;
	}

	kc->fd = fd;
	kc->buf = (void*)buf;
	kc->size = size;
	_ffkcall_add(kc, FFKCALL_FILE_WRITE);
	return -1;
}

// Disable ffkcall logic entirely.  This may be useful for testing/debugging.
#if 0
#define fffile_open_async(name, flags, kc)  fffile_open(name, flags)
#define fffile_info_async(fd, fi, kc)  fffile_info(fd, fi)
#define fffile_read_async(fd, buf, size, kc)  fffile_read(fd, buf, size)
#define fffile_write_async(fd, buf, size, kc)  fffile_write(fd, buf, size)
#endif
