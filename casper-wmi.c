#include <linux/acpi.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/wmi.h>
#include <linux/device.h>
#include <linux/dev_printk.h>

MODULE_AUTHOR("Mustafa Ekşi <mustafa.eskieksi@gmail.com>");
MODULE_DESCRIPTION("Casper Excalibur Laptop keyboard backlight driver");
MODULE_LICENSE("GPL");

#define CASPER_EXCALIBUR_WMI_GUID "644C5791-B7B0-4123-A90B-E93876E0DAAD"

#define CASPER_KEYBOARD_LED_1 0x03
#define CASPER_KEYBOARD_LED_2 0x04
#define CASPER_KEYBOARD_LED_3 0x05
#define CASPER_ALL_KEYBOARD_LEDS 0x06
#define CASPER_CORNER_LEDS 0x07

// Found these values with reverse engineering, other values are possible
// but I don't know what they do.
#define CASPER_LED_A0 0xfb00
#define CASPER_LED_A1 0x0100

struct casper_wmi_args {
	u16 a0, a1;
	u32 a2, a3;
};

// MARRGGBB
// M: Mode, 0 and 1 is normal, up to 6 (2: blinking, 3: fade-out fade-in,
// 					4: heartbeat, 5: repeat, 6: random)
// A: alpha, R: red, G: green, B: blue
u32 casper_led_data[4] = {0};

/*
 * Function to set led value of specified zone, zone id should be between 3 and 7.
 * */
static acpi_status casper_set_backlight(u8 zone_id, u32 data) {
	struct casper_wmi_args wmi_args = {0};
	wmi_args.a0 = CASPER_LED_A0;
	wmi_args.a1 = CASPER_LED_A1;
	wmi_args.a2 = zone_id;
	wmi_args.a3 = data;
	
	struct acpi_buffer input = {
		(acpi_size)sizeof(struct casper_wmi_args), 
		&wmi_args
	};
	return wmi_set_block(CASPER_EXCALIBUR_WMI_GUID, 0,
					&input);
}

static ssize_t led_control_show(struct device *dev, struct device_attribute 
				*attr, char *buf)
{
	return sprintf(buf, 
"led 0: #%08x\n\
led 1: #%08x\n\
led 2: #%08x\n\
corner leds: #%08x\n",
		casper_led_data[0], casper_led_data[1], casper_led_data[2],
		casper_led_data[3]
	);
}

/*
 * input should start with zone id and then u32 led data (same as casper_led_data).
 * */
static ssize_t led_control_store(struct device *dev, struct device_attribute
					*attr, const char *buf, size_t count)
{
	u64 tmp;
	int ret;
	
	ret = kstrtou64(buf, 16, &tmp);
	if(ret)
		return ret;
	
	u8 led_zone = (tmp>>(8*4))&0xFF;
	if( CASPER_KEYBOARD_LED_1 > led_zone || led_zone > CASPER_CORNER_LEDS) {
		dev_err(dev, "led_control_store: this led zone doesn't exist\n");
		return -1;
	}
	ret = casper_set_backlight(
		led_zone,
		(u32) (tmp&0xFFFFFFFF)
	);
	if(ret != 0) {
		dev_err(dev, "casper-wmi ACPI status: %d\n", ret);
		return ret;
	}
	if(led_zone == CASPER_CORNER_LEDS){
		casper_led_data[3] = (u32) (tmp&0xFFFFFFFF);
	}else if(led_zone == CASPER_ALL_KEYBOARD_LEDS) {
		casper_led_data[0] = (u32) (tmp&0xFFFFFFFF);
		casper_led_data[1] = (u32) (tmp&0xFFFFFFFF);
		casper_led_data[2] = (u32) (tmp&0xFFFFFFFF);
	}else {
		casper_led_data[led_zone - CASPER_KEYBOARD_LED_1] =
							(u32) (tmp&0xFFFFFFFF);
	}
	return count;
}

static DEVICE_ATTR_RW(led_control);

static struct attribute *casper_kbd_led_attrs[] = {
	&dev_attr_led_control.attr,
	NULL,
};

ATTRIBUTE_GROUPS(casper_kbd_led);

/*
 * Sets brightness of all leds.
 * */
void set_casper_backlight_brightness(struct led_classdev *led_cdev,
					  enum led_brightness brightness)
{
	casper_led_data[0] = (casper_led_data[0] & 0xF0FFFFFF) 
		| ((u32) brightness)<<24;
	casper_led_data[1] = (casper_led_data[1] & 0xF0FFFFFF) 
		| ((u32) brightness)<<24;
	casper_led_data[2] = (casper_led_data[2] & 0xF0FFFFFF) 
		| ((u32) brightness)<<24;
	
	// Setting any of the keyboard leds' brightness sets brightness of all
	acpi_status ret = casper_set_backlight(
		CASPER_KEYBOARD_LED_1,
		casper_led_data[0]
	);
	
	if(ret != 0) {
		dev_err(led_cdev->dev, "Couldn't set brightness acpi status: %d\n", ret);
		return;
	}
	
	casper_led_data[3] = (casper_led_data[3] & 0xF0FFFFFF) | ((u32) brightness)<<24;
	ret = casper_set_backlight(
		CASPER_CORNER_LEDS,
		casper_led_data[3]
	);
	if(ret != 0)
		dev_err(led_cdev->dev, "Couldn't set brightness acpi status: %d\n", ret);
}

// Corner leds' brightness can be different from keyboard leds' but this is discarded.
enum led_brightness get_casper_backlight_brightness(
				struct led_classdev *led_cdev)
{
	return casper_led_data[0]&0x0F000000;
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
	if (!wmi_has_guid(CASPER_EXCALIBUR_WMI_GUID))
		return -ENODEV;
	return led_classdev_register(NULL, &casper_kbd_led);
}

static void __exit casper_led_exit(void)
{
	led_classdev_unregister(&casper_kbd_led);
}

module_init(casper_led_init); 
module_exit(casper_led_exit);
