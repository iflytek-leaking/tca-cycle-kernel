### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers
## Configured for: the whole iFLYTEK/Unisoc UMS9620 family (device.prefix1=ums9620)
## Kernel: TCA Cycle Kernel 5.4.210-tca-cycle

### AnyKernel setup
# global properties
properties() { '
kernel.string=TCA Cycle Kernel 5.4.210-tca-cycle (iFLYTEK UMS9620)
do.devicecheck=1
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
device.name1=ums9620_se50
device.name2=ums9620_se50_s30pro
device.name3=s30pro
device.name4=
device.name5=
# accept every UMS9620 family board: ums9620_se50 / ca30pro / ck10 / su30 / tb / 1h10 / 2h10 / ...
# (the board is picked at boot by the stock DTBO, the kernel is the same)
device.prefix1=ums9620
supported.versions=13
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties


### AnyKernel install
## no ramdisk modifications: the stock GKI ramdisk is kept byte-for-byte

# boot shell variables
BLOCK=boot;
IS_SLOT_DEVICE=1;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# boot install
# Only the kernel is replaced.
#  - boot.img is Android boot header v4 and carries NO dtb (the base DTB lives
#    in the vendor_boot partition, the board overlay in dtbo) -> never touched.
#  - the stock ramdisk is left untouched, so split_boot + flash_boot are used
#    to skip the ramdisk unpack/repack steps entirely.
split_boot;
flash_boot;
## end boot install
