#ifndef _JOSH_H_
#define _JOSH_H_


#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/udp.h>
#include "intel/igb/igb.h"


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;


static void mybug(void);

#define MYASSERT(b, msg) if(!(b)) {     pr_err(msg); ssleep(10); mybug();  }
#define MSG_COUNT 10


struct UDPPacket {
  struct ethhdr eth;
  struct iphdr ip;
  struct udphdr udp;
  u8 padding[6];
  u8 data[16];
} __attribute__((packed));

static const u8 MD_PACKET_TYPE = 1;

struct MDPayload {

  u8 packetType;
  char _padding[5];
  u64 seqNo;
  u64 localTimestamp;
  i64 price;
  i32 qty;
  u8 flags;
  char _padding2[5];
};

struct UDPMDPacket {
  struct ethhdr eth;
  struct iphdr ip;
  struct udphdr udp;
  struct MDPayload payload;
} __attribute__((packed));



#endif
