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

    return 0;
}

ssize_t pingpong_read(struct file *file, char __user *buf, 
        size_t len, loff_t *off /* unused */) {
}






















