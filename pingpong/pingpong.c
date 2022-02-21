#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/*
 * Metadata
 */

MODULE_AUTHOR("Antoine Roubaud");
MODULE_DESCRIPTION("A quite boring character device driver.");
MODULE_LICENSE("GPL v2");


/*
 * Local definitions
 */

#define PINGPONG_BUFSIZE sizeof("ping")

struct pingpong_dev {
    struct cdev cdev;
    bool was_pinged;
};

/* Major will always be dynamically allocated */
static int major;
static struct pingpong_dev *pp_dev;

/*
 * Character device file operations
 */

int pingpong_open(struct inode *inode, struct file *file) {

    unsigned int i_major = imajor(inode);
    unsigned int i_minor = iminor(inode);

    pr_info("%s()\n", __func__);

    if (i_major != major) {
	pr_err("Invalid major %d, expected %d\n", i_major, major);
	return -EINVAL;
    }
    if (i_minor != 0) {
        pr_err("pingpong driver only handles minor 0!\n");
        return -EINVAL;
    }

    /* Make file decriptor "remember" its mirror device object,
     * for later use in open() and write(). */
    file->private_data = pp_dev;

    return 0;
}

int pingpong_release(struct inode *inode /* unused */,
        struct file *file /* unused */) {

    pr_info("%s()\n", __func__);

    /* Nothing in particular to do */
    return 0;
}

ssize_t pingpong_read(struct file *file, char __user *buf, 
        size_t len, loff_t *off /* unused */) {

    struct pingpong_dev *dev;
    /* Don't forget trailling NUL */
    char pongbuf[PINGPONG_BUFSIZE+1];

    dev = (struct pingpong_dev *)file->private_data;
    if (dev->was_pinged) {
        /* Answer with a pong. Don't handle offsets */
        if (len < PINGPONG_BUFSIZE) {
            return -EINVAL;
        }
        len = min(len, PINGPONG_BUFSIZE+1);
        memset(pongbuf, 0, PINGPONG_BUFSIZE+1);
        memcpy(pongbuf, "pong", PINGPONG_BUFSIZE);
        if (copy_to_user(buf, pongbuf, len)) {
            pr_err("Failed to copy data to user\n");
            return -EINVAL;
        }
        dev->was_pinged = false;
    }
    else {
        /* Nothing to say */
        len = 0;
    }
    return len;
}

ssize_t pingpong_write(struct file *file, const char __user *buf,
        size_t len, loff_t *off /*unused */) {

    struct pingpong_dev *dev;
    /* Don't forget trailling NUL */
    char pingbuf[PINGPONG_BUFSIZE+1];

    pr_info("%s(len=%zu)\n", __func__, len);

    dev = (struct pingpong_dev *)file->private_data;
    /* copy_(from|to)_user() do not append a trailling NUL, so memset()
     * our buffer to avoid risking corrupting the stacks. */
    memset(pingbuf, 0, PINGPONG_BUFSIZE+1);

    pr_info("Cutting data length to %zu\n", PINGPONG_BUFSIZE);
    len = min(len, PINGPONG_BUFSIZE);
    if (copy_from_user(pingbuf, buf, len)) {
        pr_err("Failed to copy user data\n");
        return -EFAULT;
    }

    if (strcmp(pingbuf, "ping") == 0) {
        pr_info("I was pinged!\n");
        dev->was_pinged = true;
    }
    else {
        pr_info("I was not pinged; I received %s\n", pingbuf);
    }

    /* Even id the message is not a ping, silently eat user data */
    return len;
}

/*
 * Init & Exit
 */

static struct file_operations pingpong_fops = {
    .owner   = THIS_MODULE,
    .open    = pingpong_open,
    .release = pingpong_release,
    .read    = pingpong_read,
    .write   = pingpong_write,
    /* Other functions are using the kernel's defaults */
};

static struct pingpong_dev *pingpong_create(void) {

    struct pingpong_dev *dev = kmalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        return NULL;
    }

    dev->cdev.owner = THIS_MODULE;
    cdev_init(&dev->cdev, &pingpong_fops);

    dev->was_pinged = false;

    return dev;
}

static void pingpong_destroy(struct pingpong_dev *dev) {

    cdev_del(&dev->cdev);
    kfree(dev);
}

__exit static void pingpong_exit(void) {

    dev_t dev;

    dev = MKDEV(major, 0);
    pingpong_destroy(pp_dev);
    unregister_chrdev_region(dev, 1);
}

__init static int pingpong_init(void) {

    dev_t dev;
    int ret;
    const char devname[] = "pingpong";

    /* Allocate major */
    ret = alloc_chrdev_region(&dev, 0, 1, devname);
    if (ret < 0) {
        pr_info("Failed to allocate major\n");
        return 1;
    }
    else {
        major = MAJOR(dev);
        pr_info("Got major %d\n", major);
    }

    /* Register char device */
    pp_dev = pingpong_create();
    if (!pp_dev) {
        pr_err("Failed to init pingpong_dev\n");
        return -ENOMEM;
    }

    if (cdev_add(&pp_dev->cdev, dev, 1) < 0) {
        pr_err("Failed ot register char device\n");
        return -ENOMEM;
    }
    return 0;
}

module_init(pingpong_init);
module_exit(pingpong_exit);
