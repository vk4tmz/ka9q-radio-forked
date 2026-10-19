/* Standalone RX888 USB discovery and explicitly gated FX3 firmware upload.
 * Deliberately does not claim the streaming interface or send vendor controls.
 * Upload is disruptive: only available with --upload and --firmware together.
 */
#include <libusb-1.0/libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "ezusb.h"

int Ezusb_verbose = 0; /* required by ezusb.c */

static void usage(const char *argv0) {
  fprintf(stderr,
    "Usage: %s list\n"
    "       %s upload --firmware /absolute/path/SDDC_FX3.img --upload\n"
    "\nlist: passive USB enumeration; no interface claim, reset or control commands.\n"
    "upload: destructive to receiver state; only FX3 bootloader 04b4:00f3.\n",
    argv0, argv0);
}
static bool valid_firmware(const char *path) {
  struct stat st;
  return path && path[0] == '/' && stat(path,&st) == 0 &&
         S_ISREG(st.st_mode) && st.st_size > 0;
}
int main(int argc,char **argv) {
  bool list = argc == 2 && strcmp(argv[1],"list")==0;
  bool upload = argc >= 2 && strcmp(argv[1],"upload")==0;
  bool confirmed = false;
  const char *firmware = NULL;
  if(!list && !upload){usage(argv[0]);return 2;}
  if(upload) {
    for(int i=2;i<argc;i++){
      if(strcmp(argv[i],"--upload")==0 && !confirmed) confirmed=true;
      else if(strcmp(argv[i],"--firmware")==0 && !firmware && i+1<argc) firmware=argv[++i];
      else {usage(argv[0]);return 2;}
    }
    if(!confirmed || !valid_firmware(firmware)){
      fprintf(stderr,"Refusing upload: require --upload and existing absolute firmware path.\n");
      return 2;
    }
  }
  libusb_context *ctx=NULL;
  int rc=libusb_init(&ctx);
  if(rc<0){fprintf(stderr,"libusb_init: %s\n",libusb_error_name(rc));return 1;}
  libusb_device **devs=NULL;
  ssize_t count=libusb_get_device_list(ctx,&devs);
  if(count<0){fprintf(stderr,"device list: %s\n",libusb_error_name((int)count));libusb_exit(ctx);return 1;}
  unsigned found=0, targets=0, uploaded=0;
  for(ssize_t i=0;i<count;i++){
    struct libusb_device_descriptor desc;
    if(libusb_get_device_descriptor(devs[i],&desc)!=0)continue;
    if(desc.idVendor!=0x04b4 || (desc.idProduct!=0x00f3 && desc.idProduct!=0x00f1))continue;
    found++;
    printf("bus=%u address=%u vidpid=%04x:%04x state=%s speed=%d\n",
      libusb_get_bus_number(devs[i]),libusb_get_device_address(devs[i]),
      desc.idVendor,desc.idProduct,desc.idProduct==0x00f3?"bootloader":"loaded",
      (int)libusb_get_device_speed(devs[i]));
    fflush(stdout);
    if(!upload || desc.idProduct!=0x00f3)continue;
    targets++;
    libusb_device_handle *handle=NULL;
    rc=libusb_open(devs[i],&handle);
    if(rc<0){fprintf(stderr,"Cannot open bootloader: %s\n",libusb_error_name(rc));continue;}
    fprintf(stderr,"Uploading %s to bootloader bus=%u address=%u\n",
      firmware,libusb_get_bus_number(devs[i]),libusb_get_device_address(devs[i]));
    rc=ezusb_load_ram(handle,firmware,FX_TYPE_FX3,IMG_TYPE_IMG,1);
    if(rc==0) uploaded++;
    else fprintf(stderr,"Firmware upload failed (rc=%d)\n",rc);
    libusb_close(handle);
  }
  libusb_free_device_list(devs,1);
  libusb_exit(ctx);
  if(!found)fprintf(stderr,"No RX888-class 04b4:00f3/00f1 devices found\n");
  if(upload){
    if(targets==0)fprintf(stderr,"No bootloader devices found; already-loaded devices were not touched.\n");
    return targets>0 && targets==uploaded ? 0 : 1;
  }
  return 0;
}
