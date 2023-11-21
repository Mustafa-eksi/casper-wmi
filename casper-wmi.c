#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mustafa Ekşi");
MODULE_DESCRIPTION("A basic WMI driver for Casper Excalibur Laptop's keyboard backlights.");


int load_module(void) 
{ 
    printk("Hello world.\n"); 
    return 0; 
} 

void unload_module(void) 
{ 
    pr_info("Goodbye world.\n"); 
} 

module_init(load_module); 
module_exit(unload_module);
