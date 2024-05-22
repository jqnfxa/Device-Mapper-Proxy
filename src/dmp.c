#include <linux/device-mapper.h>
#include <linux/module.h>
#include <linux/average.h>
#include <linux/bio.h>
#include <linux/init.h>
#include <linux/kobject.h>


static struct kobject *stat = NULL;
DECLARE_EWMA(average, 8, 16);
static struct tracked_statistics {
	uint64_t read_requests;
	uint64_t write_requests;
	uint64_t total_requests;

	struct ewma_average read_avg_blocksize;
	struct ewma_average write_avg_blocksize;
	struct ewma_average total_avg_blocksize;
} dmp_statistic = {0};


/**
 * @brief initialize the dmp_statistic structure
 *
 * @param statistic the structure to initialize
 *
 * @return 0 on success
 */
static int init_dmp_statistic(struct tracked_statistics *statistic)
{
	if (statistic == NULL)
	{
		return -EINVAL;
	}

	ewma_average_init(&statistic->read_avg_blocksize);
	ewma_average_init(&statistic->write_avg_blocksize);
	ewma_average_init(&statistic->total_avg_blocksize);

	return 0;
}


/**
 * @brief function to report the statistic to the user by sysfs
 *
 * @param kobj the kobject
 * @param attr the attribute
 * @param buf output buffer
 *
 * @return number of bytes written
 */
static ssize_t volumes_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"read:\n\treqs: %llu\n\tavg size: %lu\n"
		"write:\n\treqs: %llu\n\tavg size: %lu\n"
		"total:\n\treqs: %llu\n\tavg size: %lu\n",
		dmp_statistic.read_requests,
		ewma_average_read(&dmp_statistic.read_avg_blocksize),
		dmp_statistic.write_requests,
		ewma_average_read(&dmp_statistic.write_avg_blocksize),
		dmp_statistic.total_requests,
		ewma_average_read(&dmp_statistic.total_avg_blocksize));
}
static struct kobj_attribute volumes_attr = __ATTR(volumes, 0644, volumes_show, NULL);


/**
 * @brief constructor for the dmp device
 *
 * @param ti dm_target
 * @param argc number of arguments
 * @param argv array of arguments
 *
 * @return 0 on success and error code on failure
 */
static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	if (argc != 1)
	{
		ti->error = "wrong number of arguments";
		return -EINVAL;
	}

	struct dm_dev *device = (struct dm_dev *)kmalloc(sizeof(struct dm_dev), GFP_KERNEL);
	if(device == NULL)
	{
		ti->error = "out of memory";
		return -ENOMEM;
	}

	const blk_mode_t mode = dm_table_get_mode(ti->table);
	const int status = dm_get_device(ti, argv[0], mode, &device);
	if (status != 0)
	{
		kfree(device);
		ti->error = "dmp: device lookup failed";
		return status;
	}

	ti->private = device;
	return 0;
}


/**
 * @brief destructor for the dmp device
 *
 * @param ti dm_target
 */
static void dmp_dtr(struct dm_target *ti)
{
	struct dm_dev *device = (struct dm_dev *)ti->private;
	dm_put_device(ti, device);
	kfree(device);
}


/**
 * @brief function to call on read/write operations
 *
 * @param ti dm_target
 * @param bio block of I/O
 *
 * @return `DM_MAPIO_SUBMITTED` on success and `DM_MAPIO_KILL` on failure
 */
static int dmp_map(struct dm_target *ti, struct bio *bio)
{
	const uint32_t block_size = bio->bi_iter.bi_size;

	++dmp_statistic.total_requests;
	ewma_average_add(&dmp_statistic.total_avg_blocksize, block_size);

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		++dmp_statistic.read_requests;
		ewma_average_add(&dmp_statistic.read_avg_blocksize, block_size);
		break;
	case REQ_OP_WRITE:
		++dmp_statistic.write_requests;
		ewma_average_add(&dmp_statistic.write_avg_blocksize, block_size);
		break;
	default:
		return DM_MAPIO_KILL;
	}

	struct dm_dev *device = (struct dm_dev *)ti->private;
	bio_set_dev(bio, device->bdev);
	submit_bio(bio);

	return DM_MAPIO_SUBMITTED;
}


static struct target_type dmp = {
	.name = "dmp",
	.version = {1, 0, 0},
	.module = THIS_MODULE,
	.ctr = dmp_ctr,
	.dtr = dmp_dtr,
	.map = dmp_map,
};


/**
 * @brief function to initialize the module
 *
 * @return 0 on success and error code on failure
 */
static int dmp_init(void)
{
	int status = dm_register_target(&dmp);
	if (status < 0)
	{
		pr_err("failed to register target %d\n", status);
		return status;
	}

	stat = kobject_create_and_add("stat", &THIS_MODULE->mkobj.kobj);
	if(stat == NULL)
	{
		pr_err("failed to add object\n");
		dm_unregister_target(&dmp);
		return -ENOMEM;
	}

	status = sysfs_create_file(stat, &volumes_attr.attr);
	if (status < 0)
	{
		pr_err("failed to create sysfs entry of dmp\n");
		dm_unregister_target(&dmp);
		kobject_put(stat);
		return status;
	}

	init_dmp_statistic(&dmp_statistic);
	return 0;
}


/**
 * @brief function to destroy the module
 */
static void dmp_exit(void)
{
	dm_unregister_target(&dmp);
	kobject_put(stat);
}


module_init(dmp_init);
module_exit(dmp_exit);

MODULE_AUTHOR("Ivan_Borisov");
MODULE_DESCRIPTION("Device Mapper Proxy");
MODULE_LICENSE("GPL");
