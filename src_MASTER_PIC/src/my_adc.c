#include "maindefs.h"
#include "my_adc.h"
#include "messages.h"

void adc_config() {
    //FOR ADC INIT
    TRISAbits.TRISA0 = 1;
    ANCON0bits.PCFG0 = 0;   // RA0 uses AN0 for ADC     Analog - Digital p.349
    //ADCON0bits.VCFG0 = 1;   // Set Voltage Reference
    ADCON0bits.VCFG0 = 0;   // Set Voltage Reference
    ADCON0bits.CHS0 = 0; // Sets it to AN0, Maybe we need to do .CHS instead
    ADCON1bits.ADFM = 1;    // Might need
    ADCON1bits.ADCAL = 0;   // Might need
    ADCON1bits.ACQT = 0x01;  // 1 TAD = 1.3us
    ADCON1bits.ADCS = 0x06;   //  110;  // PIC clock operates at 48MHz
    ADCON0bits.ADON = 1;    // Turn on AD Module
}

void adc_int_handler(){
    unsigned short adcResult = 0;
    adcResult = ADRESH << 8 | ADRESL;
    adcResult = 2.0*(9462.0 / (adcResult - 16.92));
    ToMainHigh_sendmsg(2, MSGT_ADC_DATA, &adcResult);
}