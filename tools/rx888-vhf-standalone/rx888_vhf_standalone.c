/* Standalone RX888 USB discovery and explicitly gated FX3 firmware upload.
 * List and clocks are passive; probe explicitly claims interface 0 for a read-only TESTFX3 request.
 * Upload is disruptive: only available with --upload and --firmware together.
 */
#include <libusb-1.0/libusb.h>
#include <errno.h>
#include <math.h>
#include "rx888_vhf_calibration.h"
#include "si5351.h"
#include "rx888_vhf_clock_usb.h"
#include "rx888_vhf_i2c_echo.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "ezusb.h"

int Ezusb_verbose = 0; /* required by ezusb.c */

static void usage(const char *argv0) {
  fprintf(stderr,
    "Usage: %s list [--reference HZ | --ppm PPM]\n"
    "       %s clocks [--reference HZ | --ppm PPM] [--adc-rate HZ] [--tuner-ref HZ]\n"
    "       %s probe --bus N --address N --exclusive [--read-i2c]\n"
    "       %s inspect-clocks --bus N --address N --exclusive\n"
    "       %s echo-reg3 --bus N --address N --exclusive --write-reg3\n"
    "       %s clock-test --bus N --address N --exclusive --write-clocks --ppm PPM\n"
    "       %s upload --firmware /absolute/path/SDDC_FX3.img --upload\n"
    "\nlist: passive USB enumeration; no interface claim, reset or control commands.\n"
    "probe: with --read-i2c, also reads Si5351 status and R828D status (no writes).\n"
    "upload: destructive to receiver state; only FX3 bootloader 04b4:00f3.\n",
    argv0, argv0, argv0, argv0, argv0, argv0, argv0);
}
static bool parse_number(const char *s,double *out) {
  char *end=NULL;
  errno=0;
  double d=strtod(s,&end);
  if(!s[0] || end==s || *end!='\0' || errno==ERANGE || !isfinite(d)) return false;
  *out=d;return true;
}
static bool valid_firmware(const char *path) {
  struct stat st;
  return path && path[0] == '/' && stat(path,&st) == 0 &&
         S_ISREG(st.st_mode) && st.st_size > 0;
}
int main(int argc,char **argv) {
  bool list = argc >= 2 && strcmp(argv[1],"list")==0;
  bool clocks = argc >= 2 && strcmp(argv[1],"clocks")==0;
  bool probe = argc >= 2 && strcmp(argv[1],"probe")==0;
  bool inspect_clocks = argc >= 2 && strcmp(argv[1],"inspect-clocks")==0;
  bool echo_reg3 = argc >= 2 && strcmp(argv[1],"echo-reg3")==0;
  bool write_reg3=false;
  bool clock_test = argc >= 2 && strcmp(argv[1],"clock-test")==0;
  bool write_clocks=false;
  bool upload = argc >= 2 && strcmp(argv[1],"upload")==0;
  bool confirmed = false;
  bool read_i2c = false;
  bool have_ref=false,have_ppm=false;
  double reference=27000000,ppm=0,adc_rate=20000000,tuner_ref=16000000;
  bool have_adc=false,have_tuner=false;
  const char *firmware = NULL;
  if(!list && !clocks && !upload && !probe && !clock_test && !inspect_clocks && !echo_reg3){usage(argv[0]);return 2;}
  if(list || clocks) {
    for(int i=2;i<argc;i++){
      if(strcmp(argv[i],"--reference")==0 && !have_ref && i+1<argc) {
        have_ref=parse_number(argv[++i],&reference);
        if(!have_ref || reference<=0) {usage(argv[0]);return 2;}
      } else if(strcmp(argv[i],"--ppm")==0 && !have_ppm && i+1<argc) {
        have_ppm=parse_number(argv[++i],&ppm);
        if(!have_ppm || fabs(ppm)>100) {usage(argv[0]);return 2;}
      } else if(clocks && strcmp(argv[i],"--adc-rate")==0 && !have_adc && i+1<argc) {
        have_adc=parse_number(argv[++i],&adc_rate);
        if(!have_adc || adc_rate<1e6 || adc_rate>130e6) {usage(argv[0]);return 2;}
      } else if(clocks && strcmp(argv[i],"--tuner-ref")==0 && !have_tuner && i+1<argc) {
        have_tuner=parse_number(argv[++i],&tuner_ref);
        if(!have_tuner || tuner_ref<=0) {usage(argv[0]);return 2;}
      } else {usage(argv[0]);return 2;}
    }
    if(have_ref && have_ppm) {fprintf(stderr,"--reference and --ppm are mutually exclusive\n");return 2;}
    double actual=have_ref?reference:rx888_calibrated_reference(27000000,ppm);
    if(!isfinite(actual))return 2;
    printf("calibration nominal_reference_hz=27000000 measured_reference_hz=%.6f ppm=%+.6f source=%s (report only; hardware unchanged)\n",
       actual,rx888_reference_ppm(27000000,actual),have_ref?"reference":have_ppm?"ppm":"default");
  }
  if(clocks) {
    double actual=have_ref?reference:rx888_calibrated_reference(27000000,ppm);
    for(int i=0;i<2;i++){
      double target=i==0?adc_rate:tuner_ref;
      si5351_solution_t s={0};
      if(!si5351_solve(actual,target,&s)) {
        fprintf(stderr,"Si5351 solver failed for %.3f Hz\n",target);
        return 1;
      }
      si5351_pvals_t pll={0},ms={0};
      si5351_get_pll_pvals(&s,&pll);
      si5351_get_ms_pvals(&s,&ms);
      double achieved=actual*(s.A+(double)s.B/s.C)/
        ((s.D+(double)s.E/s.F)*s.R);
      printf("%s target_hz=%.6f achieved_hz=%.6f error_hz=%+.6f PLL=%u+%u/%u MS=%u+%u/%u R=%u PLL_P1=%u P2=%u P3=%u MS_P1=%u P2=%u P3=%u (not programmed)\n",
        i==0?"adc":"tuner_ref",target,achieved,achieved-target,
        s.A,s.B,s.C,s.D,s.E,s.F,s.R,
        pll.P1,pll.P2,pll.P3,ms.P1,ms.P2,ms.P3);
    }
    return 0;
  }
  int probe_bus=-1,probe_address=-1;
  if(probe || clock_test || inspect_clocks || echo_reg3) {
    for(int i=2;i<argc;i++) {
      double d=0;
      if(strcmp(argv[i],"--exclusive")==0 && !confirmed)confirmed=true;
      else if(probe && strcmp(argv[i],"--read-i2c")==0 && !read_i2c)read_i2c=true;
      else if(echo_reg3 && strcmp(argv[i],"--write-reg3")==0 && !write_reg3)write_reg3=true;
      else if(clock_test && strcmp(argv[i],"--write-clocks")==0 && !write_clocks)write_clocks=true;
      else if(clock_test && strcmp(argv[i],"--ppm")==0 && !have_ppm && i+1<argc){
        have_ppm=parse_number(argv[++i],&ppm);
        if(!have_ppm || fabs(ppm)>100){usage(argv[0]);return 2;}
      }
      else if(strcmp(argv[i],"--bus")==0 && probe_bus<0 && i+1<argc &&
              parse_number(argv[++i],&d) && d>=1 && d<=255 && floor(d)==d)probe_bus=(int)d;
      else if(strcmp(argv[i],"--address")==0 && probe_address<0 && i+1<argc &&
              parse_number(argv[++i],&d) && d>=1 && d<=127 && floor(d)==d)probe_address=(int)d;
      else {usage(argv[0]);return 2;}
    }
    if(!confirmed || probe_bus<0 || probe_address<0 ||
       (clock_test && (!write_clocks || !have_ppm)) || (echo_reg3 && !write_reg3)){
      fprintf(stderr,"Refusing: --exclusive, --bus N, --address N required; clock-test also requires --write-clocks and --ppm.\n");
      return 2;
    }
  }
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
    if(probe || clock_test || inspect_clocks || echo_reg3) {
      if((int)libusb_get_bus_number(devs[i])!=probe_bus ||
         (int)libusb_get_device_address(devs[i])!=probe_address)continue;
      targets++;
      if(desc.idProduct!=0x00f1){
        fprintf(stderr,"Probe refuses bootloader; firmware must be loaded.\n");
        continue;
      }
      libusb_device_handle *handle=NULL;
      rc=libusb_open(devs[i],&handle);
      if(rc!=0 || !handle){
        fprintf(stderr,"Probe open refused: %s\n",libusb_error_name(rc));
        continue;
      }
      rc=libusb_kernel_driver_active(handle,0);
      if(rc!=0){
        fprintf(stderr,"Probe refuses active/unknown kernel driver status (%d).\n",rc);
        libusb_close(handle);
        continue;
      }
      rc=libusb_claim_interface(handle,0);
      if(rc!=0){
        fprintf(stderr,"Probe claim refused: %s (stop receiver applications first).\n",
          libusb_error_name(rc));
        libusb_close(handle);
        continue;
      }
      uint8_t info[4]={0};
      rc=libusb_control_transfer(handle,
        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
        TESTFX3,0,0,info,sizeof info,1000);
      if(rc==4){
        printf("probe firmware hw=0x%02x version=%u.%u requests=%u (TESTFX3 only)\n",
          info[0],info[1],info[2],info[3]);
        bool success=true;
        if(echo_reg3){
          if(info[0]!=0x04 || info[1]!=2 || info[2]<2){
            fprintf(stderr,"echo-reg3 refuses unverified firmware protocol version.\n");
            success=false;
          }else{
            rx888_clock_io io={.ctx=handle,.read=rx888_usb_clock_read,
              .write=rx888_usb_clock_write,.delay=NULL};
            uint8_t before=0xff,after=0xff;
            int echo_rc=rx888_clock_echo_reg3(&io,&before,&after);
            printf("echo-reg3 rc=%d before=0x%02x after=0x%02x\n",echo_rc,before,after);
            if(echo_rc){fprintf(stderr,"Echo failed or register unsafe; hardware state must be checked.\n");success=false;}
          }
        }
        if(inspect_clocks){
          rx888_clock_io io={.ctx=handle,.read=rx888_usb_clock_read,
             .write=NULL,.delay=NULL};
          rx888_clock_snapshot snapshot={0};
          uint8_t status=0,interrupt_status=0;
          int status_rc=rx888_usb_clock_read(handle,SI5351_REGISTER_STATUS,&status);
          int interrupt_rc=rx888_usb_clock_read(handle,1,&interrupt_status);
          int snapshot_rc=rx888_clock_take_snapshot(&io,&snapshot);
          if(status_rc || interrupt_rc || snapshot_rc){
            fprintf(stderr,"inspect-clocks read failure: status=%d interrupt=%d snapshot=%d\n",
              status_rc,interrupt_rc,snapshot_rc);
            success=false;
          }else{
            printf("si5351 status=0x%02x SYS_INIT=%u LOL_A=%u LOL_B=%u LOS=%u\n",
              status,(unsigned)!!(status&0x80),(unsigned)!!(status&0x20),
              (unsigned)!!(status&0x40),(unsigned)!!(status&0x10));
            printf("si5351 interrupt_status=0x%02x\n",interrupt_status);
            printf("si5351 output_enable_reg3=0x%02x output_mask_reg9=0x%02x pll_source_reg15=0x%02x\n",
              snapshot.output_enable,snapshot.output_mask,snapshot.source);
            printf("si5351 clk0_ctrl_reg16=0x%02x enabled=%u powerdown=%u\n",
              snapshot.clk0,(unsigned)!(snapshot.output_enable&1),
              (unsigned)!!(snapshot.clk0&0x80));
            printf("si5351 clk2_ctrl_reg18=0x%02x enabled=%u powerdown=%u\n",
              snapshot.clk2,(unsigned)!(snapshot.output_enable&4),
              (unsigned)!!(snapshot.clk2&0x80));
            const uint8_t *blocks[]={snapshot.pll_a,snapshot.pll_b,snapshot.ms0,snapshot.ms2};
            const char *names[]={"plla","pllb","ms0","ms2"};
            const unsigned bases[]={SI5351_REGISTER_MSNA_BASE,SI5351_REGISTER_MSNB_BASE,
              SI5351_REGISTER_MS0_BASE,SI5351_REGISTER_MS2_BASE};
            for(unsigned j=0;j<4;j++){
              printf("si5351 %s reg%u..%u =",names[j],bases[j],bases[j]+7);
              for(unsigned k=0;k<8;k++)printf(" %02x",blocks[j][k]);
              putchar('\n');
            }
            printf("inspect-clocks read-only complete; no I2C writes\n");
          }
        }
        if(clock_test){
          if(info[0]!=0x04 || info[1]!=2 || info[2]<2){
            fprintf(stderr,"Clock test refuses unverified firmware identity.\n");
            success=false;
          }else{
            rx888_clock_io io={.ctx=handle,.read=rx888_usb_clock_read,
                .write=rx888_usb_clock_write,.delay=rx888_usb_clock_delay};
            rx888_clock_snapshot original={0};
            rx888_si5351_plan adc={0},tuner={0};
            double ref=rx888_calibrated_reference(27000000,ppm);
            if(!rx888_si5351_plan_clock(ref,20000000,&adc) ||
               !rx888_si5351_plan_clock(ref,16000000,&tuner)){
              fprintf(stderr,"Clock test planning failed, hardware unchanged.\n");
              success=false;
            }else if(rx888_clock_take_snapshot(&io,&original)!=0){
              fprintf(stderr,"Clock snapshot failed; refusing any writes.\n");
              success=false;
            }else{
              printf("clock-test snapshot complete; ADC=%.3f tuner=%.3f reference=%.3f\n",
                     adc.achieved_hz,tuner.achieved_hz,ref);
              fflush(stdout);
              int program_rc=rx888_clock_program(&io,&adc,&tuner);
              int restore_rc=rx888_clock_restore(&io,&original);
              int verify_rc=rx888_clock_verify_restore(&io,&original);
              printf("clock-test program_rc=%d restore_rc=%d verify_rc=%d\n",program_rc,restore_rc,verify_rc);
              if(program_rc || restore_rc || verify_rc){
                fprintf(stderr,"Clock test failed or restoration uncertain; do not restart receiver until checked.\n");
                success=false;
              }
            }
          }
        }
        if(read_i2c){
          uint8_t si=0;
          int n=libusb_control_transfer(handle,
            LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
            I2CRFX3,SI5351_ADDR,SI5351_REGISTER_STATUS,&si,1,1000);
          if(n==1)printf("i2c Si5351 status reg=0x00 raw=0x%02x\n",si);
          else {fprintf(stderr,"Si5351 status read failed (%d)\n",n);success=false;}
          uint8_t st[5]={0};
          n=libusb_control_transfer(handle,
            LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
            I2CRFX3,R828D_ADDR,0,st,sizeof st,1000);
          if(n==5){
            printf("i2c R828D status raw=");
            for(size_t k=0;k<sizeof st;k++)printf(" %02x",st[k]);
            printf(" decoded=");
            for(size_t k=0;k<sizeof st;k++){
              uint8_t b=st[k];b=(uint8_t)((b>>4)|(b<<4));
              b=(uint8_t)(((b&0xcc)>>2)|((b&0x33)<<2));
              b=(uint8_t)(((b&0xaa)>>1)|((b&0x55)<<1));
              printf(" %02x",b);
            }
            putchar('\n');
          }else {fprintf(stderr,"R828D status read failed (%d)\n",n);success=false;}
        }
        if(success)uploaded++;
      }else fprintf(stderr,"Probe TESTFX3 failed: %s (%d bytes)\n",
        rc<0?libusb_error_name(rc):"short reply",rc);
      int release=libusb_release_interface(handle,0);
      if(release!=0)fprintf(stderr,"Probe interface release error: %s\n",libusb_error_name(release));
      libusb_close(handle);
      continue;
    }
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
  if(probe || clock_test || inspect_clocks || echo_reg3){
    if(targets==0)fprintf(stderr,"Probe target not found at selected bus/address.\n");
    return targets==1 && uploaded==1 ? 0 : 1;
  }
  if(upload){
    if(targets==0)fprintf(stderr,"No bootloader devices found; already-loaded devices were not touched.\n");
    return targets>0 && targets==uploaded ? 0 : 1;
  }
  return 0;
}
