#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"


/* the client socket descriptor for the connection to the server */
int cli_sd = -1;


/* attempts to read n bytes from fd; returns true on success and false on
* failure */
static bool nread(int fd, int len, uint8_t *buf) {
 int num_read = 0;
 while( num_read < len ){
   int n = read(fd, &buf[num_read], len - num_read);
   if( n < 0 ){ return false; } // check if read failed
   num_read += n;
 }
 return true;
}


/* attempts to write n bytes to fd; returns true on success and false on
* failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
 int num_write = 0;
 while( num_write < len ){
   int n = write(fd, &buf[num_write], len - num_write);
   if( n < 0 ){ return false; } // check if write failed
   num_write += n;
 }
 return true;
}


/* attempts to receive a packet from fd; returns true on success and false on
* failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {


 uint8_t header[HEADER_LEN];
 uint16_t len;
 int current_location_in_op = 0;


   if (nread(fd, HEADER_LEN, header) == true){


   memcpy(&len, &header[current_location_in_op], sizeof(len));
   current_location_in_op += sizeof(len);
   memcpy(op, &header[current_location_in_op], sizeof(*op));
   current_location_in_op += sizeof(*op);
   memcpy(ret, &header[current_location_in_op], sizeof(*ret));


   if (ntohs(len) == HEADER_LEN){
     return true;
   }
   if(ntohs(len) == HEADER_LEN + 256){
     nread(fd, 256, block);
     return true;
   }


 }
  return false;
}


/* attempts to send a packet to sd; returns true on success and false on
* failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {


 jbod_cmd_t op_cmd = op >> 26;
 uint16_t len = HEADER_LEN;


 if(op_cmd == JBOD_WRITE_BLOCK){
   len += 256;
 }


 uint8_t buf[HEADER_LEN + 256];
 uint16_t big_endian_len = htons(len);
 uint32_t big_endian__op = htonl(op);
 int current_location_in_op = 0;


 memcpy(&buf[current_location_in_op], &big_endian_len, sizeof(big_endian_len));
 current_location_in_op += sizeof(big_endian_len);


 memcpy(&buf[current_location_in_op], &big_endian__op, sizeof(big_endian__op));
 current_location_in_op += sizeof(big_endian__op) + sizeof(uint16_t);


 if(op_cmd == JBOD_WRITE_BLOCK){


   memcpy(&buf[current_location_in_op], block, 256);
   if(nwrite(sd, HEADER_LEN + 256, buf) == true){
     return true;
   }


 }


 // non write or read commands
 if(nwrite(sd, HEADER_LEN, buf) == true){
   return true;
 }


 return false;


}


/* attempts to connect to server and set the global cli_sd variable to the
* socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {


 // create socket
 int sockfd;
 sockfd = socket(PF_INET, SOCK_STREAM, 0);
 if (sockfd == -1) {
   printf( "Error on socket creation \n");
   return false;
 }


 // set up sockaddr object
 struct sockaddr_in caddr;
 caddr.sin_family = AF_INET;
 caddr.sin_port = htons(port);
 if ( inet_aton(ip, &caddr.sin_addr) == 0 ) {
   return false;
 }


 // connect to server
 if ( connect(sockfd, (const struct sockaddr *) &caddr, sizeof(caddr)) == -1 ) {
   return false;
 }


 cli_sd = sockfd;


 return true;
}


/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
 close( cli_sd );
 cli_sd = -1;
}


/* sends the JBOD operation to the server and receives and processes the
* response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
   // printf( "ENTERED CLIENT OPERATION with [%d]\n", op );


   uint16_t returned_packet;
   uint32_t op2;


   if (cli_sd == -1) {
   printf("Client not connected to server.\n");
   return -1;
 }


 if(!send_packet(cli_sd, op, block)){
   printf("Send Packet failed");
 }
 if(!recv_packet(cli_sd, &op2, &returned_packet, block)){
   printf("Recieved Packet Failed");
 }


 return returned_packet;
}
