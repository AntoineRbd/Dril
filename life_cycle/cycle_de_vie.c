#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>


MODULE_AUTHOR("Antoine Roubaud");

MODULE_DESCRIPTION("life cycle");

MODULE_LICENSE("GPL v2");

struct my_dev {
	struct cdev cdev;
};

static int major;
static struct my_dev *my_dev;

static struct file_operations life_cycle_fops = {
	.owner = THIS_MODULE,
	/* Only use the kernerl's defaults */
};

__init static int life_cycle_init(void) {

	dev_t dev;
	int ret;
	const char devname[] = "life_cycle";

	/* Allocate major */
	ret = alloc_chrdev_region(&dev, 0, 1, devname);

	if (ret < 0) {
		pr_err("Failed to allocate major\n");
		return 1;
	}
	else {
		major = MAJOR(dev);
		pr_info("Got major %d\n", major);
	}

	/* Allocate our device structure */
	my_dev = kmalloc(sizeof(*my_dev), GFP_KERNEL);
	if (! my_dev) {
		pr_err("Failed to allocate struct my_dev \n");
		return -ENOMEM;
	}
	else {
		pr_info("Allocated struct my_dev\n");
	}

	/* Register char device */
	my_dev->cdev.owner = THIS_MODULE;
	cdev_init(&my_dev->cdev, &life_cycle_fops);

	ret = cdev_add(&my_dev->cdev, dev, 1);
	if (ret < 0) {
		pr_err("Failed to register char device");
		return -ENOMEM;
	}
	else {
		pr_info("Registered char device\n");
	}

	return 0;
}

__exit static void life_cycle_exit(void) {
	dev_t dev;

	/* Unregister */
	cdev_del(&my_dev->cdev);
	pr_info("Unregistered char device\n");

	/* Free my_dev struct */
	kfree(my_dev);
	pr_info("Freed struct my_dev\n");

	/* Release major*/
	dev = MKDEV(major, 0);
	unregister_chrdev_region(dev, 1);
	pr_info("Released major %d\n", major);

}

module_init(life_cycle_init);
module_exit(life_cycle_exit);
