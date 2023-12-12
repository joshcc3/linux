#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/udp.h>
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


static int thread_function(void *data) {
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
  MYASSERT(adapter->num_q_vectors == adapter->num_tx_queues, "num interrupts doesn't tx match queues");
  MYASSERT(adapter->num_q_vectors == adapter->num_rx_queues, "num interrupts doesn't rx match queues");  
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
  struct UDPPacket pkt;
  const char* msg = "Hello World!\n";
  int msgLen = strlen(msg);
  strscpy(pkt.data, msg, msgLen);
  
  MYASSERT(test_bit(IGB_RING_FLAG_TX_CTX_IDX, &tx_ring->flags), "No ctx");

  for(int i = 0; i < MSG_COUNT && !kthread_should_stop(); ++i) {
       pr_info("Preparing packet");
       pkt.data[0] = (i % 10) + '0';
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
       void* data = &pkt;

       tx_head->bytecount = dataSz;
       tx_head->gso_segs = 1;
       tx_head->xdpf = NULL; // TODO - make sure this is not used anywhere else.
       u32 olinfo_status = tx_head->bytecount << E1000_ADVTXD_PAYLEN_SHIFT;
       olinfo_status |= tx_ring->reg_idx << 4;
       tx_desc->read.olinfo_status = cpu_to_le32(olinfo_status);

       pr_info("DMAing Data");       
       dma_addr_t dma;
       dma = dma_map_single(tx_ring->dev, data, dataSz, DMA_TO_DEVICE);
       if (dma_mapping_error(tx_ring->dev, dma))
	 MYASSERT(false, "DMA mapping Error");
       
       /* record length, and DMA address */
       dma_unmap_len_set(tx_buffer, len, dataSz);
       dma_unmap_addr_set(tx_buffer, dma, dma);
       // TODO: ZC update to a data location?
       
       tx_buffer->protocol = 0;
       
       tx_head->time_stamp = jiffies;
       smp_wmb();
       
       MYASSERT(txdIdx == tx_ring->next_to_use, "Race condition - index doesn't match next to use");
       int nextIdx = (txdIdx + 1) & tx_ring->count;
       tx_head->next_to_watch = tx_desc;
       tx_ring->next_to_use = nextIdx % tx_ring->count;

       pr_info("Writing tail register to trigger send");
       writel(nextIdx, tx_ring->tail);

       // TODO What does igb_xdp_ring_update_tail do?
  }
  __netif_tx_unlock(nq);

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
