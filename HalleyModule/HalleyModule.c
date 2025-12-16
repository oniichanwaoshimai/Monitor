#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/sched.h>

#define MODULE_NAME "HalleyModule"
#define PROC_FILE_NAME "tsulab"

// Период обращения кометы Галлея: ~75 лет (в секундах)
#define HALLEY_PERIOD_SECONDS (75 * 365 * 24 * 3600)

// Последний перигелий: 9 февраля 1986 года
#define LAST_PERIHELION_SECONDS	508266000

// Следующий перигелий: 28 июля 2061 года
#define NEXT_PERIHELION_SECONDS	2889709200

static struct proc_dir_entry *proc_entry;

// Функция для чтения файла /proc/tsulab
static int tsulab_proc_show(struct seq_file *m, void *v)
{
    struct timespec64 now;
    ktime_get_real_ts64(&now);
    
    u64 current_time = now.tv_sec;
    u64 elapsed = current_time - LAST_PERIHELION_SECONDS;
    u64 total_interval = NEXT_PERIHELION_SECONDS - LAST_PERIHELION_SECONDS;
    
    // в ядре нельзя использовать плавающую точку, следовательно делаем с помошью целочисленной арифметики
    // умножаем на 10000 и получения точности до 2 знаков после запятой
    u64 percentage_scaled = (elapsed * 10000ULL) / total_interval;
    u64 percentage_int = percentage_scaled / 100ULL;
    u64 percentage_frac = percentage_scaled % 100ULL;

    seq_printf(m, "Comet Halley Progress Tracker\n");
    seq_printf(m, "Last perihelion:    1986-02-09\n");
    seq_printf(m, "Next perihelion:    2061-07-28\n");

    // время в годах
    u64 years_elapsed = elapsed / (365ULL * 24ULL * 3600ULL);
    u64 years_total = total_interval / (365ULL * 24ULL * 3600ULL);

    seq_printf(m, "Time elapsed:       %llu years\n", years_elapsed);
    seq_printf(m, "Total time:         %llu years\n", years_total);
    seq_printf(m, "\nProgress between perihelia: %llu.%02llu%%\n", percentage_int, percentage_frac);

    return 0;
}

// Функция открытия proc файла
static int tsulab_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, tsulab_proc_show, NULL);
}

//для работы с фалйом 
static const struct proc_ops tsulab_proc_ops = {
    .proc_open    = tsulab_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

// Функция инициализации модуля
static int __init tsulab_init(void)
{
    pr_info("Tracking Comet Halley's progress between perihelia\n");
    
    // Создаем файл в /proc
    proc_entry = proc_create(PROC_FILE_NAME, 0444, NULL, &tsulab_proc_ops);
    if (!proc_entry) {
        pr_err("Failed to create /proc/%s\n", PROC_FILE_NAME);
        return -ENOMEM;
    }
    
    pr_info("Created /proc/%s\n", PROC_FILE_NAME);
    
    return 0;
}

// Функция очистки модуля
static void __exit tsulab_exit(void)
{
    // Удаляем файл из /proc
    if (proc_entry) {
        remove_proc_entry(PROC_FILE_NAME, NULL);
    }
    
    pr_info("Removed /proc/%s\n", PROC_FILE_NAME);
}

module_init(tsulab_init);
module_exit(tsulab_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Comet Halley progress tracker between perihelia");
