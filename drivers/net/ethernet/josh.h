#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/udp.h>
#include "intel/igb/igb.h"


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


#define MYASSERT(b, msg) if(!(b)) {     pr_err(msg); ssleep(10); BUG();  }
#define MSG_COUNT 10


struct UDPPacket {
  struct ethhdr eth;
  struct iphdr ip;
  struct udphdr udp;
  u8 padding[6];
  u8 data[16];
} __attribute__((packed));
