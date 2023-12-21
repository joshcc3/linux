#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "josh_receive.h"
#include "josh/cppkern/test.h"

static void mybug()
{
	BUG();
}

/*
  https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/82576eb-gigabit-ethernet-controller-datasheet.pdf

  Section 8 talks about the programming interface, section 7 talks about the implementation and descriptive details

  TODO - aRFS - hardware accelerated receive flow steering.
  NSE - no snoop enable -
  enable this for receive buffers that are not related to my stuff.

  how to verify if data is in processor cache?

  srrctl controls the size of the receive buffers.
  - make sure Drop_en is set to false: driver sets drop enable if supporting multiple queues and rx flow is disabled
  - assert RDMTS == 0. i.e. that an interrupt is issue as soon as there is any data availableo


  the driv
  TODO - for all the ones we configure here that are not available in ethtool - add them to the driver.

  TODO - there are a bunch of ways to trigger interrupts based on TCP packet data.

  u32 rctl = r32(E1000_RCTL(phyQIx));
  assert((rctl & RXEN) == 1); // receiever enable
  assert((rctl & SBP) == 1); // store bad packets and forward - hopefully this disables mac checking
  assert((rctl & LPE) == 0); // disable long packet reception - we shouldn't be receiving large packets on this interface
  // Do we want promiscuous mode enabled?- probably not?
  assert((rctl & BSIZE) == 3);  // set the buffer size to 3. we expect all our receive buffer packets to be <= 256 bytes: 200 byte payload + 42 bytes header (udp) or 54 bytes tcp header.
  assert((rctl & VFE) == 0); // vlan disabled
  assert((rctl & PSP) == 1); // pad small receive packets, SERC also set
  assert((rctl & SECRC) == 1); // strip ecrc and do not report in descriptor length

  u32 srrctl = r32(E1000_SRRCTL(phyQIx));
  assert((srrctl & 0x63) == 1); // receive window size in 2^(x + 10)
  assert((srrctl & (0x31 << 20)) == 0); // rdmst - rx desc min sz threshold
  assert((srrctl & (1 << 31)) == 0); // drop enabled

  u32 rdbal = r32(E1000_RDBAL(phyQIx));
  assert((rdbal & 0x7f) == 0); // read descriptor base address lower 32 bits (bottom 7 bits are 0 aligned on 128)

  u32 rdLen = r32(E1000_RDLEN(phyQIx));
  assert((rdLen + rdLen/RX_DESC_SZ * RX_BUFFER_SZ <= (32 << 10))); // we only want to keep as many descriptors and buffer memory available as would fit in the L1 cache (keep the cache half full of descriptors) // should be not more than 128 descriptors

  u32 rxdCtl = r32(E1000_RXDCTL(phyQIx));
  assert((rxdCtl & PTHRESH) >= 10);
  assert((rdxCtl & WTHRESH) == 1);
  assert((rdxCtl & ENABLE) == 1);

  u32 drxMXOD = r32(E1000_DRXMXOD(phyQIx));
  assert(drxMXOD == 0x400); // allow max outstanding data
  // TODO - assert 2 way DMA
  // TODO - disable checksum calc on packet
  assert((rxcsum & IPOFLD) == 0); // disable tcpudp, ip csum offload/
  assert((rxcsum & TUOFLD) == 0);
  assert((rxcsum & PCSD) == 0);


  u32 mrqc = r32(E1000_MRQC(phyQIx));
  assert(mrqc == 2); // TODO - what's the best options here? recv q based on MAC/RSS. rss based on tcpudp hashes

  // TODO - set RETA - redirection table





  // TODO - enable multiple receive queues to handle faster packet processing.



  // Use SWFLUSH to flush waiting descriptors

  >
  u32 droppedPackets = r32(E1000_RQDPC(phyQIx));
  assert(droppedPackets == 0);



  //  For TX: you want to set TCTL - t


  u32 ipg = r32(E1000_TIPG); // this is globally configured
  assert(ipg & IPGT == 0); // interpacketgap (4 is added) expressed in MAC clocks: so this is 32ns for a 1Gbps connection.
  TDLEN
  DRXCTL
  DTXMXSZRQ
  TXDCTL - PTHRESH, HTHRESH - these control the rate at which tx descriptors are fetched. we probably want to set these to maximum values. prefetch eagerly. HTHRESH set to 0 and PTHRESH set to max value

  WTHRESH, ENABLE

  TXPS, RXPS




  receive descriptor info
  asserts:
  RSS == 0
  LSB = bits set 0, 4,5 (4 is tcp, 5 is udp)
  assert this packet has only 1 descriptor
  HDR_LEN == 54 for tcp and 42 for udp

  SPH == 1 - split header - header len is useful
  HBO == 0

  EOP == 1
  L4I == 0 - integridty check

  PKT_LEN - packet size.

  recveive descriptors are 16 bytes
  For applications where the latency of received packets is more important that the bus efficiency and the
  CPU utilization, an EITR value of zero may be used. In this case, each receive descriptor will be written
  to the host immediately. If RXDCTL.WTHRESH equals zero, then each descriptor will be written back
  separately, otherwise, write back of descriptors may b

  Do we want header splitting? Unlikely right - the only thing that might be interesting in real marketdata would be the size.

  how large is the cache line on the network device.

  device status register


  struct igb_q_vector {
  struct igb_adapter *adapter;	 backlink
  int cpu;			 CPU for DCA
  u32 eims_value;			 EIMS mask value

  u16 itr_val;
  u8 set_itr;
  void __iomem *itr_register;

  struct igb_ring_container rx, tx;

  struct napi_struct napi;
  struct rcu_head rcu;	 to avoid race with update stats on free
  char name[IFNAMSIZ + 9];

  for dynamic allocation of rings associated with this q_vector
  struct igb_ring ring[] ____cacheline_internodealigned_in_smp;
  };




  struct igb_ring_container {
  struct igb_ring *ring;		 pointer to linked list of rings
  unsigned int total_bytes;	 total bytes processed this int
  unsigned int total_packets;	 total packets processed this int
  u16 work_limit;			 total work allowed per interrupt
  u8 count;			 total number of rings in vector
  u8 itr;				 current ITR setting for ring
  };




  struct igb_ring {
  struct igb_q_vector *q_vector;	 backlink to q_vector
  struct net_device *netdev;	 back pointer to net_device
  struct bpf_prog *xdp_prog;
  struct device *dev;		 device pointer for dma mapping
  union {				 array of buffer info structs
  struct igb_tx_buffer *tx_buffer_info;
  struct igb_rx_buffer *rx_buffer_info;
  };
  void *desc;			 descriptor ring memory
  unsigned long flags;		 ring specific flags
  void __iomem *tail;		 pointer to ring tail register
  dma_addr_t dma;			 phys address of the ring
  unsigned int  size;		 length of desc. ring in bytes

  u16 count;			 number of desc. in the ring
  u8 queue_index;			 logical index of the ring
  u8 reg_idx;			 physical index of the ring
  bool launchtime_enable;		 true if LaunchTime is enabled
  bool cbs_enable;		 indicates if CBS is enabled
  s32 idleslope;			 idleSlope in kbps
  s32 sendslope;			 sendSlope in kbps
  s32 hicredit;			 hiCredit in bytes
  s32 locredit;			 loCredit in bytes

  everything past this point are written often
  u16 next_to_clean;
  u16 next_to_use;
  u16 next_to_alloc;

  union {
  TX
  struct {
  struct igb_tx_queue_stats tx_stats;
  struct u64_stats_sync tx_syncp;
  struct u64_stats_sync tx_syncp2;
  };
  RX
  struct {
  struct sk_buff *skb;
  struct igb_rx_queue_stats rx_stats;
  struct u64_stats_sync rx_syncp;
  };
  };
  struct xdp_rxq_info xdp_rxq;
  } ____cacheline_internodealigned_in_smp;



  ----
  Need to be careful with reading hardware registers are they are sometimes reset on read.

  Its interesting, the NIC fires rx interrupts for packets that come in however they have a size of 0. Multicast packets I understand, that is configured at the nic level. however ip6 and even ipv4 dhcp packets come in from the nic with the rx descriptor set to 0. Wonder why this is.
  Ok for another thing we get regular interrupts from the nic every 2 seconds. I wonder what these interrupts are.

todo we should make sure that the driver calls its poll in a different thread

*/

void handleMDPacket(struct UDPMDPacket *packet, u16 packetSz)
{
	// update the page reference count.
	// there is a page count bias which seems to control how and when pages are reused (reused whem?);
	//
	pr_info("version %d, ihl %d, udp sz %d, packetlen %d",
		packet->ip.version, packet->ip.ihl, ntohs(packet->udp.len),
		packetSz);
	MYASSERT(packet->ip.version == 4, "IP VERSION");
	MYASSERT(packet->ip.ihl == 5, "IHL");

	MYASSERT(packet->udp.len == sizeof(struct UDPPacket) -
					    sizeof(struct ethhdr) -
					    sizeof(struct iphdr),
		 "PACKET SZ");
	MYASSERT(packetSz == sizeof(struct UDPMDPacket), "Packet len");

	MYASSERT(packetSz == sizeof(struct UDPMDPacket), "Packet Sz");
	pr_info("Observed MD Packet %llu, %llu, %llu, %d, %x",
		packet->payload.seqNo, packet->payload.localTimestamp,
		packet->payload.price, packet->payload.qty,
		packet->payload.flags);

	swapcontext_(&mainCtx, &blockingRecvCtx);
}

void handlePacket(struct igb_q_vector *qvec, int packetSz, int ntc)
{
	MYASSERT(!qvec->tx.ring, "tx ring");
	MYASSERT(NULL != qvec->ring, "qvec ring");
	MYASSERT(qvec->cpu == 0, "cpu");
	MYASSERT(NULL != qvec->itr_register, "register");
	MYASSERT(qvec->rx.count != 0, "num desc");
	MYASSERT(qvec->rx.total_bytes == 0, "processed bytes");
	MYASSERT(qvec->rx.total_packets == 0, "processed packets");

	struct igb_ring *rx = qvec->rx.ring;
	struct igb_rx_buffer *rxbuf = &(rx->rx_buffer_info[ntc]);
	struct page *dataPage = rxbuf->page;
	MYASSERT(rxbuf->page_offset < PAGE_SIZE, "PAGE_OFFSET invalid");
	struct UDPMDPacket *packet =
		(struct UDPMDPacket *)(page_address(dataPage) +
				       rxbuf->page_offset);
	bool isIP = packet->eth.h_proto == htons(ETH_P_IP);
	// TODO Filter this on the incoming port.
	bool isMDPacket = isIP && packet->ip.protocol == 17 &&
			  packet->payload.packetType == MD_PACKET_TYPE;

	if (isMDPacket) {
		rxbuf->joshFlags |= (1 << JOSH_RX_PAGE_PROCESSED_SHIFT);
		handleMDPacket(packet, packetSz);
	}
}

void josh_handle_packets(struct igb_q_vector *qvec, int irq)
{
	struct igb_ring *rx = (qvec->rx.ring);
	int iterations = 0;
	bool isFrag = false;

	int ogNTC = rx->next_to_clean;
	int ogNTU = rx->next_to_use;
	int ogNTA = rx->next_to_alloc;

	while (true) {
		// TODO what happens when we read the rescriptor descToClean? Is that mmap io and therefore can the nic keep pushing descriptors as we write them back to it.
		// TODO - check for interrupts that have happened in the meantime. are there any rules about how long this interrupt servicing routine can take - hopefully we cannot be preempted here.
		// TODO - does a read on the descriptor 0 it? Highly unlikely but do need to verify in the data sheet.
		u16 ntc = rx->next_to_clean;
		MYASSERT(ntc <= rx->count, "Invalid next to clean");

		union e1000_adv_rx_desc *descToClean =
			IGB_RX_DESC(rx, rx->next_to_clean);
		u16 packetSz = le16_to_cpu(descToClean->wb.upper.length);

		if (packetSz == 0) {
			break;
		}
		MYASSERT(++iterations <= rx->count,
			 "Unexpectedly looped through rx buffers");
		MYASSERT(rx->next_to_clean != rx->next_to_use &&
				 rx->next_to_clean != rx->next_to_alloc,
			 "RX ring next_to_clean invalid");

		MYASSERT(packetSz == le16_to_cpu(descToClean->wb.upper.length),
			 "Read on rx desc 0 it");
		u32 stBits = descToClean->wb.upper.status_error;
		u32 status_error = stBits >> 19;
		u32 status = stBits & ~((u32)(-1) << 19);
		bool dd = status & 1;
		bool eop = (status >> 1) & 1;
		// TODO - this is a bug with the qemu hardware implementation - it does not set this. (grepping for PIF)
		// AFAIK - the driver doesn't use this either *shrug*, can test to see on real hardware though
		//    bool pif = (status >> 6) & 1;
		bool vp = (status >> 3) & 1;

		MYASSERT(dd == 1, "Descriptor done flag unset.");

		if ((status_error) != 0) {
			pr_info("NIC reported error: %x",
				descToClean->wb.upper.status_error);
			continue;
		}
		bool packetFilters = false;
		bool b;

		if ((b = eop != 1)) {
			isFrag = true;
			packetFilters |= b;
			pr_info("Skipping fragmented packets");
		}
		// TODO Why is pif set?
		/*if((b = pif == 1)) {
      packetFilters |= b;
      pr_info("Packets not destined for us");

      }*/
		if ((b = vp == 1)) {
			packetFilters |= b;
			pr_info("Vlan packet");
		}

		if ((b = dd != 1)) {
			packetFilters |= b;
			pr_info("Descriptor Done not set");
		}
		if ((b = packetSz == 0)) {
			packetFilters |= b;
			pr_info("LEngth is 0");
		}
		packetFilters |= isFrag;

		isFrag = isFrag && (eop == 0);

		if (packetFilters) {
			pr_info("Bailing on irq %d, queue %d", irq,
				qvec->rx.ring->queue_index);
			continue;
		}

		dma_rmb();

		struct igb_rx_buffer *rxbuf = &(rx->rx_buffer_info[ntc]);
		prefetchw(rxbuf->page);
		dma_sync_single_range_for_cpu(rx->dev, rxbuf->dma,
					      rxbuf->page_offset, packetSz,
					      DMA_FROM_DEVICE);

		MYASSERT(rxbuf->joshFlags == 0,
			 "Josh Flags Descriptor not reset.");
		rxbuf->joshFlags = 1 << JOSH_RX_PAGE_IN_CACHE_SHIFT;
		handlePacket(qvec, packetSz, ntc);
		rx->next_to_clean = (rx->next_to_clean + 1) % rx->count;
	}
	// must alloc buffer.
	rx->next_to_clean = ogNTC;

	MYASSERT(ogNTC == rx->next_to_clean,
		 "OG ntc rx ring state not preserved.");
	MYASSERT(ogNTU == rx->next_to_use,
		 "OG ntu rx ring state not preserved.");
	MYASSERT(ogNTA == rx->next_to_alloc,
		 "OG nta rx ring state not preserved.");
}

/*
  Bugs: I'm getting bugs with a kernel panic -was getting it when trying to get the realtime - after doing a lot of gdb

  122.593628] Call Trace:
  122.593736]  <IRQ>
  122.593828]  dump_stack_lvl+0x36/0x50
  122.593993]  panic+0x312/0x340
  122.594134]  ? __schedule+0x850/0x850
  122.594296]  __stack_chk_fail+0x15/0x20
  122.594466]  __schedule+0x850/0x850
  122.594624]  schedule+0x2d/0xb0
  122.594765]  ? __napi_poll+0x23/0x1b0
  122.594927]  ? net_rx_action+0x29e/0x310
  122.595100]  ? __do_softirq+0xbd/0x29e
  122.595268]  ? handle_edge_irq+0x86/0x220
  122.595443]  ? irq_exit_rcu+0x37/0x80
  122.595609]  ? common_interrupt+0x80/0x90
  122.595786]  </IRQ>
  122.596146] Kernel Offset: disabled

TODO: still getting kernel panics with page faults - likely due to dma fetches

*/
