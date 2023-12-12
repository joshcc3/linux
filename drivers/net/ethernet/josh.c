#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "intel/igb/igb.h"

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

  pr_info("Device Info: tx %d, rx %d, min_frame %d, max_frame %d",
	  adapter->num_tx_queues,
	  adapter->num_rx_queues,
	  adapter->min_frame_size,
	  adapter->max_frame_size);
  
  while (!kthread_should_stop()) {
       pr_info("Hello\n");
        ssleep(5);
    }
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
