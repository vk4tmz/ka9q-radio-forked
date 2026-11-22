/* Offline regression for the Dynamic receiver checkpoint record parser.
 * This deliberately includes the implementation so its static parser can
 * be exercised without starting radiod or opening radio/USB/network devices.
 *
 * Build with -ffunction-sections -fdata-sections -Wl,--gc-sections so the
 * unused radiod integration functions are excluded from the test binary.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/dynamic_state.c"

static void expect_record(char const *line,uint32_t expected_ssrc,
                          uint8_t const *bytes,size_t expected_len){
  dynamic_restore_entry_t entry = {0};
  assert(parse_restore_record(line,&entry) == 0);
  assert(entry.ssrc == expected_ssrc);
  assert(entry.len == expected_len);
  assert(memcmp(entry.payload,bytes,expected_len) == 0);
  free(entry.payload);
}

static void expect_reject(char const *line){
  dynamic_restore_entry_t entry = {0};
  assert(parse_restore_record(line,&entry) < 0);
  free(entry.payload);
}

int main(void){
  uint8_t const payload[] = {0x01,0x02,0xab,0xff};
  expect_record("156800 4 0102abff\n",156800,payload,sizeof(payload));
  expect_record("156800\t4\t0102ABFF\r\n",156800,payload,sizeof(payload));
  expect_reject("");
  expect_reject("not-an-ssrc 4 0102abff");
  expect_reject("4294967296 4 0102abff");
  expect_reject("156800");
  expect_reject("156800 0 ");
  expect_reject("156800 4 0102ab");
  expect_reject("156800 4 0102abff00");
  expect_reject("156800 4 0102abfg");
  expect_reject("156800 4 0102abffx");
  expect_reject("156800 999999999 0102abff");
  puts("PASS: Dynamic checkpoint record parsing and malformed-input rejection");
  return 0;
}
