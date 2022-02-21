#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/*
 * Metadata
 */

MODULE_AUTHOR("Antoine Roubaud");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("A driver that like accounting.");

struct stdevs {
    struct cdev cdev;
    /* Trick to reach the underlying struct device,
     * so our attributes will be able to access our data. */
    struct device *dev;
};

/* Will be stored in each struct device 
 */

struct stats_driver_data {
    unsigned int nb_reads;
    unsigned int nb_writes;
};

enum stats_metric {
    NB_READS,
    NB_WRITE,
};

int stats_open(struct inode *inode, struct file *flip);
ssize_t stats_read(struct file *flip, char __user *buf, size_t len, loff_t *off);
int stats_release(struct inode *inode, struct file *flip);
ssize_t stats_write(struct file *flip, const char __user *buf, size_t len, loff_t *off);

/*
 * Local definitions
 */

static unsigned int stats_major;
/* We hardcode the number of devices that show up at
 * insertion, but a module parameter could also be used. */
#define STATS_MAX_DEV 3

static struct stdevs *stdevs[STATS_MAX_DEV];

static struct file_operations stats_fops = {
    .owner = THIS_MODULE,
    .open = stats_open,
    .read = stats_read,
    .release = stats_release,
    .write = stats_write,
};

static unsigned int stats_average(enum stats_metric metric) {

    unsigned int avg;
    struct stats_driver_data *dd;
    size_t i;
    size_t nb_devs;

    /* Just fetch the statistics hoocked to each device,
     * and add them up. We skip devices for witch no such 
     * operation was found, because it would not really
     * make sense. */

    avg = 0;
    nb_devs = 0;
    for (i = 0; i < STATS_MAX_DEV; ++i) {
        dd = (struct stats_driver_data *) stdevs[i]->dev->driver_data;
        switch (metric) {
            case NB_READS:
                if (dd->nb_reads > 0) {
                    avg += dd->nb_reads;
                    ++nb_devs;
                }
                break;
            case NB_WRITE:
                if (dd->nb_writes > 0) {
                    avg += dd->nb_writes;
                    ++nb_devs;
                }
                break;
            default:
                /* Should never reach here */
                break;
        }
    }

    if (nb_devs == 0)
        return 0;
    else
        return avg / nb_devs;
}

/*
 * Character device file operations start here
 */

int stats_open(struct inode *inode, struct file *filp) {

    struct stdevs *stdev;

    stdev = container_of(inode->i_cdev, struct stdevs, cdev);
    if (filp->private_data != stdev) {
        /* First open() on this device: remember it */
        filp->private_data = stdev;
        return 0;
    }
    else {
        /* Prevent multiple open() on device */
        return -EBUSY;
    }
}

int stats_release(struct inode *inode, struct file *filp) {

    /* Do nothing in particular. */
    return 0;
}

ssize_t stats_read(struct file *filp, char __user *buf,
        size_t len, loff_t *off) {

    struct stdevs         *stdev;
    struct stats_driver_data *dd;

    pr_info("%s()\n", __func__);

    stdev = (struct stdevs *) filp->private_data;
    dd    = (struct stats_driver_data *) stdev->dev->driver_data;
    dd->nb_reads++;
    return len;
}

ssize_t stats_write(struct file *filp, const char __user *buf,
        size_t len, loff_t *off) {

    struct stdevs         *stdev;
    struct stats_driver_data *dd;

    pr_info("%s()\n", __func__);

    stdev = (struct stdevs *) filp->private_data;
    dd    = (struct stats_driver_data *) stdev->dev->driver_data;
    dd->nb_writes++;
    /* Silently eat up given data. */
    return len;
}

/*
 * Class attributes, shared among our devices, start here.
 */

static ssize_t avg_nb_reads_show(struct class *class,
        struct class_attribute *attr, char *buf) {

    int ret;
    unsigned int avg;

    avg = stats_average(NB_READS);
    ret = snprintf(buf, 8 /* 32-bit number + \n */, "%u\n", avg);
    if (ret < 0) {
        pr_err("Failed to show average nb reads\n");
    }
    return ret;
}
CLASS_ATTR_RO(avg_nb_reads);

static ssize_t avg_nb_writes_show(struct class *class,
        struct class_attribute *attr, char *buf) {

    int ret;
    unsigned int avg;

    avg = stats_average(NB_WRITE);
    ret = snprintf(buf, 8 /* 32-bit number + \n */, "%u\n", avg);
    if (ret < 0) {
        pr_err("Failed to show average nb writes\n");
    }
    return ret;
}
CLASS_ATTR_RO(avg_nb_writes);

static struct class_attribute *stats_class_attrs[] = {
    &class_attr_avg_nb_reads,
    &class_attr_avg_nb_writes,
    NULL,
};

static const struct attribute_group stats_class_group = {
    .attrs = (struct attribute **)stats_class_attrs,
    /* is_visible() == NULL <==> always visible */
};

static const struct attribute_group *stats_class_groups[] = {
    &stats_class_group,
    NULL,
};

/* Our own class! */
static struct class stats_class = {
    .name        = "statistics",
    .owner       = THIS_MODULE,
    .class_groups = stats_class_groups,
};

/*
 * Device-specific attributes start here.
 */

static ssize_t nb_reads_show(struct device *dev,
        struct device_attribute *attr, char *buf) {

    int ret;
    struct stats_driver_data *dd;

    dd = (struct stats_driver_data *) dev->driver_data;
    ret = snprintf(buf, 8 /* 32-bit number + \n */, "%u\n", dd->nb_reads);
    if (ret < 0) {
        pr_err("Failed to show nb_reads\n");
    }
    return ret;
}
/* Generates dev_attr_nb_reads */
DEVICE_ATTR_RO(nb_reads);

static ssize_t nb_writes_show(struct device *dev,
        struct device_attribute *attr, char *buf) {

    int ret;
    struct stats_driver_data *dd;

    pr_info("%s()\n", __func__);

    dd = (struct stats_driver_data *) dev->driver_data;
    ret = snprintf(buf, 8 /* 32-bit number + \n */, "%u\n", dd->nb_writes);
    if (ret < 0) {
        pr_err("Failed to show nb_writes\n");
    }
    return ret;
}
/* Generates dev_attr_nb_writes */
DEVICE_ATTR_RO(nb_writes);

static struct attribute *stats_attrs[] = {
    &dev_attr_nb_reads.attr,
    &dev_attr_nb_writes.attr,
    NULL,
};

static const struct attribute_group stats_group = {
    .attrs = stats_attrs,
    /* is_visible() == NULL <==> always visible */
};

static const struct attribute_group *stats_groups[] = {
    &stats_group,
    NULL
};

/* Helper functions
 */

static void stats_destroy_sysfs(struct stdevs **stdevs,
        size_t nb_devices) {

    size_t i;

    for (i = 0; i < nb_devices; ++i) {
        kfree(stdevs[i]->dev->driver_data);
        device_destroy(&stats_class, MKDEV(stats_major, i));
    }
    class_unregister(&stats_class);
}

/* Create the whole file hierarchy under /sys/class/statistics/.
 * Character devices setup must have already been completed.
 */
static int stats_create_sysfs(struct stdevs **stdevs) {

    int ret;
    struct device *dev;
    struct stats_driver_data *dd;
    size_t i;

    ret = class_register(&stats_class);
    if (ret < 0) {
        ret = 1;
        goto sysfs_end;
    }

    for (i = 0; i < STATS_MAX_DEV; ++i) {
        /* Create device with all its attributes */
        dev = device_create_with_groups(&stats_class, NULL,
                MKDEV(stats_major, i), NULL /* No private data */,
                stats_groups, "stats%zu", i);
        if (IS_ERR(dev)) {
            ret = 1;
            goto sysfs_cleanup;
        }

        /* Store access to the device. As we're the one creating it,
         * we take advantage if this direct access to struct device.
         * Note that on regular scenarios, this is not the case;
         * "struct device" and "struct cdev" are disjoint. */
        stdevs[i]->dev = dev;

        /* Setup device's statistics, and store them in it */
        dd = kmalloc(sizeof(struct stats_driver_data), GFP_KERNEL);
        if (! dd) {
            device_destroy(&stats_class, MKDEV(stats_major, i));
            ret = 1;
            goto sysfs_cleanup;
        }
        dd->nb_reads = 0;
        dd->nb_writes = 0;
        stdevs[i]->dev->driver_data = (void *)dd;
    }

    goto sysfs_end;

sysfs_cleanup:
    stats_destroy_sysfs(stdevs, i);
sysfs_end:
    return 0;
}

static void stats_delete_devices(size_t count) {

    size_t i;

    for (i = 0; i < count; ++i) {
        cdev_del(&(stdevs[i])->cdev);
        kfree(stdevs[i]);
    }
}

static void stats_init_dev(struct stdevs *dev) {

    dev->cdev.owner = THIS_MODULE;
    cdev_init(&dev->cdev, &stats_fops);
    dev->dev = NULL;
}

static int stats_setup_dev(size_t idx) {

    int ret;

    stdevs[idx] = kmalloc(sizeof(*stdevs[idx]), GFP_KERNEL);
    if (! stdevs[idx]) {
        pr_err("Failed to alloc stdevs %zu\n", idx);
        return 1;
    }

    stats_init_dev(stdevs[idx]);

    ret = cdev_add(&(stdevs[idx])->cdev, MKDEV(stats_major, idx), 1);
    if (ret < 0) {
        pr_err("Failed to add stdevs %zu\n", idx);
        /* Free this device ourselves */
        kfree(stdevs[idx]);
        return 1;
    }

    return 0;
}

__exit
static void stats_exit(void) {

    pr_info("%s()\n", __func__);
    stats_destroy_sysfs(stdevs, STATS_MAX_DEV);
    stats_delete_devices(STATS_MAX_DEV);
    unregister_chrdev_region(MKDEV(stats_major, 0), STATS_MAX_DEV);
}

__init
static int stats_init(void) {

    int ret = 0;
    dev_t dev_t;
    size_t devices_set_up = 0;
    size_t i;

    pr_info("%s()\n", __func__);

    ret = alloc_chrdev_region(&dev_t, 0, STATS_MAX_DEV, "statistics");
    if (ret < 0) {
        pr_err("Failed to allocate major/minor\n");
        goto init_end;
    }
    stats_major = MAJOR(dev_t);
    pr_info("%s() : got major %u\n", __func__, stats_major);

    for (i = 0; i < STATS_MAX_DEV; ++i) {
        if (stats_setup_dev(i)) {
            goto init_cleanup;
        }
        ++devices_set_up;
    }

    if (stats_create_sysfs(stdevs)) {
        pr_err("Failed to create class\n");
        ret = -ENOMEM;
        goto init_cleanup;
    }

    goto init_end;

init_cleanup:
    stats_delete_devices(devices_set_up);
    unregister_chrdev_region(MKDEV(stats_major, 0), STATS_MAX_DEV);
init_end:
    return ret;
}

module_init(stats_init);
module_exit(stats_exit);
