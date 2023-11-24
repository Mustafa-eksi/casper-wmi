#include <linux/acpi.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mustafa Ekşi");
MODULE_DESCRIPTION(
	"A basic WMI driver for Casper Excalibur Laptop's keyboard backlights.");
	
#define CASPER_LED_BIOS_GUID "644C5791-B7B0-4123-A90B-E93876E0DAAD"

static int casper_led_perform_fn(u8 region, u8 red, u8 green,
			       u8 blue, u8 alpha)
{
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	struct bios_args *bios_return;
	struct acpi_buffer input;
	union acpi_object *obj;
	acpi_status status;
	u8 return_code;

	struct bios_args args = {
		.length = length,
		.result_code = result_code,
		.device_id = device_id,
		.command = command,
		.on_time = on_time,
		.off_time = off_time
	};

	input.length = sizeof(struct bios_args);
	input.pointer = &args;

	status = wmi_evaluate_method(DELL_LED_BIOS_GUID, 0, 1, &input, &output);
	if (ACPI_FAILURE(status))
		return status;

	obj = output.pointer;

	if (!obj)
		return -EINVAL;
	if (obj->type != ACPI_TYPE_BUFFER) {
		kfree(obj);
		return -EINVAL;
	}

	bios_return = ((struct bios_args *)obj->buffer.pointer);
	return_code = bios_return->result_code;

	kfree(obj);

	return return_code;
}

static int __init casper_led_init(void)
{
	int error = 0;

	if (!wmi_has_guid(CASPER_LED_BIOS_GUID))
		return -ENODEV;

	error = led_off();
	if (error != 0)
		return -ENODEV;

	return led_classdev_register(NULL, &dell_led);
}

static void __exit casper_led_exit(void)
{
	led_classdev_unregister(&dell_led);

	led_off();
}

module_init(casper_led_init); 
module_exit(casper_led_exit);
