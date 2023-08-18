#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"
#include "net.h"

int IsMounted = 0; // return true if non zero


uint32_t encode_op(int cmd, int disk_num, int block_num)
{
 uint32_t op = 0;
 op |= cmd << 26;
 op |= disk_num << 22;
 op |= block_num;
 return op;
}


int translate_address(uint32_t addr, int *disk_num, int *block_num, int *offset)
{
 *disk_num = 0;
 *block_num = 0;
 *offset = 0;
 int i;
 for (i = 65535; i < addr; i += 65536)
 {
   *disk_num += 1;
 }
 for (i = 255; i < addr; i += 256)
 {
   *block_num += 1;
 }
 *block_num = *block_num - (*disk_num * 256);
 *offset = addr - (*block_num * 256) - (*disk_num * 65536);
 return 1;
}


// Functions above are helper functions.


int mdadm_mount(void)
{
 if (IsMounted == 1)
 {
   return -1;
 }
 uint32_t op = encode_op(JBOD_MOUNT, 0, 0);
 jbod_client_operation(op, NULL);
 IsMounted = 1;
 return 1;
}


int mdadm_unmount(void)
{
 if (IsMounted == 0)
 {
   return -1;
 }
 uint32_t op = encode_op(JBOD_UNMOUNT, 0, 0);
 jbod_client_operation(op, NULL);
 IsMounted = 0;
 return 1;
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
{
 if (len == 0)
 {
   return 0;
 }
 if (IsMounted == 0 || len > 1024 || len + addr > 1048576 || buf == NULL)
 {
   return -1;
 }


 int disk_num;
 int block_num;
 int offset;
 uint8_t myBuf[256];


 int i = 0;
 for (i = 0; i < len; i++)
 {
   translate_address(addr + i, &disk_num, &block_num, &offset);
   if( cache_lookup(disk_num, block_num, myBuf) == 1){
     memcpy(buf, myBuf, JBOD_BLOCK_SIZE);
     return len;
   }
   uint32_t op1 = encode_op(JBOD_SEEK_TO_DISK, disk_num, block_num);
   jbod_client_operation(op1, NULL);
   uint32_t op2 = encode_op(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
   jbod_client_operation(op2, NULL);
   uint32_t op3 = encode_op(JBOD_READ_BLOCK, disk_num, block_num);
   jbod_client_operation(op3, myBuf);
   // printf("addr + i = %u, disk_num = %d, block_num = %d,./tester -w traces/random-input >x offset = %d\n", addr + i, disk_num, block_num, offset);
   // printf("\n");
   memcpy(buf + i, myBuf + offset, 1);


   // the commented code below makes me more effcient by reading the whole block if possible ( but i like my older simple slow method of bytee by byte)
   // int remaining = len - i < JBOD_BLOCK_SIZE - offset ? len - i : JBOD_BLOCK_SIZE - offset;
   // memcpy(buf + i, myBuf + offset, remaining);
   // i = i + remaining - 1;


   cache_insert(disk_num, block_num, buf);
 }


 return len;
}


int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf)
{
 if (len == 0)
 {
   return 0;
 }
 if (IsMounted == 0 || len > 1024 || len + addr > 1048576 || buf == NULL)
 {
   return -1;
 }


 int disk_num;
 int block_num;
 int offset;
 uint8_t myBuffer[256];


 // write 1 byte at a time
 int i = 0;
 for (i = 0; i < len; i++)
 {
   bool isCacheable = false;


   // Reach to the correct disk and block space
   translate_address(addr + i, &disk_num, &block_num, &offset);
  
   if( cache_lookup(disk_num, block_num, myBuffer) == 1){
     isCacheable = true;
   }


   if(isCacheable == false){
   uint32_t op1 = encode_op(JBOD_SEEK_TO_DISK, disk_num, block_num);
   jbod_client_operation(op1, NULL);
   uint32_t op2 = encode_op(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
   jbod_client_operation(op2, NULL);


   // save the block content to a temporary buffer:
   uint32_t op3 = encode_op(JBOD_READ_BLOCK, disk_num, block_num);
   jbod_client_operation(op3, myBuffer);
   }


   // change to new content
   myBuffer[offset] = buf[i]; // could also be done like: memcpy(myBuffer + offset, buf + i, 1);


   // relocate to correct disk and block
   uint32_t op4 = encode_op(JBOD_SEEK_TO_DISK, disk_num, block_num);
   jbod_client_operation(op4, NULL);
   uint32_t op5 = encode_op(JBOD_SEEK_TO_BLOCK, disk_num, block_num);
   jbod_client_operation(op5, NULL);


   // write to the correct address
   uint32_t op6 = encode_op(JBOD_WRITE_BLOCK, disk_num, block_num);
   jbod_client_operation(op6, myBuffer);
 }


 return len;
}


