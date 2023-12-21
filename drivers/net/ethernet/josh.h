#ifndef _JOSH_H_
#define _JOSH_H_


#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/udp.h>
#include "intel/igb/igb.h"


static void mybug(void);

#define MYASSERT(b, msg) if(!(b)) {     pr_err(msg); ssleep(10); mybug();  }
#define MSG_COUNT 10


#endif
