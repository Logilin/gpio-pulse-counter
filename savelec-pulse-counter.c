// SPDX-License-Identifier: GPL-2.0
//
// Savelec board pulse counter
//
// (c) 2024 Christophe Blaess
//
//    https://www.logilin.fr/
//

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>


static unsigned long long Counter;
static spinlock_t Counter_spl;

#define SAVELEC_PULSE_IN 31

static ssize_t savelec_pulse_counter_read(struct file *filp, char *u_buffer, size_t length, loff_t *offset)
{
	unsigned long irqs;
	unsigned long long value;
	char k_buffer[80];

	if (*offset != 0)
		return 0;

	spin_lock_irqsave(&Counter_spl, irqs);
	value = Counter;
	Counter = 0;
	spin_unlock_irqrestore(&Counter_spl, irqs);

	snprintf(k_buffer, 80, "%llu\n", value);

	if (length < strlen(k_buffer) + 1)
		return -EINVAL;
	if (copy_to_user(u_buffer, k_buffer, strlen(k_buffer) + 1) != 0)
		return -EFAULT;
	*offset = strlen(k_buffer) + 1;
	return strlen(k_buffer) + 1;
}


static irqreturn_t savelec_pulse_counter_handler(int irq, void *ident)
{
	spin_lock(&Counter_spl);

	Counter ++;

	spin_unlock(&Counter_spl);

	return IRQ_HANDLED;
}


static const struct file_operations savelec_pulse_counter_fops = {
	.owner =  THIS_MODULE,
	.read  =  savelec_pulse_counter_read,
};


static struct miscdevice savelec_pulse_counter_misc_driver = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = THIS_MODULE->name,
	.fops  = &savelec_pulse_counter_fops,
	.mode  = 0666,
};


static int __init savelec_pulse_counter_init(void)
{
	int err;

	err = gpio_request(SAVELEC_PULSE_IN, THIS_MODULE->name);
	if (err != 0)
		return err;

	err = gpio_direction_input(SAVELEC_PULSE_IN);
	if (err != 0) {
		gpio_free(SAVELEC_PULSE_IN);
		return err;
	}

	spin_lock_init(&Counter_spl);
	Counter = 0;

	err = request_irq(gpio_to_irq(SAVELEC_PULSE_IN), savelec_pulse_counter_handler,
		IRQF_SHARED | IRQF_TRIGGER_RISING,
		THIS_MODULE->name, THIS_MODULE->name);
	if (err != 0) {
		gpio_free(SAVELEC_PULSE_IN);
		return err;
	}

	err = misc_register(&savelec_pulse_counter_misc_driver);
	if (err != 0) {
		free_irq(gpio_to_irq(SAVELEC_PULSE_IN), THIS_MODULE->name);
		gpio_free(SAVELEC_PULSE_IN);
		return err;
	}

	return 0;
}


static void __exit savelec_pulse_counter_exit(void)
{
	misc_deregister(&savelec_pulse_counter_misc_driver);
	free_irq(gpio_to_irq(SAVELEC_PULSE_IN), THIS_MODULE->name);
	gpio_free(SAVELEC_PULSE_IN);
}


module_init(savelec_pulse_counter_init);
module_exit(savelec_pulse_counter_exit);

MODULE_DESCRIPTION("Savelec Pulse Counter");
MODULE_AUTHOR("Christophe Blaess <Christophe.Blaess@Logilin.fr>");
MODULE_LICENSE("GPL v2");
