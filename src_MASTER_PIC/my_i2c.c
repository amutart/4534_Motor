#include "maindefs.h"
#ifndef __XC8
#include <i2c.h>
#else
#include <plib/i2c.h>
#endif
#include "my_i2c.h"
#include "interrupts.h"
#include "debug.h"

int Send = 0;
static i2c_comm *ic_ptr;

#define I2C_MODE_MASTER 1
#define I2C_MODE_SLAVE 2
static unsigned char i2c_mode = 0;

// Configure for I2C Master mode -- the variable "slave_addr" should be stored in
//   i2c_comm (as pointed to by ic_ptr) for later use.
int isSending()
{
    return ic_ptr->status == I2C_SLAVE_SEND;
}
void i2c_configure_master(unsigned char slave_addr) {
    i2c_mode = I2C_MODE_MASTER;
    // Page 176
    ic_ptr->slave_addr = slave_addr;

    // Clearing the registers
    SSPCON1 = 0x0;
    SSPCON2 = 0x0;
    SSPSTAT = 0x0;

    //Configure SSPxADD for the I2C Clock
    SSPADD = 0x63;	 //100kHz Baud clock(0x63) @40MHz

    // Configred into I2C Master mode, clock = FOSC/(4* (SSPxADD + 1))
    SSPCON1bits.SSPM = 0x08;
    SSPCON1bits.SSPEN = 1;

    // What is SLEW?  Turned it off anyways
    SSPSTAT = SLEW_OFF;

    // Turning on bits of LATC to set SCL1/SDA1 on
    LATCbits.LATC3 = 1;
    LATCbits.LATC4 = 1;
    TRISCbits.TRISC3 = 1;
    TRISCbits.TRISC4 = 1;
    // TRISBbits.TRISB4 = 1;
    //TRISBbits.TRISB5 = 1;
    //LATBbits.LATB4 = 1;
    //LATBbits.LATB5 = 1;
}

// Sending in I2C Master mode [slave write]
// 		returns -1 if the i2c bus is busy
// 		return 0 otherwise
// Will start the sending of an i2c message -- interrupt handler will take care of
//   completing the message send.  When the i2c message is sent (or the send has failed)
//   the interrupt handler will send an internal_message of type MSGT_MASTER_SEND_COMPLETE if
//   the send was successful and an internal_message of type MSGT_MASTER_SEND_FAILED if the
//   send failed (e.g., if the slave did not acknowledge).  Both of these internal_messages
//   will have a length of 0.
// The subroutine must copy the msg to be sent from the "msg" parameter below into
//   the structure to which ic_ptr points [there is already a suitable buffer there].

unsigned char i2c_master_send(unsigned char length, unsigned char *msg) {

    // See page 160, if the READ_WRITE register is 1 then a transmit is in progress
    if (SSPSTATbits.READ_WRITE == 1){
        return -1;      //returns -1 if the i2c bus is busy
    }

    // Sending the buffer using ic_ptr
    unsigned int currentLength;
    for (currentLength = 0; currentLength < length; currentLength++){
        ic_ptr->outbuffer[currentLength] = msg[currentLength];
    }

    ic_ptr->outbuflen = length;
    ic_ptr->outbufind = 0;

    // Start the I2C transfer
    // See page 162 - SSPxCON2 is the MSSPx Control Register 2
    SSPCON2bits.RCEN = 0;   // Enables receive mode for I2C (Master recieve mode only)
    SSPCON2bits.SEN = 1;    // Initiate start condition on the SDAx and SCLx pins

    ic_ptr->status = I2C_MASTER_SEND_STARTED;

    return(0);
}

// Receiving in I2C Master mode [slave read]
// 		returns -1 if the i2c bus is busy
// 		return 0 otherwise
// Will start the receiving of an i2c message -- interrupt handler will take care of
//   completing the i2c message receive.  When the receive is complete (or has failed)
//   the interrupt handler will send an internal_message of type MSGT_MASTER_RECV_COMPLETE if
//   the receive was successful and an internal_message of type MSGT_MASTER_RECV_FAILED if the
//   receive failed (e.g., if the slave did not acknowledge).  In the failure case
//   the internal_message will be of length 0.  In the successful case, the
//   internal_message will contain the message that was received [where the length
//   is determined by the parameter passed to i2c_master_recv()].
// The interrupt handler will be responsible for copying the message received into

unsigned char i2c_master_recv(unsigned char length) {
    // See page 160, if the READ_WRITE register is 1 then a transmit is in progress
    if (SSPSTATbits.READ_WRITE == 1){
        return -1;      //returns -1 if the i2c bus is busy
    }

    ic_ptr->buflen = length;
    ic_ptr->bufind = 0;
    
    //SSPCON2bits.RCEN = 1;
    SSPCON2bits.SEN = 1;

    ic_ptr->status = I2C_MASTER_RECV_STARTED;

    return(0);
}

void Handle_i2c_data_save(unsigned char length,unsigned char *msg)
{
    if(ic_ptr->sendBufLen < I2C_Buffer_Size)
    {
     ic_ptr->sendBuf[ic_ptr->sendBufLen] = *msg;
     ic_ptr->sendBufLen++;
    }
}

void start_i2c_slave_reply(unsigned char length, unsigned char *msg) {



    for (ic_ptr->outbuflen = 0; ic_ptr->outbuflen < length; ic_ptr->outbuflen++) {


        if(ic_ptr->outbuflen < ic_ptr->sendBufLen)
        {
        ic_ptr->outbuffer[ic_ptr->outbuflen] = ic_ptr->sendBuf[ic_ptr->outbuflen];
        }
        else if(ic_ptr->sendBufLen != 0)
        {
            ic_ptr->outbuffer[ic_ptr->outbuflen] = ic_ptr->sendBuf[ic_ptr->sendBufLen-1];
        }
        else
        {
             ic_ptr->outbuffer[ic_ptr->outbuflen] = ic_ptr->sendBuf[length - 1];
        }

    }
    ic_ptr->sendBufLen =0;
    ic_ptr->outbuflen = length;
    ic_ptr->outbufind = 1; // point to the second byte to be sent

    // put the first byte into the I2C peripheral
    SSPBUF = ic_ptr->outbuffer[0];
    // we must be ready to go at this point, because we'll be releasing the I2C
    // peripheral which will soon trigger an interrupt
    SSPCON1bits.CKP = 1;

}

// an internal subroutine used in the slave version of the i2c_int_handler

 void handle_start(unsigned char data_read) {
    ic_ptr->event_count = 1;
    ic_ptr->buflen = 0;
    // check to see if we also got the address
    if (data_read) {
        if (SSPSTATbits.D_A == 1) {
            // this is bad because we got data and
            // we wanted an address
            ic_ptr->status = I2C_IDLE;
            ic_ptr->error_count++;
            ic_ptr->error_code = I2C_ERR_NOADDR;
        } else {
            if (SSPSTATbits.R_W == 1) {
                ic_ptr->status = I2C_SLAVE_SEND;
            } else {
                ic_ptr->status = I2C_RCV_DATA;
            }
        }
    } else {
        ic_ptr->status = I2C_STARTED;
    }
}

// this is the interrupt handler for i2c -- it is currently built for slave mode
// -- to add master mode, you should determine (at the top of the interrupt handler)
//    which mode you are in and call the appropriate subroutine.  The existing code
//    below should be moved into its own "i2c_slave_handler()" routine and the new
//    master code should be in a subroutine called "i2c_master_handler()"

void i2c_int_handler() {
    switch (i2c_mode){
        case I2C_MODE_MASTER: {
            i2c_master_int_handler();
            break;
        }
        case I2C_MODE_SLAVE: {
            i2c_slave_int_handler();
            break;
        }
        default: {
            // mode was not set, ERROR
            break;
        }
    }
}

void i2c_slave_int_handler() {
    unsigned char i2c_data;
    unsigned char data_read = 0;
    unsigned char data_written = 0;
    unsigned char msg_ready = 0;
    unsigned char msg_to_send = 0;
    unsigned char overrun_error = 0;
    unsigned char error_buf[3];

    // clear SSPOV
    if (SSPCON1bits.SSPOV == 1) {
        SSPCON1bits.SSPOV = 0;
        // we failed to read the buffer in time, so we know we
        // can't properly receive this message, just put us in the
        // a state where we are looking for a new message
        ic_ptr->status = I2C_IDLE;
        overrun_error = 1;
        ic_ptr->error_count++;
        ic_ptr->error_code = I2C_ERR_OVERRUN;
    }
    // read something if it is there
    if (SSPSTATbits.BF == 1) {
        i2c_data = SSPBUF;
        data_read = 1;
    }

    if (!overrun_error) {
        switch (ic_ptr->status) {
            case I2C_IDLE:
            {
                // ignore anything except a start
                if (SSPSTATbits.S == 1) {
                    handle_start(data_read);
                    // if we see a slave read, then we need to handle it here
                    if (ic_ptr->status == I2C_SLAVE_SEND) {
                        data_read = 0;
                        msg_to_send = 1;
                    }
                }
                break;
            }
            case I2C_STARTED:
            {
                // in this case, we expect either an address or a stop bit
                if (SSPSTATbits.P == 1) {
                    // we need to check to see if we also read an
                    // address (a message of length 0)
                    ic_ptr->event_count++;
                    if (data_read) {
                        if (SSPSTATbits.D_A == 0) {
                            msg_ready = 1;
                        } else {
                            ic_ptr->error_count++;
                            ic_ptr->error_code = I2C_ERR_NODATA;
                        }
                    }
                    ic_ptr->status = I2C_IDLE;
                } else if (data_read) {
                    ic_ptr->event_count++;
                    if (SSPSTATbits.D_A == 0) {
                        if (SSPSTATbits.R_W == 0) { // slave write
                            ic_ptr->status = I2C_RCV_DATA;
                        } else { // slave read
                            ic_ptr->status = I2C_SLAVE_SEND;
                            msg_to_send = 1;
                            // don't let the clock stretching bit be let go
                            data_read = 0;
                        }
                    } else {
                        ic_ptr->error_count++;
                        ic_ptr->status = I2C_IDLE;
                        ic_ptr->error_code = I2C_ERR_NODATA;
                    }
                }
                break;
            }
            case I2C_SLAVE_SEND:
            {
                if (ic_ptr->outbufind < ic_ptr->outbuflen) {
                    SSPBUF = ic_ptr->outbuffer[ic_ptr->outbufind];
                    ic_ptr->outbufind++;
                    data_written = 1;
                } else {
                    // we have nothing left to send
                    ic_ptr->status = I2C_IDLE;
                }
                break;
            }
            case I2C_RCV_DATA:
            {
                // we expect either data or a stop bit or a (if a restart, an addr)
                if (SSPSTATbits.P == 1) {
                    // we need to check to see if we also read data
                    ic_ptr->event_count++;
                    if (data_read) {
                        if (SSPSTATbits.D_A == 1) {
                            ic_ptr->buffer[ic_ptr->buflen] = i2c_data;
                            ic_ptr->buflen++;
                            msg_ready = 1;
                        } else {
                            ic_ptr->error_count++;
                            ic_ptr->error_code = I2C_ERR_NODATA;
                            ic_ptr->status = I2C_IDLE;
                        }
                    } else {
                        msg_ready = 1;
                    }
                    ic_ptr->status = I2C_IDLE;
                } else if (data_read) {
                    ic_ptr->event_count++;
                    if (SSPSTATbits.D_A == 1) {
                        ic_ptr->buffer[ic_ptr->buflen] = i2c_data;
                        ic_ptr->buflen++;
                    } else /* a restart */ {
                        if (SSPSTATbits.R_W == 1) {
                            ic_ptr->status = I2C_SLAVE_SEND;
                            msg_ready = 1;
                            msg_to_send = 1;
                            // don't let the clock stretching bit be let go
                            data_read = 0;
                        } else { /* bad to recv an address again, we aren't ready */
                            ic_ptr->error_count++;
                            ic_ptr->error_code = I2C_ERR_NODATA;
                            ic_ptr->status = I2C_IDLE;
                        }
                    }
                }
                break;
            }
        }
    }

    // release the clock stretching bit (if we should)
    if (data_read || data_written) {
        // release the clock
        if (SSPCON1bits.CKP == 0) {
            SSPCON1bits.CKP = 1;
        }
    }

    // must check if the message is too long, if
    if ((ic_ptr->buflen > MAXI2CBUF - 2) && (!msg_ready)) {
        ic_ptr->status = I2C_IDLE;
        ic_ptr->error_count++;
        ic_ptr->error_code = I2C_ERR_MSGTOOLONG;
    }

    if (msg_ready) {
        ic_ptr->buffer[ic_ptr->buflen] = ic_ptr->event_count;
        ToMainHigh_sendmsg(ic_ptr->buflen + 1, MSGT_I2C_DATA, (void *) ic_ptr->buffer);
        ic_ptr->buflen = 0;
    } else if (ic_ptr->error_count >= I2C_ERR_THRESHOLD) {
        error_buf[0] = ic_ptr->error_count;
        error_buf[1] = ic_ptr->error_code;
        error_buf[2] = ic_ptr->event_count;
        ToMainHigh_sendmsg(sizeof (unsigned char) *3, MSGT_I2C_DBG, (void *) error_buf);
        ic_ptr->error_count = 0;
    }
    if (msg_to_send) {
        // send to the queue to *ask* for the data to be sent out

        start_i2c_slave_reply(I2C_Buffer_Size, 0);
        msg_to_send = 0;
    }
}

// set up the data structures for this i2c code
// should be called once before any i2c routines are called

void init_i2c(i2c_comm *ic) {
    ic_ptr = ic;
    ic_ptr->buffer[0] = 0;
    ic_ptr->buffer[1] = 0;
    ic_ptr->buffer[2] = 0;
    ic_ptr->buflen = 0;
    ic_ptr->event_count = 0;
    ic_ptr->status = I2C_IDLE;
    ic_ptr->error_count = 0;
    ic_ptr->sendBufInd = 0;
    ic_ptr->sendBufLen = 0;
}

// setup the PIC to operate as a slave
// the address must include the R/W bit

void i2c_configure_slave(unsigned char addr) {

    // ensure the two lines are set for input (we are a slave)
#ifdef __USE18F26J50
    //THIS CODE LOOKS WRONG, SHOULDN'T IT BE USING THE TRIS BITS???
    PORTBbits.SCL1 = 1;
    PORTBbits.SDA1 = 1;
#else
#ifdef __USE18F46J50
    TRISBbits.TRISB4 = 1; //RB4 = SCL1
    TRISBbits.TRISB5 = 1; //RB5 = SDA1
#else
    TRISCbits.TRISC3 = 1;
    TRISCbits.TRISC4 = 1;
#endif
#endif

    // set the address
    SSPADD = addr;
    //OpenI2C(SLAVE_7,SLEW_OFF); // replaced w/ code below
    SSPSTAT = 0x0;
    SSPCON1 = 0x0;
    SSPCON2 = 0x0;
    SSPCON1 |= 0x0E; // enable Slave 7-bit w/ start/stop interrupts
    SSPSTAT |= SLEW_OFF;

#ifdef I2C_V3
    I2C1_SCL = 1;
    I2C1_SDA = 1;
#else
#ifdef I2C_V1
    I2C_SCL = 1;
    I2C_SDA = 1;
#else
#ifdef __USE18F26J50
    PORTBbits.SCL1 = 1;
    PORTBbits.SDA1 = 1;
#else
#ifdef __USE18F46J50
    PORTBbits.SCL1 = 1;
    PORTBbits.SDA1 = 1;
#else
    __dummyXY=35;// Something is messed up with the #ifdefs; this line is designed to invoke a compiler error
#endif
#endif
#endif
#endif
    i2c_mode = I2C_MODE_SLAVE;
    // enable clock-stretching
    SSPCON2bits.SEN = 1;
    SSPCON1 |= SSPENB;
    // end of i2c configure
}

void i2c_master_int_handler(){
    switch (ic_ptr->status){
        // Page 183 - I2C Master Mode Waveform (Transfer)
        case (I2C_MASTER_SEND_STARTED): {
            ic_ptr->status = I2C_MASTER_SEND_START;
            SSPBUF = ((ic_ptr->slave_addr << 1) & 0xFE);
            break;
        }
        case (I2C_MASTER_SEND_START): {
            if (SSPCON2bits.ACKSTAT == 0) {
                if (ic_ptr->outbufind < ic_ptr->outbuflen) {
                    SSPBUF = ic_ptr->outbuffer[ic_ptr->outbufind];
                    ic_ptr->outbufind++;
                    ic_ptr->status = I2C_MASTER_SEND_START;
                }
                else {
                    ToMainHigh_sendmsg(0, MSGT_I2C_MASTER_SEND_COMPLETE, 0);
                    ic_ptr->outbuflen = 0;
                    ic_ptr->status = I2C_IDLE;
                    SSPCON2bits.PEN = 1;
                }
            }
            else {
                ToMainHigh_sendmsg(0, MSGT_I2C_MASTER_SEND_FAILED, 0);
                ic_ptr->status = I2C_IDLE;
                SSPCON2bits.PEN = 1;
            }
            break;
        }
        case (I2C_MASTER_SEND_DONE): {
            ToMainHigh_sendmsg(0, MSGT_I2C_MASTER_SEND_COMPLETE, 0);
            ic_ptr->status = I2C_IDLE;
            break;
        }

        // Master receive
        
        case I2C_MASTER_RECV_STARTED: {
            //flipDBG(1);
                ic_ptr->status = I2C_MASTER_RECV_SET_RCEN;
                SSPBUF = ((ic_ptr->slave_addr << 1) | 0x01) & 0xFF;
                break;
        }
        case I2C_MASTER_RECV_SET_RCEN: {
            if (SSPCON2bits.ACKSTAT == 0) {
                //flipDBG(1);
                ic_ptr->status = I2C_MASTER_RECV_DATA;
                SSPCON2bits.RCEN = 1;
            }
            else {
                ToMainHigh_sendmsg(0, MSGT_I2C_MASTER_RECV_FAILED, 0);
                ic_ptr->status = I2C_IDLE;
                SSPCON2bits.PEN = 1;
            }
            break;
        }
        case I2C_MASTER_RECV_DATA: {
            //flipDBG(1);
            //    if (SSPSTATbits.BF == 1) {
                    ic_ptr->buffer[ic_ptr->bufind] = SSPBUF;
                    ic_ptr->bufind++;
                    ic_ptr->status = I2C_MASTER_RECV_ACK;
           //     }
                if (ic_ptr->bufind < ic_ptr->buflen) {
                    SSPCON2bits.ACKDT = 0;
                    SSPCON2bits.ACKEN = 1;
                }
                else {
                    SSPCON2bits.ACKDT = 1;
                    SSPCON2bits.ACKEN = 1;
                }
            break;
        }
        case I2C_MASTER_RECV_ACK: {
            if (ic_ptr->bufind < ic_ptr->buflen) {
                ic_ptr->status = I2C_MASTER_RECV_DATA;
                SSPCON2bits.RCEN = 1;
            }
            else {
                ToMainHigh_sendmsg(ic_ptr->buflen, MSGT_I2C_MASTER_RECV_COMPLETE, (void *)(ic_ptr->buffer));
                ic_ptr->status = I2C_IDLE;
                SSPCON2bits.PEN = 1;
            }
            break;
        }
    }
}