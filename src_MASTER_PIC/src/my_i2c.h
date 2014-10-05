#ifndef __my_i2c_h
#define __my_i2c_h

#include "messages.h"

#define MAXI2CBUF MSGLEN
typedef struct __i2c_comm {
    unsigned char buffer[MAXI2CBUF];
    unsigned char buflen;
    unsigned char bufind;
    unsigned char event_count;
    unsigned char status;
    unsigned char error_code;
    unsigned char error_count;
    unsigned char outbuffer[MAXI2CBUF];
    unsigned char outbuflen;
    unsigned char outbufind;
    unsigned char slave_addr;

    unsigned char sendBuf[MAXI2CBUF];
    unsigned char sendBufLen;
    unsigned char sendBufInd;

} i2c_comm;


#define I2C_IDLE 0x5
#define I2C_STARTED 0x6
#define	I2C_RCV_DATA 0x7
#define I2C_SLAVE_SEND 0x8

#define I2C_MASTER_SEND_STARTED 0x10
#define I2C_MASTER_SEND_DONE 0x11
#define I2C_MASTER_RECV_STARTED 0x20
#define I2C_MASTER_RECV_ACK 0x21
#define I2C_MASTER_RECV_DATA 0x22


#define I2C_MASTER_SEND_START 0x30
#define I2C_MASTER_RECV_LAST_DATA 0x31
#define I2C_MASTER_RECV_SEND_ACK 0x32
#define I2C_MASTER_RECV_SET_RCEN 0x33


#define I2C_ERR_THRESHOLD 1
#define I2C_ERR_OVERRUN 0x4
#define I2C_ERR_NOADDR 0x5
#define I2C_ERR_NODATA 0x6
#define I2C_ERR_MSGTOOLONG 0x7
#define I2C_ERR_MSG_TRUNC 0x8


#define I2C_MODE_MASTER 100
#define I2C_MODE_SLAVE 200

int isSending();
void Handle_i2c_data_save(unsigned char length,unsigned char *msg);
void SetDatapoint(char data, int position);
void init_i2c(i2c_comm *);
void i2c_int_handler(void);
void start_i2c_slave_reply(unsigned char,unsigned char *);
void i2c_configure_slave(unsigned char);
void i2c_configure_master(unsigned char);
unsigned char i2c_master_send(unsigned char,unsigned char *);
unsigned char i2c_master_recv(unsigned char);

void i2c_master_int_handler(void);
void i2c_slave_int_handler(void);

unsigned int I2C_Buffer_Size = 23;

#endif