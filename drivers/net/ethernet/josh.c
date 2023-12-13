#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "josh.h"



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

/* igb_dump - Print registers, Tx-rings and Rx-rings */
void igb_dump(struct igb_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	//	struct e1000_hw *hw = &adapter->hw;
	//	struct igb_reg_info *reginfo;
	struct igb_ring *tx_ring;
	union e1000_adv_tx_desc *tx_desc;
	struct my_u0 { __le64 a; __le64 b; } *u0;
	struct igb_ring *rx_ring;
	union e1000_adv_rx_desc *rx_desc;
	u32 staterr;
	u16 i, n;

	//	if (!netif_msg_hw(adapter))
	//return;

	/* Print netdevice Info */
	if (netdev) {
		dev_info(&adapter->pdev->dev, "Net device Info\n");
		pr_info("Device Name     state            trans_start\n");
		pr_info("%-15s %016lX %016lX\n", netdev->name,
			netdev->state, dev_trans_start(netdev));
	}

	/* Print Registers 
	dev_info(&adapter->pdev->dev, "Register Dump\n");
	pr_info(" Register Name   Value\n");
	for (reginfo = (struct igb_reg_info *)igb_reg_info_tbl;
	     reginfo->name; reginfo++) {
		igb_regdump(hw, reginfo);
		}*/

	/* Print TX Ring Summary */
	if (!netdev || !netif_running(netdev))
		goto exit;

	dev_info(&adapter->pdev->dev, "TX Rings Summary\n");
	pr_info("Queue [NTU] [NTC] [bi(ntc)->dma  ] leng ntw timestamp\n");
	for (n = 0; n < adapter->num_tx_queues; n++) {
		struct igb_tx_buffer *buffer_info;
		tx_ring = adapter->tx_ring[n];
		buffer_info = &tx_ring->tx_buffer_info[tx_ring->next_to_clean];
		pr_info(" %5d %5X %5X %016llX %04X %p %016llX\n",
			n, tx_ring->next_to_use, tx_ring->next_to_clean,
			(u64)dma_unmap_addr(buffer_info, dma),
			dma_unmap_len(buffer_info, len),
			buffer_info->next_to_watch,
			(u64)buffer_info->time_stamp);
	}

	/* Print TX Rings */
	//	if (!netif_msg_tx_done(adapter))
	//		goto rx_ring_summary;

	dev_info(&adapter->pdev->dev, "TX Rings Dump\n");

	/* Transmit Descriptor Formats
	 *
	 * Advanced Transmit Descriptor
	 *   +--------------------------------------------------------------+
	 * 0 |         Buffer Address [63:0]                                |
	 *   +--------------------------------------------------------------+
	 * 8 | PAYLEN  | PORTS  |CC|IDX | STA | DCMD  |DTYP|MAC|RSV| DTALEN |
	 *   +--------------------------------------------------------------+
	 *   63      46 45    40 39 38 36 35 32 31   24             15       0
	 */

	for (n = 0; n < adapter->num_tx_queues; n++) {
		tx_ring = adapter->tx_ring[n];
		pr_info("------------------------------------\n");
		pr_info("TX QUEUE INDEX = %d\n", tx_ring->queue_index);
		pr_info("------------------------------------\n");
		pr_info("T [desc]     [address 63:0  ] [PlPOCIStDDM Ln] [bi->dma       ] leng  ntw timestamp        bi->skb\n");

		for (i = 0; tx_ring->desc && (i < tx_ring->count); i++) {
			const char *next_desc;
			struct igb_tx_buffer *buffer_info;
			tx_desc = IGB_TX_DESC(tx_ring, i);
			buffer_info = &tx_ring->tx_buffer_info[i];
			u0 = (struct my_u0 *)tx_desc;
			if (i == tx_ring->next_to_use &&
			    i == tx_ring->next_to_clean)
				next_desc = " NTC/U";
			else if (i == tx_ring->next_to_use)
				next_desc = " NTU";
			else if (i == tx_ring->next_to_clean)
				next_desc = " NTC";
			else
				next_desc = "";

			pr_info("T [0x%03X]    %016llX %016llX %016llX %04X  %p %016llX %p%s\n",
				i, le64_to_cpu(u0->a),
				le64_to_cpu(u0->b),
				(u64)dma_unmap_addr(buffer_info, dma),
				dma_unmap_len(buffer_info, len),
				buffer_info->next_to_watch,
				(u64)buffer_info->time_stamp,
				buffer_info->skb, next_desc);

			if (netif_msg_pktdata(adapter) && buffer_info->skb)
				print_hex_dump(KERN_INFO, "",
					DUMP_PREFIX_ADDRESS,
					16, 1, buffer_info->skb->data,
					dma_unmap_len(buffer_info, len),
					true);
		}
	}

	/* Print RX Rings Summary */
	//rx_ring_summary:
	dev_info(&adapter->pdev->dev, "RX Rings Summary\n");
	pr_info("Queue [NTU] [NTC]\n");
	for (n = 0; n < adapter->num_rx_queues; n++) {
		rx_ring = adapter->rx_ring[n];
		pr_info(" %5d %5X %5X\n",
			n, rx_ring->next_to_use, rx_ring->next_to_clean);
	}

	/* Print RX Rings */
	if (!netif_msg_rx_status(adapter))
		goto exit;

	dev_info(&adapter->pdev->dev, "RX Rings Dump\n");

	/* Advanced Receive Descriptor (Read) Format
	 *    63                                           1        0
	 *    +-----------------------------------------------------+
	 *  0 |       Packet Buffer Address [63:1]           |A0/NSE|
	 *    +----------------------------------------------+------+
	 *  8 |       Header Buffer Address [63:1]           |  DD  |
	 *    +-----------------------------------------------------+
	 *
	 *
	 * Advanced Receive Descriptor (Write-Back) Format
	 *
	 *   63       48 47    32 31  30      21 20 17 16   4 3     0
	 *   +------------------------------------------------------+
	 * 0 | Packet     IP     |SPH| HDR_LEN   | RSV|Packet|  RSS |
	 *   | Checksum   Ident  |   |           |    | Type | Type |
	 *   +------------------------------------------------------+
	 * 8 | VLAN Tag | Length | Extended Error | Extended Status |
	 *   +------------------------------------------------------+
	 *   63       48 47    32 31            20 19               0
	 */

	for (n = 0; n < adapter->num_rx_queues; n++) {
		rx_ring = adapter->rx_ring[n];
		pr_info("------------------------------------\n");
		pr_info("RX QUEUE INDEX = %d\n", rx_ring->queue_index);
		pr_info("------------------------------------\n");
		pr_info("R  [desc]      [ PktBuf     A0] [  HeadBuf   DD] [bi->dma       ] [bi->skb] <-- Adv Rx Read format\n");
		pr_info("RWB[desc]      [PcsmIpSHl PtRs] [vl er S cks ln] ---------------- [bi->skb] <-- Adv Rx Write-Back format\n");

		for (i = 0; i < rx_ring->count; i++) {
			const char *next_desc;
			struct igb_rx_buffer *buffer_info;
			buffer_info = &rx_ring->rx_buffer_info[i];
			rx_desc = IGB_RX_DESC(rx_ring, i);
			u0 = (struct my_u0 *)rx_desc;
			staterr = le32_to_cpu(rx_desc->wb.upper.status_error);

			if (i == rx_ring->next_to_use)
				next_desc = " NTU";
			else if (i == rx_ring->next_to_clean)
				next_desc = " NTC";
			else
				next_desc = "";

			if (staterr & E1000_RXD_STAT_DD) {
				/* Descriptor Done */
				pr_info("%s[0x%03X]     %016llX %016llX ---------------- %s\n",
					"RWB", i,
					le64_to_cpu(u0->a),
					le64_to_cpu(u0->b),
					next_desc);
			} else {
				pr_info("%s[0x%03X]     %016llX %016llX %016llX %s\n",
					"R  ", i,
					le64_to_cpu(u0->a),
					le64_to_cpu(u0->b),
					(u64)buffer_info->dma,
					next_desc);

				if (netif_msg_pktdata(adapter) &&
				    buffer_info->dma && buffer_info->page) {
					print_hex_dump(KERN_INFO, "",
					  DUMP_PREFIX_ADDRESS,
					  16, 1,
					  page_address(buffer_info->page) +
						      buffer_info->page_offset,
					  igb_rx_bufsz(rx_ring), true);
				}
			}
		}
	}

exit:
	return;
}




void initializeNetworkBuffer(struct UDPPacket* pkt) {
  MYASSERT((void*)&(pkt->eth) - (void*)pkt == 0, "eth not aligned");
  MYASSERT((void*)&(pkt->ip) - (void*)&(pkt->eth) == sizeof(struct ethhdr), "ip not aligned");
  MYASSERT((void*)&(pkt->udp) - (void*)&(pkt->ip) == sizeof(struct iphdr), "udp not aligned");
  MYASSERT((void*)&(pkt->padding) - (void*)&(pkt->udp) == sizeof(struct udphdr), "data not aligned");
  
  u8 sourceMac[ETH_ALEN] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
  u8 destMac [ETH_ALEN] = {0xd2, 0x64, 0x11, 0xc3, 0x95, 0x51};
  for(int i = 0; i < ETH_ALEN; ++i) {
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
  pkt->ip.tot_len = htons(sizeof(struct UDPPacket) - sizeof(struct ethhdr));

  pkt->ip.tos = 0; // TODO - set to higher value
  pkt->ip.id = htons(1);
  pkt->ip.check = 0;

  u8 sourceIPBytes[4] = {192, 168, 100, 2};
  u8 destIPBytes[4] = {192, 168, 100, 1};
  const u32 sourceIP = *(const u32*)(sourceIPBytes);
  const u32 destIP = *(const u32*)(destIPBytes);
  pkt->ip.saddr = sourceIP;
  pkt->ip.daddr = destIP;

  const u8* dataptr = (u8 *)(&pkt->ip);
 const u16 kernelcsum = ip_fast_csum(dataptr, pkt->ip.ihl);

  pkt->ip.check = kernelcsum;

  int udpPacketSz = sizeof(struct UDPPacket) - sizeof(struct ethhdr) - sizeof(struct iphdr);
  pkt->udp.len = htons(udpPacketSz);
  pkt->udp.check = 0;
  pkt->udp.dest = htons(4321);
  pkt->udp.source = htons(1234);

  const char* msg = "Hello World!\n";
  int msgLen = strlen(msg);
  strscpy(pkt->data, msg, msgLen);

  pkt->udp.check = 0;
  
}


static int thread_function(void *threadData) {
  const char* dev_name = "enp0s3";
  struct net_device* dev;
  dev = dev_get_by_name(&init_net, dev_name);

  if(!dev) {
    pr_err("JoshStat: Failed to get net dev [%s]\n", dev_name);
    // TODO - what is the correct return code over here?
    return PTR_ERR(thread);
  }

  struct igb_adapter* adapter;
  adapter = netdev_priv(dev);
  if(!adapter) {
    pr_err("JoshStat: Failed to get adapter [%s]\n", dev_name);
    // TODO - what is the correct return code over here?
    return PTR_ERR(thread);    
  }
  pr_info("Device Info: qvecs %d, tx %d, rx %d, min_frame %d, max_frame %d",
	  adapter->num_q_vectors,
	  adapter->num_tx_queues,
	  adapter->num_rx_queues,
	  adapter->min_frame_size,
	  adapter->max_frame_size);
  MYASSERT(adapter->num_q_vectors == adapter->num_rx_queues + adapter->num_tx_queues, "num interrupts doesn't tx + rx queues");
  MYASSERT(adapter->min_frame_size <= sizeof(struct UDPPacket), "min frame size too large");
  MYASSERT(adapter->max_frame_size >= sizeof(struct UDPPacket), "max frame size too small");  
  /*
    What are these fields?
    	u16 mng_vlan_id;
	u32 bd_number;
	u32 wol;
	u32 en_mng_pt;
	u16 link_speed;
	u16 link_duplex;
  */
  
  int cpu = smp_processor_id();
  // TODO MYASSERT(cpu == PINNED_CPU);

  MYASSERT(adapter->num_tx_queues <= 16, "Unexpected, number of tx queus more than 16");
  int tx_ring_idx = cpu % adapter->num_tx_queues;

  // TODO assert num rings is the same as number of cpus
  pr_info("Running on cpu %d", cpu);
  MYASSERT(cpu >= 0 && cpu <= 0, "Unexpected large cpu");
  
  // TODO: Use IGB_PTP_TX_IN_PROGRESS
  struct igb_ring *tx_ring = adapter->tx_ring[tx_ring_idx];

  struct netdev_queue *nq = txring_txq(tx_ring);
  MYASSERT(nq, "No associated netdev queue");

  // TODO - what is the implication of this?
  __netif_tx_lock(nq, cpu);

  int dataSz = sizeof(struct UDPPacket);


  // TODO - why is this not set? The driver does set this when creating a q vector. This ctx is supposed to indicate that there is a unique tx q per interrupt.
  //  MYASSERT(test_bit(IGB_RING_FLAG_TX_CTX_IDX, &tx_ring->flags), "No ctx");

  size_t total_buffer_size = sizeof(struct UDPPacket) * MSG_COUNT;
  MYASSERT(total_buffer_size < PAGE_SIZE, "Buffer requested more than a page size.");
  gfp_t dma_mem_flags = GFP_KERNEL;

  // TODO - use kmem_cache_create/alloc when doing multiple allocations instead
  void* dmaSendBuffer = kmalloc(total_buffer_size, dma_mem_flags);
  MYASSERT(dmaSendBuffer, "Could not allocate a buffer for sending data");

  struct UDPPacket *packets = (struct UDPPacket*) dmaSendBuffer;
  for(int i = 0; i < MSG_COUNT; ++i) {
    initializeNetworkBuffer(&packets[i]);
  }
  igb_dump(adapter);

  int i = 0;
  //  for(int i = 0; i < MSG_COUNT && !kthread_should_stop(); ++i) {

       struct UDPPacket* pkt = packets;
       MYASSERT(i == 0, "I");
       pkt->data[0] = (i % 10) + '0';
       MYASSERT(!(test_bit(__IGB_DOWN, &adapter->state)), "IGB Down");
       MYASSERT(!(test_bit(__IGB_TESTING, &adapter->state)), "IGB Testing");
       MYASSERT(!(test_bit(__IGB_RESETTING, &adapter->state)), "IGB Resetting");
       MYASSERT(!(test_bit(__IGB_PTP_TX_IN_PROGRESS, &adapter->state)), "PTP_TX_IN_PROGRESS");
       int txdIdx = tx_ring->next_to_use;
       int txdUseCount = 2;
       
       MYASSERT(igb_desc_unused(tx_ring) >= txdUseCount + 2, "No remaining txd");

       struct igb_tx_buffer *tx_head = &tx_ring->tx_buffer_info[txdIdx];
       struct igb_tx_buffer *tx_buffer = tx_head;
       union e1000_adv_tx_desc *tx_desc = IGB_TX_DESC(tx_ring, txdIdx);
       pr_info("Preparing packet txIx %d", txdIdx);

       void* data = pkt;

       tx_head->bytecount = dataSz;
       tx_head->gso_segs = 1;
       tx_head->xdpf = NULL; // TODO - make sure this is not used anywhere else.
       u32 olinfo_status = tx_head->bytecount << E1000_ADVTXD_PAYLEN_SHIFT;
       // TODO - this should be set but the ctx_dx bit is not set in the falgs for some reason
       olinfo_status |= tx_ring->reg_idx << 4;
       tx_desc->read.olinfo_status = cpu_to_le32(olinfo_status);

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
       
       MYASSERT(txdIdx == tx_ring->next_to_use, "Race condition - index doesn't match next to use");
       int nextIdx = (txdIdx + 1) % tx_ring->count;
       tx_head->next_to_watch = tx_desc;
       tx_ring->next_to_use = nextIdx;
       wmb();
       pr_info("Writing tail register to trigger send: dma %llu", dma);
       writel(nextIdx, tx_ring->tail);
       // TODO What does igb_xdp_ring_update_tail do?
       //  }
  __netif_tx_unlock(nq);
  igb_dump(adapter);

  thread = NULL;

  return 0;

}


static int __init my_module_init(void) {
    pr_info("My module loaded\n");
    thread = kthread_create(thread_function, NULL, "my_thread");
    if (IS_ERR(thread)) {
        pr_err("Failed to create thread\n");
        return PTR_ERR(thread);
    }
    wake_up_process(thread);
    return 0;
}

static void __exit my_module_exit(void) {
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
