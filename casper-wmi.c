#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/acpi.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mustafa Ekşi");
MODULE_DESCRIPTION(
	"A basic WMI driver for Casper Excalibur Laptop's keyboard backlights.");

#define CASPER_EXCALIBUR_WMI_OBJECT_GUID "644C5791-B7B0-4123-A90B-E93876E0DAAD"

static int __init load_module(void) 
{
	if (!wmi_has_guid(CASPER_EXCALIBUR_WMI_OBJECT_GUID)) {
	    printk("No known WMI GUID found\n");
	    return -ENODEV;
	}
	char magical_input[32] = {0x00,0xfb,0x00,0x01,0x06,0x00,0x00,0x00,0x11,0x40,0xff,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	struct acpi_buffer input = {(acpi_size)sizeof(char)*32, &magical_input};
	//struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	wmi_set_block(CASPER_EXCALIBUR_WMI_OBJECT_GUID, 0,
					&input);
	return 0;
} 

void unload_module(void) 
{ 
	pr_info("Goodbye world.\n"); 
} 

module_init(load_module); 
module_exit(unload_module);
