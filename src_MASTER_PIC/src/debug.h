/* 
 * File:   debug.h
 * Author: ECE4534
 *
 * Created on September 10, 2014, 5:52 PM
 */

#ifndef DEBUG_H
#define	DEBUG_H

// Define this to turn on instrumentation
#define DO_DEBUG

// Use definitions to make debugging configurable
#define I2C_ISR LATBbits.LATB1

// Define out debugging functions
#ifdef DO_DEBUG
#define DEBUG_ON(a) {\
    a = 1;\
}
#define DEUBG_OFF(a) {\
    a = 0;\
}
#else
#define DEBUG_ON(a)
#define DEBUG_OFF(a)
#endif



#endif	/* DEBUG_H */

