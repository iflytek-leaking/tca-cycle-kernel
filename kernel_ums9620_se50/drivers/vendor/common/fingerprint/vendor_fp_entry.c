/****************************

       vendor_fp.c

****************************/

#include <linux/init.h>
#include <linux/module.h>

#include "vendor_fp_entry.h"

static int __init vendor_fp_init(void)
{
	vendor_fp_log(INFO_LOG, "%s enter!\n", __func__);

#ifdef CONFIG_PLATFORM_FINGERPRINT_CDFINGER
	vendor_fp_log(INFO_LOG, "cdfinger_fp_init enter!\n");
	cdfinger_fp_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_CHIPONE
	vendor_fp_log(INFO_LOG, "fpsensor_init enter!\n");
	fpsensor_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SUNWAVE
	vendor_fp_log(INFO_LOG, "sf_ctl_driver_init enter!\n");
	sf_ctl_driver_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_GOODIX
	vendor_fp_log(INFO_LOG, "goodix_driver_init enter!\n");
	gf_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC
	vendor_fp_log(INFO_LOG, "fpc_sensor_init enter!\n");
	fpc_sensor_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC1020
	vendor_fp_log(INFO_LOG, "fpc_init enter!\n");
	fpc_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SILEAD
	vendor_fp_log(INFO_LOG, "silfp_dev_init enter!\n");
	silfp_dev_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH
	vendor_fp_log(INFO_LOG, "focaltech_fp_driver_init enter!\n");
	focaltech_fp_driver_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH_V2
	vendor_fp_log(INFO_LOG, "focaltech_fp_driver_v2_init enter!\n");
	focaltech_fp_driver_v2_init();
#endif

	return 0;
}


static void __exit vendor_fp_exit(void)
{
	vendor_fp_log(INFO_LOG, "%s enter!\n", __func__);

#ifdef CONFIG_PLATFORM_FINGERPRINT_CDFINGER
	vendor_fp_log(INFO_LOG, "cdfinger_fp_exit enter!\n");
	cdfinger_fp_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_CHIPONE
	vendor_fp_log(INFO_LOG, "fpsensor_exit enter!\n");
	fpsensor_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SUNWAVE
	vendor_fp_log(INFO_LOG, "sf_ctl_driver_exit enter!\n");
	sf_ctl_driver_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_GOODIX
	vendor_fp_log(INFO_LOG, "goodix_driver_exit enter!\n");
	gf_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC
	vendor_fp_log(INFO_LOG, "fpc_sensor_exit enter!\n");
	fpc_sensor_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC1020
	vendor_fp_log(INFO_LOG, "fpc_exit enter!\n");
	fpc_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SILEAD
	vendor_fp_log(INFO_LOG, "silfp_dev_exit enter!\n");
	silfp_dev_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH
	vendor_fp_log(INFO_LOG, "focaltech_fp_driver_exit enter!\n");
	focaltech_fp_driver_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH_V2
	vendor_fp_log(INFO_LOG, "focaltech_fp_driver_v2_exit enter!\n");
	focaltech_fp_driver_v2_exit();
#endif

}

module_init(vendor_fp_init);
module_exit(vendor_fp_exit);

MODULE_DESCRIPTION("VENDOR Fp Driver Entry");
MODULE_AUTHOR("***@vendor.com");
MODULE_LICENSE("GPL");
MODULE_ALIAS("VENDOR");


