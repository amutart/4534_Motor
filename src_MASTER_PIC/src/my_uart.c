#include "maindefs.h"
#ifndef __XC8
#include <usart.h>
#else
#include <plib/usart.h>
#endif
#include "my_uart.h"

static uart_comm *uc_ptr;
static int UART_TX_PTR, UART_DATA_LEN;
static unsigned char msgbuffer[MSGLEN + 1];

void uart_recv_int_handler() {
#ifdef __USE18F26J50
    if (DataRdy1USART()) {
        uc_ptr->buffer[uc_ptr->buflen] = Read1USART();
#else
#ifdef __USE18F46J50
    if (DataRdy1USART()) {
        uc_ptr->buffer[uc_ptr->buflen] = Read1USART();
#else
    if (DataRdyUSART()) {
        uc_ptr->buffer[uc_ptr->buflen] = ReadUSART();
#endif

#endif

        uc_ptr->buflen++;
        // check if a message should be sent
        if (uc_ptr->buflen == MAXUARTBUF) {
            ToMainLow_sendmsg(uc_ptr->buflen, MSGT_UART_DATA, (void *) uc_ptr->buffer);
            uc_ptr->buflen = 0;
        }
    }
#ifdef __USE18F26J50
    if (USART1_Status.OVERRUN_ERROR == 1) {
#else
#ifdef __USE18F46J50
    if (USART1_Status.OVERRUN_ERROR == 1) {
#else
    if (USART_Status.OVERRUN_ERROR == 1) {
#endif
#endif
        // we've overrun the USART and must reset
        // send an error message for this
        RCSTAbits.CREN = 0;
        RCSTAbits.CREN = 1;
        ToMainLow_sendmsg(0, MSGT_OVERRUN, (void *) 0);
    }
}

void init_uart_recv(uart_comm *uc) {
    uc_ptr = uc;
    uc_ptr->buflen = 0;
}

// UART Configuration
void uart_config() {
    RCSTA1bits.SPEN = 0x1;      // enable serial bit

    TRISCbits.TRISC7 = 0x1;     // sets Rx input
    TRISCbits.TRISC6 = 0x0;     // sets Tx output

    TXSTA1bits.BRGH = 0x1;      // use high baud
    BAUDCONbits.BRG16 = 0x1;    // use 16bit baudrate generator

#ifdef __USE18F45J10
    SPBRGH = 0x00;
    SPBRG = 0xCF;
#else
    // Sets up baud rate generator
    SPBRGH1 = 0x00;
    SPBRG1 = 0xCF;
#endif

    TXSTA1bits.TXEN = 0x1;      // Transmition Enable
    TXSTA1bits.SYNC = 0x0;      // Asyncmode
    RCSTA1bits.CREN = 0x1;
}

// Send uart msg from main
void send_uart_msg(int length, unsigned char *msg) {
    signed char ierr = FromMainLow_sendmsg(length, MSGT_UART_DATA, (void *) msg);

    //msgbuffer = msg;
    UART_DATA_LEN = length;
    UART_TX_PTR = 0;

    // turn on the TxIH
    PIE1bits.TX1IE = 1;
}

// UART interrupt handler for transmition
void uart_tran_int_handler() {
    if (PIR1bits.TX1IF && PIE1bits.TX1IE) {

        signed char ierr = FromMainLow_recvmsg(MSGLEN, MSGT_UART_DATA, (void *)msgbuffer );

        TXREG1 = msgbuffer[UART_TX_PTR];
        UART_TX_PTR++;

        if(UART_TX_PTR == UART_DATA_LEN) {
            PIE1bits.TX1IE = 0;
        }
    }
}
