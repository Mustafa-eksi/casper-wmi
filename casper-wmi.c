#include <linux/acpi.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/wmi.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mustafa Ekşi");
MODULE_DESCRIPTION(
	"A basic WMI driver for Casper Excalibur Laptop's keyboard backlights.");
	
#define CASPER_LED_BIOS_GUID "644C5791-B7B0-4123-A90B-E93876E0DAAD"

//static void casper_wmi_set_block() {
	
//}

struct zone_data {
	u8 r, g, b, a;
};

struct zone_data casper_wmi_data[4] = {0};

static acpi_status casper_set_single_zone(size_t zone_id) {
	char magical_input[32] = {
		0x00,0xfb,0x00,0x01,
		zone_id == 0 ? 0x07 : zone_id+2, // Zone
		0x00,0x00,0x00,
		casper_wmi_data[zone_id].b, // Blue
		casper_wmi_data[zone_id].g, // Green
		casper_wmi_data[zone_id].r, // Red
		casper_wmi_data[zone_id].a, // Alpha
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	};
	struct acpi_buffer input = {(acpi_size)sizeof(char)*32, &magical_input};
	//struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	return wmi_set_block(CASPER_LED_BIOS_GUID, 0,
					&input);
}

static acpi_status casper_perform_wmi(void) {
	for(size_t i = 0; i<4; i++) {
		acpi_status ret = casper_set_single_zone(i);
		if(ret != 0)
			return ret;
	}
	return 0;
}

static ssize_t zone_colors_show(struct device *dev, struct device_attribute *attr,
			        char *buf)
{
	//int value = hp_wmi_read_int(HPWMI_DISPLAY_QUERY);

	//if (value < 0)
	//	return value;
	return 0;
	
}

static ssize_t zone_colors_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	u64 tmp;
	int ret;
	
	ret = kstrtou64(buf, 16, &tmp);
	// 0x 12 34 56 - 78 90 ab cd ef
	u8 zone = (tmp>>(8*4))&0xFF;
	if(zone > 3) {
		printk("Zone doesn't exist\n");
		return 1;
	}
	casper_wmi_data[zone].a = (u8) tmp&0xFF;
	casper_wmi_data[zone].r = (u8) (tmp>>(8*3))&0xFF;
	casper_wmi_data[zone].g = (u8) (tmp>>(8*2))&0xFF;
	casper_wmi_data[zone].b = (u8) (tmp>>8)&0xFF;
	casper_set_single_zone(zone);
	//printk("tmp: zone: %0x, r: %0x, g: %0x, b: %0x, a: %0x\n", (u8)(tmp>>(8*4))&0xFF, (u8) (tmp>>(8*3))&0xFF, (u8) (tmp>>(8*2))&0xFF, (u8) (tmp>>(8))&0xFF, (u8) tmp&0xFF);
	return count;
}

static DEVICE_ATTR_RW(zone_colors);

static struct attribute *casper_kbd_led_attrs[] = {
	&dev_attr_zone_colors.attr,
	NULL,
};
ATTRIBUTE_GROUPS(casper_kbd_led);

struct casper_kbd_backlights_data {
	u8 zone, red, green, blue, alpha_and_pattern;
};

void set_casper_backlight_brightness(struct led_classdev *led_cdev,
					  enum led_brightness brightness)
{
	casper_wmi_data[0].a = (u8)brightness;
	casper_wmi_data[1].a = (u8)brightness;
	casper_wmi_data[2].a = (u8)brightness;
	casper_wmi_data[3].a = (u8)brightness;
	acpi_status ret = casper_perform_wmi();
	if(ret != 0) {
		printk("casper_perform_wmi returned non zero value\n");
	}
	printk("set_brightness. brightness: %d\n", brightness);
}

enum led_brightness get_casper_backlight_brightness(struct led_classdev *led_cdev)
{
	return 1;
}

static struct led_classdev casper_kbd_led = {
	.name = "casper_excalibur::kbd_backlight",
	.brightness	= 0,
	.brightness_set = set_casper_backlight_brightness,
	.brightness_get = get_casper_backlight_brightness,
	.max_brightness = 2,
	.groups = casper_kbd_led_groups,
};

static int __init casper_led_init(void)
{
	if (!wmi_has_guid(CASPER_LED_BIOS_GUID))
		return -ENODEV;
	
	if(casper_perform_wmi() != 0) {
		printk("casper_perform_wmi returned non zero value\n");
	}
	return led_classdev_register(NULL, &casper_kbd_led);
}

static void __exit casper_led_exit(void)
{
	led_classdev_unregister(&casper_kbd_led);
}

module_init(casper_led_init); 
module_exit(casper_led_exit);
