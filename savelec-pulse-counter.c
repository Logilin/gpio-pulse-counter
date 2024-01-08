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


#define NB   6
static unsigned long long Counter[NB];
static spinlock_t Counter_spl[NB];

int Savelec_pulse_in[NB] = { 31, 30, 122, 121, 124, 123};

static struct miscdevice savelec_pulse_counter_misc_driver [NB];


static ssize_t savelec_pulse_counter_read(struct file *filp, char *u_buffer, size_t length, loff_t *offset)
{
	unsigned long irqs;
	unsigned long long value;
	char k_buffer[80];
	unsigned long num = (unsigned long) filp->private_data;

	if ((num < 0) || (num >= NB)) {
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


int savelec_pulse_counter_open(struct inode *ind, struct file *filp)
{
	int i;
	for (i = 0; i < NB; i ++) {
		if (iminor(ind) == savelec_pulse_counter_misc_driver[i].minor) {
			filp->private_data = (void *) i;
			break;
		}
	}
	return 0;
}


static irqreturn_t savelec_pulse_counter_handler(int irq, void *id)
{
	unsigned long num = (unsigned long) id;

	if ((num < 0) || (num >= NB)) {
		pr_err("%s: Invalid number: %lu\n", THIS_MODULE->name, num);
		return IRQ_NONE;
	}

	spin_lock(&Counter_spl[num]);

	Counter[num] ++;

	spin_unlock(&Counter_spl[num]);

	return IRQ_HANDLED;
}


static const struct file_operations savelec_pulse_counter_fops = {
	.owner =  THIS_MODULE,
	.read  =  savelec_pulse_counter_read,
	.open  =  savelec_pulse_counter_open,
};


static int __init savelec_pulse_counter_init(void)
{
	long num;
	int err;
	char name[32];

	for (num = 0; num < NB; num++) {
		err = gpio_request(Savelec_pulse_in[num], THIS_MODULE->name);
		if (err != 0) {
			pr_err("%s: Unable to request GPIO %d\n", THIS_MODULE->name, Savelec_pulse_in[num]);
			for (num-- ; num >= 0; num--)
				gpio_free(Savelec_pulse_in[num]);
			return err;
		}
		gpio_direction_input(Savelec_pulse_in[num]);
	}

	for (num = 0; num < NB; num++) {
		spin_lock_init(&Counter_spl[num]);
		Counter[num] = 0;
	}

	for (num = 0; num < NB; num++) {
		err = request_irq(gpio_to_irq(Savelec_pulse_in[num]), savelec_pulse_counter_handler,
			IRQF_TRIGGER_RISING,
			THIS_MODULE->name, (void *) num);
		if (err != 0) {
			pr_err("%s: Unable to request IRQ for GPIO %d\n", THIS_MODULE->name, Savelec_pulse_in[num]);
			for (num-- ; num >= 0; num--)
				free_irq(gpio_to_irq(Savelec_pulse_in[num]), (void *) num);
			for (num = 0; num < NB; num++)
				gpio_free(Savelec_pulse_in[num]);
			return err;
		}
	}


	for (num = 0; num < NB; num++) {
		memset(&savelec_pulse_counter_misc_driver[num], 0, sizeof(struct miscdevice));
		sprintf(name, "savelec-pulse-counter-%ld", num);
		savelec_pulse_counter_misc_driver[num].name  = name;
		savelec_pulse_counter_misc_driver[num].minor = MISC_DYNAMIC_MINOR;
		savelec_pulse_counter_misc_driver[num].fops  = &savelec_pulse_counter_fops;
		savelec_pulse_counter_misc_driver[num].mode  = 0666;

		err = misc_register(&savelec_pulse_counter_misc_driver[num]);
		if (err != 0) {
			pr_err("%s: Unable to request IRQ for GPIO %d\n", THIS_MODULE->name, Savelec_pulse_in[num]);
			for (num-- ; num >= 0; num--)
				misc_deregister(&savelec_pulse_counter_misc_driver[num]);
			for (num = 0; num < NB; num++) {
				free_irq(gpio_to_irq(Savelec_pulse_in[num]), (void *) num);
				gpio_free(Savelec_pulse_in[num]);
			}
			return err;
		}
	}

	return 0;
}


static void __exit savelec_pulse_counter_exit(void)
{
	unsigned long num;

	for (num = 0; num < NB; num++) {
		misc_deregister(&savelec_pulse_counter_misc_driver[num]);
		free_irq(gpio_to_irq(Savelec_pulse_in[num]), (void *) num);
		gpio_free(Savelec_pulse_in[num]);
	}
}


module_init(savelec_pulse_counter_init);
module_exit(savelec_pulse_counter_exit);

MODULE_DESCRIPTION("Savelec Pulse Counter");
MODULE_AUTHOR("Christophe Blaess <Christophe.Blaess@Logilin.fr>");
MODULE_LICENSE("GPL v2");
