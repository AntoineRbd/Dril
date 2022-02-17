#include <linux/module.h>

MODULE_AUTHOR("Antoine Roubaud");

MODULE_DESCRIPTION("hello world");

MODULE_LICENSE("GPL v2");

__init static int hello_world_init(void) {
    printk(KERN_INFO "hellow world!\n");
    return 0;
}

__exit static void hello_world_exit(void) {
    printk(KERN_INFO "exit hello world\n");
}

module_init(hello_world_init);
module_exit(hello_world_exit);
