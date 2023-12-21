#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "josh.h"

static void mybug()
{
	BUG();
}

/*

Calling into igb_xmit_frame_ring:
need 5 descriptors in total -
1 descriptor for the header, 1 desc for page of data.
2 for the gap and 1 context descr



should lock a particular netdev queue to a particlar cpu.

struct xdp_frame {
	void *data;
	u16 len;
	u16 headroom;
	u32 metasize;  uses lower 8-bits
	 Lifetime of xdp_rxq_info is limited to NAPI/enqueue time,
	 * while mem info is valid on remote CPU.

	struct xdp_mem_info mem;
	struct net_device *dev_rx;  used by cpumap
	u32 frame_sz;
	u32 flags;  supported values defined in xdp_buff_flags
};

 tx_ring - the igb ring associated with the current cpu.


igb_xmit_xdp_ring(adapter, tx_ring, xdpf)

igb_xdp_tx_queue_mapping
 */

static struct task_struct *thread;

void initializeNetworkBuffer(struct UDPPacket *pkt)
{
	MYASSERT((void *)&(pkt->eth) - (void *)pkt == 0, "eth not aligned");
	MYASSERT((void *)&(pkt->ip) - (void *)&(pkt->eth) ==
			 sizeof(struct ethhdr),
		 "ip not aligned");
	MYASSERT((void *)&(pkt->udp) - (void *)&(pkt->ip) ==
			 sizeof(struct iphdr),
		 "udp not aligned");
	MYASSERT((void *)&(pkt->padding) - (void *)&(pkt->udp) ==
			 sizeof(struct udphdr),
		 "data not aligned");

	u8 sourceMac[ETH_ALEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
	u8 destMac[ETH_ALEN] = { 0xd2, 0x64, 0x11, 0xc3, 0x95, 0x51 };
	for (int i = 0; i < ETH_ALEN; ++i) {
		pkt->eth.h_source[i] = sourceMac[i];
		pkt->eth.h_dest[i] = destMac[i];
	}

	//        assert(pkt->eth.h_dest[0] == 0 && pkt->eth.h_dest[1] == 0 && pkt->eth.h_dest[2] == 0 && pkt->eth.h_dest[3] == 0);
	//        assert(pkt->eth.h_source[0] == 0 && pkt->eth.h_source[1] == 0 && pkt->eth.h_source[2] == 0 && pkt->eth.h_source[3] == 0);
	pkt->eth.h_proto = htons(ETH_P_IP);
	pkt->ip.ihl = 5;
	pkt->ip.version = 4;
	pkt->ip.frag_off = 0;
	pkt->ip.ttl = 0x80;
	pkt->ip.protocol = 17;
	pkt->ip.tot_len =
		htons(sizeof(struct UDPPacket) - sizeof(struct ethhdr));

	pkt->ip.tos = 0; // TODO - set to higher value
	pkt->ip.id = htons(1);
	pkt->ip.check = 0;

	u8 sourceIPBytes[4] = { 192, 168, 100, 2 };
	u8 destIPBytes[4] = { 192, 168, 100, 1 };
	const u32 sourceIP = *(const u32 *)(sourceIPBytes);
	const u32 destIP = *(const u32 *)(destIPBytes);
	pkt->ip.saddr = sourceIP;
	pkt->ip.daddr = destIP;

	const u8 *dataptr = (u8 *)(&pkt->ip);
	const u16 kernelcsum = ip_fast_csum(dataptr, pkt->ip.ihl);

	pkt->ip.check = kernelcsum;

	int udpPacketSz = sizeof(struct UDPPacket) - sizeof(struct ethhdr) -
			  sizeof(struct iphdr);
	pkt->udp.len = htons(udpPacketSz);
	pkt->udp.check = 0;
	pkt->udp.dest = htons(4321);
	pkt->udp.source = htons(1234);

	const char *msg = "Hello World!\n";
	int msgLen = strlen(msg);
	strscpy(pkt->data, msg, msgLen);

	pkt->udp.check = 0;
}

void prepareWrite(int cpu, struct igb_ring* tx_ring) {
	int cpu = smp_processor_id();
	struct netdev_queue *nq = txring_txq(tx_ring);
	MYASSERT(nq, "No associated netdev queue");

	__netif_tx_lock(nq, cpu);

	MYASSERT(!(test_bit(__IGB_DOWN, &adapter->state)), "IGB Down");
	MYASSERT(!(test_bit(__IGB_TESTING, &adapter->state)), "IGB Testing");
	MYASSERT(!(test_bit(__IGB_RESETTING, &adapter->state)),
		 "IGB Resetting");
	MYASSERT(!(test_bit(__IGB_PTP_TX_IN_PROGRESS, &adapter->state)),
		 "PTP_TX_IN_PROGRESS");
}

struct UDPPacket* getPacketBuffer(void* dmaSendBuffer) {
	return (struct UDPPacket*) dmaSendBuffer;
}

void preparePacket(struct UDPPacket* pkt) {
	int i = 9;
	pkt->data[0] = (i % 10) + '0';
}

void completeTx(struct igb_ring *tx_ring, void *pkt) {
	int dataSz = sizeof(struct UDPPacket);


	int txdIdx = tx_ring->next_to_use;
	int txdUseCount = 2;

	MYASSERT(igb_desc_unused(tx_ring) >= txdUseCount + 2,
		 "No remaining txd");

	struct igb_tx_buffer *tx_head = &tx_ring->tx_buffer_info[txdIdx];
	struct igb_tx_buffer *tx_buffer = tx_head;
	union e1000_adv_tx_desc *tx_desc = IGB_TX_DESC(tx_ring, txdIdx);
	pr_info("Preparing packet txIx %d", txdIdx);

	void *data = pkt;

	tx_head->bytecount = dataSz;
	tx_head->gso_segs = 1;
	tx_head->xdpf =
		NULL; // TODO - make sure this is not used anywhere else.
	u32 olinfo_status = tx_head->bytecount << E1000_ADVTXD_PAYLEN_SHIFT;
	// TODO - this should be set but the ctx_dx bit is not set in the flags for some reason
	olinfo_status |= tx_ring->reg_idx << 4;
	tx_desc->read.olinfo_status = cpu_to_le32(olinfo_status);

	// TODO - configure and use dca instead.
	pr_info("DMAing Data %d", dataSz);
	dma_addr_t dma;
	dma = dma_map_single(tx_ring->dev, data, dataSz, DMA_TO_DEVICE);
	if (dma_mapping_error(tx_ring->dev, dma))
		MYASSERT(false, "DMA mapping Error");

	/* record length, and DMA address */
	dma_unmap_len_set(tx_buffer, len, dataSz);
	dma_unmap_addr_set(tx_buffer, dma, dma);
	// TODO: ZC update to a data location?

	int cmd_type = E1000_ADVTXD_DTYP_DATA | E1000_ADVTXD_DCMD_DEXT |
		       E1000_ADVTXD_DCMD_IFCS | dataSz;

	tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type);
	tx_desc->read.buffer_addr = cpu_to_le64(dma);

	tx_buffer->protocol = 0;
	tx_desc->read.cmd_type_len |= cpu_to_le32(IGB_TXD_DCMD);

	netdev_tx_sent_queue(txring_txq(tx_ring), tx_head->bytecount);

	tx_head->time_stamp = jiffies;
	smp_wmb();

	MYASSERT(txdIdx == tx_ring->next_to_use,
		 "Race condition - index doesn't match next to use");
	int nextIdx = (txdIdx + 1) % tx_ring->count;
	tx_head->next_to_watch = tx_desc;
	tx_ring->next_to_use = nextIdx;
	wmb();
	pr_info("Writing tail register to trigger send: dma %llu", dma);
	writel(nextIdx, tx_ring->tail);
	// TODO What does igb_xdp_ring_update_tail do?
	__netif_tx_unlock(nq);
}



static int thread_function(void *threadData)
{
	const char *dev_name = "enp0s3";
	struct net_device *dev;
	dev = dev_get_by_name(&init_net, dev_name);

	if (!dev) {
		pr_err("JoshStat: Failed to get net dev [%s]\n", dev_name);
		// TODO - what is the correct return code over here?
		return PTR_ERR(thread);
	}

	struct igb_adapter *adapter;
	adapter = netdev_priv(dev);
	if (!adapter) {
		pr_err("JoshStat: Failed to get adapter [%s]\n", dev_name);
		// TODO - what is the correct return code over here?
		return PTR_ERR(thread);
	}
	int cpu = smp_processor_id();
	// TODO MYASSERT(cpu == PINNED_CPU);

	MYASSERT(adapter->num_tx_queues <= 16,
		 "Unexpected, number of tx queus more than 16");
	int tx_ring_idx = cpu % adapter->num_tx_queues;
	// TODO: Use IGB_PTP_TX_IN_PROGRESS
	struct igb_ring *tx_ring = adapter->tx_ring[tx_ring_idx];

	pr_info("Device Info: cpu qvec: %p, qvecs %d, tx %d, rx %d, min_frame %d, max_frame %d",
		tx_ring->q_vector, adapter->num_q_vectors,
		adapter->num_tx_queues, adapter->num_rx_queues,
		adapter->min_frame_size, adapter->max_frame_size);
	MYASSERT(adapter->num_q_vectors ==
			 adapter->num_rx_queues + adapter->num_tx_queues,
		 "num interrupts doesn't tx + rx queues");
	MYASSERT(adapter->min_frame_size <= sizeof(struct UDPPacket),
		 "min frame size too large");
	MYASSERT(adapter->max_frame_size >= sizeof(struct UDPPacket),
		 "max frame size too small");
	/*
    What are these fields?
    	u16 mng_vlan_id;
	u32 bd_number;
	u32 wol;
	u32 en_mng_pt;
	u16 link_speed;
	u16 link_duplex;
  */

	// TODO assert num rings is the same as number of cpus
	pr_info("Running on cpu %d", cpu);
	MYASSERT(cpu >= 0 && cpu <= 0, "Unexpected large cpu");

	// TODO - what is the implication of this?


	// TODO - why is this not set? The driver does set this when creating a q vector. This ctx is supposed to indicate that there is a unique tx q per interrupt.
	//  MYASSERT(test_bit(IGB_RING_FLAG_TX_CTX_IDX, &tx_ring->flags), "No ctx");

	size_t total_buffer_size = sizeof(struct UDPPacket) * MSG_COUNT;
	MYASSERT(total_buffer_size < PAGE_SIZE,
		 "Buffer requested more than a page size.");
	gfp_t dma_mem_flags = GFP_KERNEL;

	// TODO - use kmem_cache_create/alloc when doing multiple allocations instead
	void *dmaSendBuffer = kmalloc(total_buffer_size, dma_mem_flags);
	MYASSERT(dmaSendBuffer, "Could not allocate a buffer for sending data");

	struct UDPPacket *packets = (struct UDPPacket *)dmaSendBuffer;
	for (int i = 0; i < MSG_COUNT; ++i) {
		initializeNetworkBuffer(&packets[i]);
	}

	// TODO - wrap this in a c++ class.
	prepareWrite(cpu, tx_ring);
	struct UDPPacket *pkt = getPacketBuffer(dmaSendBuffer);
	preparePacket(pkt);
	completeTx(tx_ring, pkt);


	thread = NULL;

	return 0;
}

static int __init my_module_init(void)
{
	pr_info("My module loaded\n");
	thread = kthread_create(thread_function, NULL, "my_thread");
	if (IS_ERR(thread)) {
		pr_err("Failed to create thread\n");
		return PTR_ERR(thread);
	}
	wake_up_process(thread);
	return 0;
}

static void __exit my_module_exit(void)
{
	if (thread) {
		kthread_stop(thread);
		pr_info("My module unloaded\n");
	}
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple kernel module to create a kthread");
