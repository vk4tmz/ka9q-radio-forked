#include "rx888_capture_sink.h"
#include <assert.h>
#include <string.h>
int main(void){
 FILE *f=tmpfile(); assert(f);
 struct rx888_capture_sink sink={.file=f};
 const int16_t samples[]={-32768,-1,0,1,32767};
 assert(rx888_capture_sink_write(&sink,samples,sizeof samples));
 assert(sink.bytes_written==sizeof samples && sink.failed_writes==0);
 rewind(f);
 int16_t readback[5]={0};
 assert(fread(readback,sizeof readback[0],5,f)==5);
 assert(memcmp(samples,readback,sizeof samples)==0);
 fclose(f);
 puts("PASS: RX888 raw real-s16 capture sink round trip");
 return 0;
}
