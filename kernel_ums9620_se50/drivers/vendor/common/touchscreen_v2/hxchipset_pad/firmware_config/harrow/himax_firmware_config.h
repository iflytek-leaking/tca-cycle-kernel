/************************************************************************
* himax_firmware_config.h  --  board profile: harrow (iFLYTEK SE50)
*
* Reconstructed from the stock iFlytek SE50 vmlinux (driver string
* "3.0.0.19_#5635_Iflytek_SE50_01") by disassembling
* himax_mcu_touch_information() and reading the constants it stores into
* struct himax_ic_data:
*
*     rx_num        = 48      tx_num       = 32
*     bt_num        = 0       max_pt       = 10
*     int_is_edge   = 0       (upstream papaya profile had 1)
*     stylus_func   = 1       (upstream papaya profile had 0 -> pen off)
*     stylus_id_v2  = 0
*     stylus_ratio  = 1       (upstream papaya profile had 10)
*
* The stock code has no himax_differ_config branch and its rodata has no
* vendor strings, so HXTP_DIFFER_CONFIG is intentionally NOT defined.
*
* Firmware/criteria file names are fixed (see himax_common_interface.c):
* Himax_firmware.bin / Himax_mpfw.bin / hx_criteria.csv.
************************************************************************/
#ifndef _HIMAX_FIRMWARE_CONFIG_H_
#define _HIMAX_FIRMWARE_CONFIG_H_

/*===========Himax Option function=============*/
#define HX_RST_PIN_FUNC
#define HX_EXCP_RECOVERY

/*#define HX_NEW_EVENT_STACK_FORMAT*/
/*#define HX_BOOT_UPGRADE*/
#define HX_SMART_WAKEUP
/* Disabled to match the iFlytek SE50 stock kernel: it has no
 * "system enters into pm_suspend" / "pm_completion" strings, i.e. the stock
 * driver does not build the HX_CONFIG_PM spi_driver .pm callbacks nor the
 * wait_for_completion_timeout() in the threaded ISR. */
/*#define HX_CONFIG_PM*/
/*#define HX_GESTURE_TRACK*/
#define HX_RESUME_SEND_CMD	/*Need to enable on TDDI chipset*/
/*#define HX_HIGH_SENSE*/
/*#define HX_PALM_REPORT*/
#define HX_USB_DETECT_GLOBAL
/*#define HX_HEADPHONE*/	/* stock has no himax_headset_detect_func / headset_state */

/*Horizontal and vertical screen switching*/
#define HX_HOR_VER_SWITCH_MODE	/* himax_horizontal_and_vertical_switching() is referenced unconditionally by the IC file */

/* for MTK special platform.If turning on,
 * it will report to system by using specific format.
 */
#define HX_REPORT_BY_ZTE_ALGO
/*#define HX_PROTOCOL_A*/
#define HX_PROTOCOL_B_3PA

#define HX_ZERO_FLASH
#define HX_GET_NOISE
/*#define HX_PEN_ONLY_SWITCH*/	/* stock has no "set pen only mode" strings */

/*system suspend-chipset power off,
 *oncell chipset need to enable the definition
 */
/*#define HX_RESUME_HW_RESET*/

/* Sample code for TP load before LCM
 */
/*#define HX_TP_TRIGGER_LCM_RST*/

/*#define HX_PARSE_FROM_DT*/

/*used for self test get dsram fail in stress test*/
/*#define HX_STRESS_SELF_TEST*/

/*=============================================*/

/* Enable it if driver go into suspend/resume twice */
/*#undef HX_CONFIG_FB*/

/* Enable it if driver go into suspend/resume twice */
/*#undef HX_CONFIG_DRM*/

enum fix_touch_info {
	FIX_HX_RX_NUM = 48,
	FIX_HX_TX_NUM = 32,
	FIX_HX_BT_NUM = 0,
	FIX_HX_MAX_PT = 10,
	FIX_HX_INT_IS_EDGE = false,
	FIX_HX_STYLUS_FUNC = 1,
	FIX_HX_STYLUS_ID_V2 = 0,
	FIX_HX_STYLUS_RATIO = 1,
#if defined(HX_TP_PROC_2T2R)
	FIX_HX_RX_NUM_2 = 0,
	FIX_HX_TX_NUM_2 = 0,
#endif
};

enum hxtp_vendor_id {
	HX_VENDOR_ID_0	= 0,
	HX_VENDOR_ID_1,
	HX_VENDOR_ID_2,
	HX_VENDOR_ID_3,
	HX_VENDOR_ID_MAX		= 0xFF,
};

/* Numbers of modules support */
#define HXTS_VENDOR_0_NAME	"huaying"
#define HXTS_VENDOR_1_NAME	"lead"
#define HXTS_VENDOR_2_NAME	"unknown"
#define HXTS_VENDOR_3_NAME	"unknown"

#endif