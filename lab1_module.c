#include <linux/kernel.h> /* необходим для pr_info() */
#include <linux/module.h> /* необходим для всех модулей */

int hello_init(void) {
	pr_info("Welcome to the Tomsk State University\n");
	return 0;
}

/* Если вернётся не 0, значит, init_module провалилась; модули загрузить не
получится. */
void hello_exit(void)
{
pr_info("Tomsk State University forever!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
