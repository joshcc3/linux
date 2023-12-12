#include "intel/igb/igb.h"



#define MYASSERT(b, msg) if(!(b)) {     pr_err(msg); BUG();  }
#define MSG_COUNT 10


struct UDPPacket {
  struct ethhdr eth;
  struct iphdr ip;
  struct udphdr udp;
  u8 padding[6];
  u8 data[16];
} __attribute__((packed));
