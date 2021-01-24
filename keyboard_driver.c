// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/delay.h>

#define DRIVER_NAME	"keyboard_driver"
#define COLUMNS		3
#define ROWS		4
#define COLUMN_STR	"column"
#define ROW_STR		"row"
#define GPIO_DELAY	1

struct keyboard {
	struct gpio_desc *columns[COLUMNS];
	struct gpio_desc *rows[ROWS];
	bool pressed[COLUMNS][ROWS];
	struct workqueue_struct *wq;
	struct delayed_work work;
	u32 delay_ms;
};

static const char keys[COLUMNS][ROWS] = {
	{'1', '4', '7', '*'},
	{'2', '5', '8', '0'},
	{'3', '6', '9', '#'},
};

void print_keys(const bool pressed[COLUMNS][ROWS])
{
	int x, y;
	char ch;
	char row[COLUMNS + 1] = {0};

	pr_info("#####\n");
	for (y = 0; y < ROWS; y++) {
		for (x = 0; x < COLUMNS; x++) {
			ch = pressed[x][y] ? 'X' : '.';
			row[x] = ch;
		}
		pr_info("#%s#\n", row);
	}
	pr_info("#####\n");
}

static void scan_column(struct keyboard *keyboard, int x)
{
	int y;
	int val;
	pr_debug("++%s: %d", __func__, x);

	for (y = 0; y < ROWS; y++) {
		val = gpiod_get_value(keyboard->rows[y]);
		if (!keyboard->pressed[x][y] && !val) {
			pr_info("%c\n", keys[x][y]);
		}
		keyboard->pressed[x][y] = !val;
	}
}

static void keys_polling(struct work_struct *data)
{
	int x;
	int ret;
	struct delayed_work *work = container_of(data, struct delayed_work, work);
	struct keyboard *keyboard = container_of(work, struct keyboard, work);

	pr_debug("++%s\n", __func__);

	for (x = 0; x < COLUMNS; x++) {
		gpiod_direction_output(keyboard->columns[x], 0);
		msleep(GPIO_DELAY);
		mb();
		scan_column(keyboard, x);
		gpiod_direction_input(keyboard->columns[x]);
		mb();
		msleep(GPIO_DELAY);
	}

//	print_keys(keyboard->pressed);

	ret =  queue_delayed_work(keyboard->wq, &keyboard->work,
				  msecs_to_jiffies(keyboard->delay_ms));
	if (false == ret) {
		pr_err("Can't queue work\n");
	}
}

static int keyboard_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct keyboard *keyboard;
	int i;
	int ret;
	int debounce;
	char gpio_name[max(sizeof(ROW_STR), sizeof(COLUMN_STR)) + 1];

	pr_debug("++%s\n", __func__);

	keyboard = devm_kzalloc(dev, sizeof(*keyboard), GFP_KERNEL);
	if (NULL == keyboard) {
		pr_err("Can't allocate memory\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, keyboard);

	for (i = 0; i < COLUMNS; i++) {
		sprintf(gpio_name, "%s%d", COLUMN_STR, i);
		keyboard->columns[i] = devm_gpiod_get(dev, gpio_name, GPIOD_IN);
		if (IS_ERR(keyboard->columns[i])) {
			pr_err("Can't obtain %s\n", gpio_name);
			return PTR_ERR(keyboard->columns[i]);
		}
	}

	for (i = 0; i < ROWS; i++) {
		sprintf(gpio_name, "%s%d", ROW_STR, i);
		keyboard->rows[i] = devm_gpiod_get(dev, gpio_name, GPIOD_IN);
		if (IS_ERR(keyboard->rows[i])) {
			pr_err("Can't obtain %s\n", gpio_name);
			return PTR_ERR(keyboard->rows[i]);
		}
	}

	ret = of_property_read_u32(node, "debounce-delay-ms", &debounce);
	if (ret < 0) {
		dev_warn(dev, "No HW support for debouncing\n");
	} else {
		for (i = 0; i < ROWS; i++)
			ret = gpiod_set_debounce(keyboard->rows[i], debounce * 1000);
	}

	ret = of_property_read_u32(node, "poll-delay-ms", &keyboard->delay_ms);
	if (ret < 0) {
		pr_err("Can't read delay");
		return ret;
	}

	keyboard->wq = create_workqueue("Keys polling");
	INIT_DELAYED_WORK(&keyboard->work, keys_polling);
	ret =  queue_delayed_work(keyboard->wq, &keyboard->work,
				  msecs_to_jiffies(keyboard->delay_ms));
	if (false == ret) {
		pr_err("Can't queue work\n");
		return ret;
	}

	return 0;
}

static int keyboard_remove(struct platform_device *pdev)
{
	struct keyboard * keyboard = dev_get_drvdata(&pdev->dev);

	pr_debug("++%s\n", __func__);

	cancel_delayed_work_sync(&keyboard->work);
	return 0;
}

static const struct of_device_id keyboard_of_match[] = {
	{ .compatible = "globallogic,hw4" },
	{}, /* sentinel */
};
MODULE_DEVICE_TABLE(of, keyboard_of_match);

static struct platform_driver keybord_driver = {
	.probe = keyboard_probe,
	.remove = keyboard_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = keyboard_of_match,
	},
};

module_platform_driver(keybord_driver);

MODULE_AUTHOR("Viaheslav Lykhohub <viacheslav.lykhohub@globallogic.com>");
MODULE_DESCRIPTION("Driver for membrane keyboard");
MODULE_LICENSE("GPL");
