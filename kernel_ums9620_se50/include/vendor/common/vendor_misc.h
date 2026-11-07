#ifndef __POWER_VENDOR_MISC__
#define __POWER_VENDOR_MISC__

enum charger_types_oem {
	CHARGER_TYPE_DEFAULT = -1,
	CHARGER_TYPE_SDP = CHARGER_TYPE_DEFAULT,
	CHARGER_TYPE_UNKNOWN = 0,
	CHARGER_TYPE_FAST_CHARGER = 1,
	CHARGER_TYPE_PD = CHARGER_TYPE_FAST_CHARGER,
	CHARGER_TYPE_QC = CHARGER_TYPE_FAST_CHARGER,
	CHARGER_TYPE_MTK_PE_PLUS = CHARGER_TYPE_FAST_CHARGER,
	CHARGER_TYPE_SFCP = CHARGER_TYPE_FAST_CHARGER,
	CHARGER_TYPE_5V_ADAPTER = 2,
	CHARGER_TYPE_DCP_SLOW = 3,
	CHARGER_TYPE_SDP_NBC1P2 = 4,
	CHARGER_TYPE_SDP_NBC1P2_SLOW = 5,
	CHARGER_TYPE_SDP_NBC1P2_CHARGR_ERR = 6,
	CHARGER_TYPE_SQC = 7,
};

struct vendor_misc_ops {
	/* How the ops should behave */
	char *node_name;
	/* Returns 0, or -errno.  arg is in kp->arg. */
	int (*set)(const char *val, const void *arg);
	/* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
	int (*get)(char *buffer, const void *arg);
	/* Optional function to free kp->arg when module unloaded. */
	void (*free)(void *arg);
	void *arg;
};


#if defined(CONFIG_VENDOR_MISC) ||defined(CONFIG_VENDOR_MISC_COMMON)
extern int vendor_misc_register_callback(struct vendor_misc_ops * node_ops, void * arg);
extern int vendor_misc_get_node_val(const char *node_name);
int vendor_poweroff_charging_status(void);

#else
static inline int vendor_misc_register_callback(struct vendor_misc_ops * node_ops, void * arg)
{
	return -ENOSYS;
}

static inline int vendor_misc_get_node_val(const char *node_name)
{
	return -ENOSYS;
}

static inline int vendor_poweroff_charging_status(void)
{
	return -ENOSYS;
}

#endif

#if defined(CONFIG_VENDOR_CHARGER_POLICY_SERVICE) ||defined(CONFIG_VENDOR_CHARGER_POLICY_SERVICE_COMMON)
extern bool charger_policy_get_status(void);
#else
static inline bool charger_policy_get_status(void)
{
	return 0;
}
#endif


#endif
