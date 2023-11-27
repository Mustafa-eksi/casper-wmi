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

static acpi_status casper_set_all_keyboard(u8 r, u8 g, u8 b, u8 a) {
	casper_wmi_data[1].r = r;
	casper_wmi_data[1].g = g;
	casper_wmi_data[1].b = b;
	casper_wmi_data[1].a = a;
	casper_wmi_data[2] = casper_wmi_data[1];
	casper_wmi_data[3] = casper_wmi_data[1];
	char magical_input[32] = {
		0x00,0xfb,0x00,0x01,
		0x06, // Zone
		0x00,0x00,0x00,
		b, // Blue
		g, // Green
		r, // Red
		a, // Alpha
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	};
	struct acpi_buffer input = {(acpi_size)sizeof(char)*32, &magical_input};
	//struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	return wmi_set_block(CASPER_LED_BIOS_GUID, 0,
					&input);
}

static ssize_t zone_colors_show(struct device *dev, struct device_attribute *attr,
			        char *buf)
{
	printk("zone_colors_show\n");
	int count = sprintf(buf, "zone 0: #%02x%02x%02x%02x\nzone 1: #%02x%02x%02x%02x\nzone 2: #%02x%02x%02x%02x\nzone 3: #%02x%02x%02x%02x\n",
		casper_wmi_data[0].r, casper_wmi_data[0].g,
		casper_wmi_data[0].b, casper_wmi_data[0].a,
		
		casper_wmi_data[1].r, casper_wmi_data[1].g,
		casper_wmi_data[1].b, casper_wmi_data[1].a,
		
		casper_wmi_data[2].r, casper_wmi_data[2].g,
		casper_wmi_data[2].b, casper_wmi_data[2].a,
		
		casper_wmi_data[3].r, casper_wmi_data[3].g,
		casper_wmi_data[3].b, casper_wmi_data[3].a
	);
	printk("%d\n", count);
	//char *hello = "hello";
	//strncpy(buf, hello, strlen(hello));
	return count;
}

static ssize_t zone_colors_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	u64 tmp;
	int ret;
	
	ret = kstrtou64(buf, 16, &tmp);
	if(ret)
		return ret;
	
	u8 zone = (tmp>>(8*4))&0xFF;
	if(zone > 4) {
		printk("Zone doesn't exist\n");
		return 1;
	}
	printk("ret a: %0x\n",(u8) tmp&0xFF);
	if(zone == 4) {
		// apply for all zones on keyboard
		ret = casper_set_all_keyboard(
			(u8) (tmp>>(8*3))&0xFF,
			(u8) (tmp>>(8*2))&0xFF,
			(u8) (tmp>>8)&0xFF,
			(u8) tmp&0xFF
		);
		if(ret != 0) {
			printk("ACPI status: %d\n", ret);
		}
	}else {
		casper_wmi_data[zone].a = (u8) tmp&0xFF;
		casper_wmi_data[zone].r = (u8) (tmp>>(8*3))&0xFF;
		casper_wmi_data[zone].g = (u8) (tmp>>(8*2))&0xFF;
		casper_wmi_data[zone].b = (u8) (tmp>>8)&0xFF;
		ret = casper_set_single_zone(zone);
		if(ret != 0) {
			printk("ACPI status: %d\n", ret);
		}
	}	
	
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
	casper_wmi_data[0].a = (casper_wmi_data[0].a&0xF0) | (u8)brightness;
	casper_wmi_data[1].a = (casper_wmi_data[1].a&0xF0) | (u8)brightness;
	casper_wmi_data[2].a = (casper_wmi_data[2].a&0xF0) | (u8)brightness;
	casper_wmi_data[3].a = (casper_wmi_data[3].a&0xF0) | (u8)brightness;
	acpi_status ret = casper_set_single_zone(0);
	if(ret != 0) {
		printk("casper_perform_wmi returned non zero value\n");
	}
	ret = casper_set_single_zone(1);
	if(ret != 0) {
		printk("casper_perform_wmi returned non zero value\n");
	}
	printk("set_brightness. brightness: %d\n", brightness);
}

enum led_brightness get_casper_backlight_brightness(struct led_classdev *led_cdev)
{
	//printk("a: %d\n", casper_wmi_data[3].a);
	u8 brightness = casper_wmi_data[3].a&0x0F;
	if(brightness == 0)
		return 0;
	else if(brightness == 1)
		return 1;
	else
		return 2;
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
