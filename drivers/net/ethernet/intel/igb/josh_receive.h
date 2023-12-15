
#ifndef _JOSH_RECEIVE_H_
#define _JOSH_RECEIVE_H_

#include "../../josh.h"
#include "igb.h"

void josh_handle_packets(struct igb_q_vector* qvec, int irq);

#define JOSH_RX_PAGE_IN_CACHE_SHIFT 0
#define JOSH_RX_PAGE_PROCESSED_SHIFT 1

#endif
