// SPDX-License-Identifier: GPL-2.0
//
// GPIO pulse counter
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


#define NB_MAX  32
int Gpio[NB_MAX];
int Edge[NB_MAX];

int Gpio_count;
int Edge_count;

module_param_array_named(gpio, Gpio, int, &Gpio_count, 0);
module_param_array_named(edge, Edge, int, &Edge_count, 0);

static unsigned long long Counter[NB_MAX];
static spinlock_t Counter_spl[NB_MAX];

static struct miscdevice Gpio_pulse_counter_misc_driver [NB_MAX];


static ssize_t gpio_pulse_counter_read(struct file *filp, char *u_buffer, size_t length, loff_t *offset)
{
	unsigned long irqs;
	unsigned long long value;
	char k_buffer[80];
	unsigned long num = (unsigned long) filp->private_data;

	if ((num < 0) || (num >= Gpio_count)) {
		pr_err("%s: Invalid number: %lu\n", THIS_MODULE->name, num);
		return -EINVAL;
	}

	if (*offset != 0)
		return 0;

	spin_lock_irqsave(&Counter_spl[num], irqs);
	value = Counter[num];
	Counter[num] = 0;
	spin_unlock_irqrestore(&Counter_spl[num], irqs);

	snprintf(k_buffer, 80, "%llu\n", value);

	if (length < strlen(k_buffer) + 1)
		return -EINVAL;
	if (copy_to_user(u_buffer, k_buffer, strlen(k_buffer) + 1) != 0)
		return -EFAULT;
	*offset = strlen(k_buffer) + 1;
	return strlen(k_buffer) + 1;
}


int gpio_pulse_counter_open(struct inode *ind, struct file *filp)
{
	int i;
	for (i = 0; i < Gpio_count; i ++) {
		if (iminor(ind) == Gpio_pulse_counter_misc_driver[i].minor) {
			filp->private_data = (void *) i;
			break;
		}
	}
	return 0;
}


static irqreturn_t gpio_pulse_counter_handler(int irq, void *id)
{
	unsigned long num = (unsigned long) id;

	if ((num < 0) || (num >= Gpio_count)) {
		pr_err("%s: Invalid number: %lu\n", THIS_MODULE->name, num);
		return IRQ_NONE;
	}

	spin_lock(&Counter_spl[num]);
	Counter[num] ++;
	spin_unlock(&Counter_spl[num]);

	return IRQ_HANDLED;
}


static const struct file_operations Gpio_pulse_counter_fops = {
	.owner =  THIS_MODULE,
	.read  =  gpio_pulse_counter_read,
	.open  =  gpio_pulse_counter_open,
};


static int __init gpio_pulse_counter_init(void)
{
	long num;
	int err;
	char name[32];

	if (Gpio_count != Edge_count) {
		pr_err("%s: count of gpios and edges differ.\n", THIS_MODULE->name);
		return -EINVAL;
	}

	if (Gpio_count == 0) {
		pr_err("%s: no GPIO given as module param.\n", THIS_MODULE->name);
		return -EINVAL;
	}

	for (num = 0; num < Gpio_count; num++) {
		err = gpio_request(Gpio[num], THIS_MODULE->name);
		if (err != 0) {
			pr_err("%s: Unable to request GPIO %d\n", THIS_MODULE->name, Gpio[num]);
			for (num-- ; num >= 0; num--)
				gpio_free(Gpio[num]);
			return err;
		}
		gpio_direction_input(Gpio[num]);
	}

	for (num = 0; num < Gpio_count; num++) {
		spin_lock_init(&Counter_spl[num]);
		Counter[num] = 0;
	}

	for (num = 0; num < Gpio_count; num++) {
		err = request_irq(gpio_to_irq(Gpio[num]), gpio_pulse_counter_handler,
			(Edge[num] == 1 ? IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING),
			THIS_MODULE->name, (void *) num);
		if (err != 0) {
			pr_err("%s: Unable to request IRQ for GPIO %d\n", THIS_MODULE->name, Gpio[num]);
			for (num-- ; num >= 0; num--)
				free_irq(gpio_to_irq(Gpio[num]), (void *) num);
			for (num = 0; num < Gpio_count; num++)
				gpio_free(Gpio[num]);
			return err;
		}
	}

	for (num = 0; num < Gpio_count; num++) {
		memset(&Gpio_pulse_counter_misc_driver[num], 0, sizeof(struct miscdevice));
		sprintf(name, "gpio-pulse-counter-%ld", num);
		Gpio_pulse_counter_misc_driver[num].name  = name;
		Gpio_pulse_counter_misc_driver[num].minor = MISC_DYNAMIC_MINOR;
		Gpio_pulse_counter_misc_driver[num].fops  = &Gpio_pulse_counter_fops;
		Gpio_pulse_counter_misc_driver[num].mode  = 0666;

		err = misc_register(&Gpio_pulse_counter_misc_driver[num]);
		if (err != 0) {
			pr_err("%s: unable to request IRQ for GPIO %d\n", THIS_MODULE->name, Gpio[num]);
			for (num-- ; num >= 0; num--)
				misc_deregister(&Gpio_pulse_counter_misc_driver[num]);
			for (num = 0; num < Gpio_count; num++) {
				free_irq(gpio_to_irq(Gpio[num]), (void *) num);
				gpio_free(Gpio[num]);
			}
			return err;
		}
	}

	return 0;
}


static void __exit gpio_pulse_counter_exit(void)
{
	unsigned long num;

	for (num = 0; num < Gpio_count; num++) {
		misc_deregister(&Gpio_pulse_counter_misc_driver[num]);
		free_irq(gpio_to_irq(Gpio[num]), (void *) num);
		gpio_free(Gpio[num]);
	}
}


module_init(gpio_pulse_counter_init);
module_exit(gpio_pulse_counter_exit);

MODULE_DESCRIPTION("GPIO Pulse Counter");
MODULE_AUTHOR("Christophe Blaess <Christophe.Blaess@Logilin.fr>");
MODULE_LICENSE("GPL v2");
