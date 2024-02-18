#include <linux/acpi.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/wmi.h>
#include <linux/device.h>
#include <linux/dev_printk.h>
#include <linux/hwmon.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/mod_devicetable.h>

MODULE_AUTHOR("Mustafa Ekşi <mustafa.eskieksi@gmail.com>");
MODULE_DESCRIPTION("Casper Excalibur Laptop WMI driver");
MODULE_LICENSE("GPL");

#define CASPER_WMI_GUID "644C5791-B7B0-4123-A90B-E93876E0DAAD"

#define CASPER_KEYBOARD_LED_1 0x03
#define CASPER_KEYBOARD_LED_2 0x04
#define CASPER_KEYBOARD_LED_3 0x05
#define CASPER_ALL_KEYBOARD_LEDS 0x06
#define CASPER_CORNER_LEDS 0x07

#define CASPER_READ 0xfa00
// operations below are read only
#define CASPER_HARDWAREINFO 0x0200 // hwmon
#define CASPER_BIOSVER 0x0201    // hwmon

#define CASPER_WRITE 0xfb00
// operations below are write only
#define CASPER_LEDCTRL 0x0100      // ledclass_dev
#define CASPER_SETWINKEY 0x0200       // ??

// this is read and write
#define CASPER_POWERPLAN 0x0300           // hwmon

// I don't know why but I can't use some of new api in wmi.h but this works.
#ifndef to_wmi_device
#define to_wmi_device(device)	container_of(device, struct wmi_device, dev)
#endif

struct casper_wmi_args {
	u16 a0, a1;
	u32 a2, a3, a4, a5, a6, rev0, rev1;
};

// MARRGGBB
// M: Mode, 0 and 1 is normal, up to 6 (2: blinking, 3: fade-out fade-in,
// 					4: heartbeat, 5: repeat, 6: random)
// A: alpha, R: red, G: green, B: blue
u32 casper_led_data[4] = {0};

/*
 * Function to set led value of specified zone, zone id should be between 3 and 7.
 * */
static acpi_status casper_set(u16 a1, u8 zone_id, u32 data)
{
	struct casper_wmi_args wmi_args = {0};
	wmi_args.a0 = CASPER_WRITE;
	wmi_args.a1 = a1;
	wmi_args.a2 = zone_id;
	wmi_args.a3 = data;
	
	struct acpi_buffer input = {
		(acpi_size)sizeof(struct casper_wmi_args), 
		&wmi_args
	};
	return wmi_set_block(CASPER_WMI_GUID, 0,
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
	ret = casper_set(
		CASPER_LEDCTRL,
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
	acpi_status ret = casper_set(
		CASPER_LEDCTRL,
		CASPER_KEYBOARD_LED_1,
		casper_led_data[0]
	);
	
	if(ret != 0) {
		dev_err(led_cdev->dev, "Couldn't set brightness acpi status: %d\n", ret);
		return;
	}
	
	casper_led_data[3] = (casper_led_data[3] & 0xF0FFFFFF) | ((u32) brightness)<<24;
	ret = casper_set(
		CASPER_LEDCTRL,
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
	.name = "casper::kbd_backlight",
	.brightness	= 0,
	.brightness_set = set_casper_backlight_brightness,
	.brightness_get = get_casper_backlight_brightness,
	.max_brightness = 2,
	.groups = casper_kbd_led_groups,
};

// HWMON PART v

enum casper_power_plan {
	HIGH_POWER = 1,
	GAMING = 2,
	TEXT_MODE = 3,
	LOW_POWER = 4
};

static acpi_status casper_query(struct wmi_device *wdev, u16 a1,
				      struct casper_wmi_args *out)
{
	struct casper_wmi_args wmi_args = {0};
	wmi_args.a0 = CASPER_READ;
	wmi_args.a1 = a1;
	
	struct acpi_buffer input = {
		(acpi_size)sizeof(struct casper_wmi_args), 
		&wmi_args
	};
	acpi_status ret = wmi_set_block(CASPER_WMI_GUID, 0, &input);
	if(ACPI_FAILURE(ret)) {
		dev_err(&wdev->dev, "Could not query (set phase), acpi status: %u",
			ret);
		return ret;
	}
	
	union acpi_object *obj = wmidev_block_query(wdev, 0);
	if(obj == NULL) {
		dev_err(&wdev->dev, "Could not query (query) hardware information");
		return AE_ERROR;
	}
	if(obj->type != ACPI_TYPE_BUFFER) {
		dev_err(&wdev->dev, "Return type is not a buffer");
		return AE_TYPE;
	}
	
	if(obj->buffer.length != 32) {
		dev_err(&wdev->dev, "Return buffer is not long enough");
		return AE_ERROR;
	}
	memcpy(out, obj->buffer.pointer, 32);
	kfree(obj);
	return ret;
}


static umode_t casper_wmi_hwmon_is_visible(const void *drvdata, enum hwmon_sensor_types type,
			      u32 attr, int channel)
{
	switch(type){
	case hwmon_fan:
		return 0444; // read only
	case hwmon_pwm:
		return 0644; // read and write
	default:
		return 0;
	}
	return 0;
}

int casper_wmi_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
		    u32 attr, int channel, long *val)
{
	struct casper_wmi_args out = {0};
	switch(type) {
	case hwmon_fan:
		acpi_status ret = casper_query(to_wmi_device(dev->parent), CASPER_HARDWAREINFO,
					   &out);
		if(ACPI_FAILURE(ret)) return ret;
		if(channel == 0) { // CPU fan
			u16 cpu_fanspeed = (u16) out.a4;
			cpu_fanspeed *= (u16) 256;
			cpu_fanspeed += (u16) (out.a4 >> 8);
			*val = cpu_fanspeed;
		}
		else if(channel == 1) { // GPU fan
			u16 gpu_fanspeed = (u16) out.a5;
			gpu_fanspeed *= (u16) 256;
			gpu_fanspeed += (u16) (out.a5 >> 8);
			*val = gpu_fanspeed;
		}
		return 0;
	case hwmon_pwm:
		casper_query(to_wmi_device(dev->parent), CASPER_POWERPLAN,
					   &out);
		if(channel == 0) { // CPU fan
			*val = (long) out.a2;
		}else {
			return -EOPNOTSUPP;
		}
		return 0;
	default:
		return -EOPNOTSUPP;
	}
	
	return 0;
}

int casper_wmi_hwmon_read_string(struct device *dev, enum hwmon_sensor_types type,
		    u32 attr, int channel, const char **str)
{
	if(channel == 0)
		*str = "cpu_fan_speed";
	else if(channel == 1)
		*str = "gpu_fan_speed";
	else
		return -EOPNOTSUPP;
	return 0;
}

int casper_wmi_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
			   u32 attr, int channel, long val)
{
	acpi_status ret;
	switch(type) {
	case hwmon_pwm:
		if(channel != 0)
			return -EOPNOTSUPP;
		/*
		 * 0 is also a valid power plan but Casper's Windows driver 
		 * doesn't support it (probably) because of stability issues.
		 * So we don't support it either.
		 */
		if(val < 1 || val > 4) {
			return -EINVAL;
		}
		ret = casper_set(CASPER_POWERPLAN, val, 0);
		printk("Writing started: %ld", val);
		if(ACPI_FAILURE(ret)) {
			dev_err(dev, "Couldn't set power plan, acpi_status: %d",
				ret);
			return -EINVAL;
		}
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static struct hwmon_ops casper_wmi_hwmon_ops = {
	.is_visible = &casper_wmi_hwmon_is_visible,
	.read = &casper_wmi_hwmon_read,
	.read_string = &casper_wmi_hwmon_read_string,
	.write = &casper_wmi_hwmon_write
};

static const struct hwmon_channel_info * const casper_wmi_hwmon_info[] = {
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL),
	HWMON_CHANNEL_INFO(pwm, HWMON_PWM_MODE),
	NULL
};

static const struct hwmon_chip_info casper_wmi_hwmon_chip_info = {
	.ops = &casper_wmi_hwmon_ops,
	.info = casper_wmi_hwmon_info,
};


static int casper_wmi_probe(struct wmi_device *wdev, const void *context)
{
	struct device *hwmon_dev;
	
	// I couldn't find (with grep) any other driver with this guid
	if (!wmi_has_guid(CASPER_WMI_GUID))
		return -ENODEV;

	hwmon_dev = devm_hwmon_device_register_with_info(&wdev->dev, "casper_wmi", wdev,
							 &casper_wmi_hwmon_chip_info, NULL);
	
	acpi_status result = led_classdev_register(&wdev->dev, &casper_kbd_led);
	if(result != 0) return -ENODEV;
		
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

void casper_wmi_remove(struct wmi_device *wdev)
{
	led_classdev_unregister(&casper_kbd_led);
}

static const struct wmi_device_id casper_wmi_id_table[] = {
	{ CASPER_WMI_GUID, NULL },
	{ }
};

static struct wmi_driver casper_wmi_driver = {
	.driver = {
		.name = "casper-wmi",
	},
	.id_table = casper_wmi_id_table,
	.probe = casper_wmi_probe,
	.remove = &casper_wmi_remove
	//void (*remove)(struct wmi_device *wdev);
};
module_wmi_driver(casper_wmi_driver);

MODULE_DEVICE_TABLE(wmi, casper_wmi_id_table);
