/*
  -------------------------------------------
  This file contains configuration data specific to the A37730-000
  
  Hart Wanetick
  10.1.2018

  --------------------------------------------
 */

#ifndef __A37730_H
#define __A37730_H

#include <xc.h>
#include <libpic30.h>
#include <adc12.h>
#include <timer.h>
#include <spi.h>
#include <uart.h>
#include "ETM.h"
#include "P1395_CAN_SLAVE.h"
#include "FIRMWARE_VERSION.h"
#include "TCPmodbus\TCPmodbus.h"
#include "ETM_TICK.h"

//#include "faults.h"

#define FCY_CLK                    10000000

#define __noModbusLibrary
// --------- Resource Summary  -----------------
/*
  Hardware Module Resource Usage

  CAN2   - Not used
  CAN1   - Used/Configured by ETM CAN (optical CAN)
  Timer4 - Used/Configured by ETM CAN - Used to Time sending of messages (status update / logging data and such) 
  Timer5 - Used/Configured by ETM CAN - Used for detecting error on can bus

  SPI1   - Used for communicating with Ethernet module
  SPI2   - Used for communicating with on board DAC

  Timer2 - ? 

  ADC Module - AN3,AN4,AN5,AN6,AN7,VREF+,VREF-,AN8,AN9,AN10

  I2C    - Not used
  
  EEPROM - The internal EEPROM is used at this time
  
 */





// ----------------- IO PIN CONFIGURATION -------------------- //
// All unused pins will be set to outputs and logic zero
// LAT values default to 0 at startup so they do not need to be manually set


// Pins to be configured as inputs
/*
  RA6  - ILOCK RELAY CLOSED
  RA7  - ENABLE RELAY CLOSED
  RA9  - ADC VREF-
  RA10 - ADC VREF+
  RA13 - ARC SIGNAL
  RA14 - ETHERNET INTERRUPT  (Configured By Pic Module)
  RA15 - PULSE GATE INTERRUPT
 
  RB0  - ICD - PROGRAM
  RB1  - ICD - PROGRAM
  RB3  - AN3 - GUN I PK ADC
  RB4  - AN4 - HV I ADC
  RB5  - AN5 - HV V ADC
  RB6  - AN6 - HTR V ADC
  RB7  - AN7 - HTR I ADC
  RB8  - AN8 - TOP V ADC
  RB9  - AN9 - BIAS V ADC
  RB10 - AN10 - THERM ADC
  RB11 - AN11 - 5V MON ADC  CURRENTLY REMOVED (OPTIONAL)
 
  RC1  - GH_BIAS_FAULT
  RC2  - GH_HARDWARE_FAULT
  RC3  - GH_TOP_HTR_OV_FAULT
  RC4  - GH_TOP_HTR_UV_FAULT
  RC13 - CUSTOMER BEAM ENABLE
  RC14 - CUSTOMER HV ON

  RD0  - ETHERNET CS  (Configured By Pic Module)
  RD8  - PULSE GATE INPUT CAPTURE
  RD11 - ETHERNET CLK OUT  (Configured By Pic Module)
  RD15 - CATHODE CURRENT PULSE INPUT CAPTURE


  RF0  - CAN 1 (Configured By Pic Module)
  RF1  - CAN 1 (Configured By Pic Module)
  RF2  - UART 1 (Configured By Pic Module)
  RF3  - UART 1 (Configured By Pic Module)
  RF6  - SPI 1 (Configured By Pic Module)
  RF7  - SPI 1 (Configured By Pic Module)
  RF8  - SPI 1 (Configured By Pic Module)


  RG0  - 15V SUPPLY ENABLE
  RG1  - 12V SUPPLY ENABLE
  RG2  - DAC LDAC (Configured By Pic Module)
  RG3  - DAC CS/LD (Configured By Pic Module)
  RG6  - SPI2 (Configured By Pic Module)
  RG7  - SPI2 (Configured By Pic Module)
  RG8  - SPI2 (Configured By Pic Module)
  RG15 - PIC_TEMP_LT_75
  
  Configured by other software modules and should be left as inputs during port configuration:
   ETHERNET
   SPI 1
   SPI 2
   UART 1 (RS-485)
   CAN 1
   DAC


  Pins to be configured as outputs
  ------------------------------
  A12 - PW LIMIT
 
  B2  - CPU BEAM ENABLE
  B12 - INTEG1
  B13 - INTEG2
  B14 - CPU HTR ENABLE
  B15 - CPU TOP ENABLE
 
  C15 - CLOCK OUT
 
  D1  - HV ON SELECT
  D2  - BEAM ENABLE SELECT
  D3  - PULSE INPUT SELECT
  D4  - LED GREEN 1
  D5  - LED GREEN 2
  D6  - LED RED
  D7  - TEST POINT 1
  D12 - HV ON SERIAL
  D13 - BEAM ENABLE SERIAL
 
  F4  - RS_485 ENABLE
  F5  - TEST POINT 2
 
  G14 - CPU HV ENABLE
  
 */
/*
   F E D C B A 9 8 7 6 5 4 3 2 1 0

A  1 1 1 0 1 1 1 1 1 1 1 1 1 1 1 1

B  0 0 0 0 1 1 1 1 1 1 1 1 1 0 1 1 

C  0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 

D  1 1 0 0 1 1 1 1 0 0 0 0 0 0 0 1 

F  1 1 1 1 1 1 1 1 1 1 0 0 1 1 1 1 

G  1 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 

 */


#define A37730_TRISA_VALUE 0b1110111111111111 
#define A37730_TRISB_VALUE 0b0000111111111011 
#define A37730_TRISC_VALUE 0b0111111111111111 
#define A37730_TRISD_VALUE 0b1100111100000001 
#define A37730_TRISF_VALUE 0b1111111111001111 
#define A37730_TRISG_VALUE 0b1011111111111111 



/*
  This sets up the ADC to work as following
  AUTO Sampling
  External Vref+/Vref-
  With 10MHz System Clock, ADC Clock is 450ns, Sample Time is 6 ADC Clock so total sample time is 9.0uS
  8 Samples per Interrupt, use alternating buffers
  Conversion rate of 111KHz (13.875 Khz per Channel), 138 Samples per 10mS interrupt
  Scan Through Selected Inputs (8 selected at any point in time)

 */

#define ADCON1_SETTING          (ADC_MODULE_OFF & ADC_IDLE_STOP & ADC_FORMAT_INTG & ADC_CLK_AUTO & ADC_AUTO_SAMPLING_ON)
#define ADCON2_SETTING          (ADC_VREF_EXT_EXT & ADC_SCAN_ON & ADC_SAMPLES_PER_INT_8 & ADC_ALT_BUF_ON & ADC_ALT_INPUT_OFF)
#define ADCON3_SETTING          (ADC_SAMPLE_TIME_4 & ADC_CONV_CLK_SYSTEM & ADC_CONV_CLK_9Tcy2)
#define ADCHS_SETTING           (ADC_CH0_POS_SAMPLEA_AN3 & ADC_CH0_NEG_SAMPLEA_VREFN & ADC_CH0_POS_SAMPLEB_AN3 & ADC_CH0_NEG_SAMPLEB_VREFN)
#define ADPCFG_SETTING          (ENABLE_AN3_ANA & ENABLE_AN4_ANA & ENABLE_AN5_ANA & ENABLE_AN6_ANA & ENABLE_AN7_ANA & ENABLE_AN8_ANA & ENABLE_AN9_ANA & ENABLE_AN10_ANA)
#define ADCSSL_SETTING          (SKIP_SCAN_AN0 & SKIP_SCAN_AN1 & SKIP_SCAN_AN2 & SKIP_SCAN_AN11 & SKIP_SCAN_AN12 & SKIP_SCAN_AN13 & SKIP_SCAN_AN14 & SKIP_SCAN_AN15)




// Digital Inputs
#define PIN_CUSTOMER_HV_ON                           _RC14
#define ILL_PIN_CUSTOMER_HV_ON_ENABLE_HV             1

#define PIN_CUSTOMER_BEAM_ENABLE                     _RC13
#define ILL_PIN_CUSTOMER_BEAM_ENABLE_BEAM_ENABLED    1

#define PIN_ARC_INTERRUPT                            _RA13
#define PIN_ETHERNET_INTERRUPT                       _RA14
#define PIN_TRIG_PULSE_INTERRUPT                     _RA15
#define PIN_TRIG_PULSE_INPUT_CAPTURE                 _RD8
#define PIN_CATH_I_PULSE_INPUT_CAPTURE               _RD15
#define PIN_ETHERNET_CLOCK_OUT                       _RD11

#define PIN_TEMP_LT_75C                              _RG15
#define ILL_PIN_TEMP_IS_LT_75C                       1               

#define PIN_INTERLOCK_RELAY_STATUS                   _RA6
#define PIN_ENABLE_RELAY_STATUS                      _RA7
#define ILL_PIN_RELAY_STATUS_CLOSED                  1

#define PIN_GH_BIAS_FAULT                            _RC1 
#define PIN_GH_HARDWARE_FAULT                        _RC2
#define PIN_GH_TOP_HTR_OV_FAULT                      _RC3
#define PIN_GH_TOP_HTR_UV_FAULT                      _RC4
#define ILL_GH_FAULT_ACTIVE                          0

//------------------- GUN Driver Interface I/O ------------------------- //
//#define PIN_CS_DAC                                   _LATD13
//#define OLL_PIN_CS_DAC_SELECTED                      1


// Digital Outputs

#define PIN_HV_ON_SERIAL                             _LATD12
#define PIN_BEAM_ENABLE_SERIAL                       _LATD13
#define OLL_SERIAL_ENABLE                            1

#define PIN_15V_SUPPLY_ENABLE                        _LATG0  //set as input for high impedance to enable
#define PIN_12V_SUPPLY_ENABLE                        _LATG1  //set as input for high impedance to enable

#define PIN_TRIG_PULSE_WIDTH_LIMITER                 _LATA12
#define OLL_TRIG_PULSE_DISABLE                       0

#define PIN_HV_ON_SELECT                             _LATD1
#define PIN_BEAM_ENABLE_SELECT                       _LATD2
#define OLL_SELECT_SERIAL_CONTROL                    1

#define PIN_TRIGGER_INPUT_SELECT                     _LATD3
#define OLL_SELECT_OPTICAL_TRIGGER                   1

#define PIN_CPU_HTR_ENABLE                           _LATB14
#define PIN_CPU_TOP_ENABLE                           _LATB15
#define PIN_CPU_HV_ENABLE                            _LATG14
#define PIN_CPU_BEAM_ENABLE                          _LATB2
#define OLL_CPU_ENABLE                               1


#define PIN_RS485_ENABLE                             _LATF4

#define PIN_INTEGRATOR_1                             _LATB12
#define PIN_INTEGRATOR_2                             _LATB13

#define OLL_LED_ON                                   0

#define PIN_LED_GREEN_1                              _LATD4
#define PIN_LED_GREEN_2                              _LATD5
#define PIN_LED_RED                                  _LATD6


#define PIN_TEST_POINT_1                             _LATD7
#define PIN_TEST_POINT_2                             _LATF5

#define PIN_LED_I2C_OPERATION                        PIN_LED_GREEN_1  // This pin is controlled by the CAN Module
#define PIN_LED_OPERATIONAL                          PIN_LED_GREEN_2

#define PIN_LED_FAULT_STATE                          PIN_LED_RED


// -----------------------  END IO PIN CONFIGURATION ------------------------ //



// -------------------------------------------- INTERNAL MODULE CONFIGURATION --------------------------------------------------//

/*
  --- SPI1 Port --- 
  This SPI port is used to connect with the gun driver
  This must be slower to compensate for the 2x delay across the optocoupler 200ns with filtering in one direction, 80ns (without filtering) in the other direction
  Minimum clock period is therefore 280ns + holdtime + margins
 */
#define A37730_SPI1CON_VALUE  (FRAME_ENABLE_OFF & ENABLE_SDO_PIN & SPI_MODE16_OFF & SPI_SMP_OFF & SPI_CKE_OFF & SLAVE_ENABLE_OFF & CLK_POL_ACTIVE_HIGH & MASTER_ENABLE_ON)
#define A37730_SPI1STAT_VALUE (SPI_ENABLE & SPI_IDLE_CON & SPI_RX_OVFLOW_CLR)   


///*
//  --- Timer2 Setup ---
//  Period of 10mS
//*/
//#define A37730_T2CON_VALUE     (T2_ON & T2_IDLE_CON & T2_GATE_OFF & T2_PS_1_8 & T2_32BIT_MODE_OFF & T2_SOURCE_INT)
//#define A37730_PR2_VALUE_US    10000   // 10mS
//#define A37730_PR2_VALUE       ((FCY_CLK/1000000)*A37730_PR2_VALUE_US/8)
///*
//  --- Timer3 Setup ---
//  Period of 1S
//*/
//#define A37730_T3CON_VALUE     (T3_ON & T3_IDLE_CON & T3_GATE_OFF & T3_PS_1_256 & T3_SOURCE_INT)
//#define A37730_PR3_VALUE_US    1000000   // 1s
//#define A37730_PR3_VALUE       ((FCY_CLK/1000000)*A37730_PR3_VALUE_US/256)

/*
  --- Timer3 Setup ---
  Period of 400mS
 */
#define A37730_T3CON_VALUE     (T3_ON & T3_IDLE_CON & T3_GATE_OFF & T3_PS_1_64 & T3_SOURCE_INT)
#define A37730_PR3_VALUE        62500  // 400mS


// ---- Hard Coded Delays ---- //
#define DELAY_FPGA_CABLE_DELAY 10

// ---------------------- Converter Logic Board Interface Control ------------------------- //
//#define WATCHDOG_HIGH                                48000
//#define WATCHDOG_LOW                                 16000

#define WATCHDOG_MODE_0         10
#define WATCHDOG_MODE_1         20
#define WATCHDOG_VALUE_0        0x3201   // 12801 - Allowed range from 8706 to 16896   //0x1000
#define WATCHDOG_VALUE_1        0xCE3E   // 52798 - Allowed range from 48703 to 56893  //0xEFF0

#define WATCHDOG_MAX_COUNT      80                           //800ms

#define MIN_WD_VALUE_0          0x2202
#define MAX_WD_VALUE_0          0x4200
#define MIN_WD_VALUE_1          0xBE3F  //0xBFF0
#define MAX_WD_VALUE_1          0xDE3D

#define DAC_DIGITAL_OFF                              0x0000
#define DAC_DIGITAL_ON                               0xFFFF

#define ADC_DATA_DIGITAL_HIGH                        0x0800

#define TARGET_CUSTOMER_HARDWARE_REV                 0b000100
#define INTERFACE_HARDWARE_REV                       0b01000001    // 0x41 'A'

//// MAX1230 Control Words
//#define MAX1230_CONVERSION_BYTE                      0b10000011
////#define MAX1230_SETUP_BYTE                           0b01101000    //with internal ref
//#define MAX1230_SETUP_BYTE                           0b01100100    //with external ref
//#define MAX1230_AVERAGE_BYTE                         0b00111000
//#define MAX1230_RESET_BYTE                           0b00010000


#define LTC265X_CMD_SELECT_INTERNAL_REFERENCE_BYTE    (LTC265X_CMD_SELECT_INTERNAL_REFERENCE | LTC265X_ADR_ALL_DACS) 

typedef struct {
    //unsigned int watchdog_count_error;          // 
    unsigned int control_state; // This stores the state of the state machine
    unsigned int request_heater_enable; // This indicates that heater_enable has been requested (either from CAN module or from discrete inputs depending upon configuration)
    unsigned int request_hv_enable; // This indicates that hv_enable has been requested (either from CAN module or from discrete inputs depending upon configuration)
    unsigned int request_beam_enable; // This indicates that beam_enable has been requested (either from CAN module or from discrete inputs depending upon configuration)
    unsigned int reset_active; // This indicates that reset has been requested (either from CAN module or from discrete inputs depending upon configuration)
    unsigned int ethernet_reset_cmd;

    unsigned int heater_start_up_attempts; // This counts the number of times the heater has started up without successfully completing it's ramp up.

    unsigned int run_time_counter; // This counts how long the unit has been running for.  It wraps every 11 minutes
    unsigned int fault_restart_remaining; // This counts down the delay of the heater automatic restart
    unsigned int power_supply_startup_remaining; // This counts down the ramp up time of the HV supply
    unsigned int heater_warm_up_time_remaining; // This counts down the heater warm up
    unsigned int heater_ramp_up_time; // This counts the time it takes the heater to ramp up
    unsigned int watchdog_counter; // This counts when to updated the watchdog DAC output on the converter logic board
    unsigned int watchdog_state_change; // This flag is so the DAC isn't rewritten to for at least 80 ms
    unsigned int watchdog_set_mode; // This is the DAC/ADC test setting for the SPI watchdog
    unsigned int heater_ramp_interval; // This counts the interval between heater ramp voltage changes
    unsigned int heater_voltage_target; // This is the targeted heater voltage set point
    unsigned int fault_holdoff_state; // This is whether to hold off current limit fault during htr warmup period
    unsigned int fault_holdoff_count; // This is a counter for the current limit fault holdoff
    unsigned int mux_fault;

    unsigned int previous_0x0A_val;
    unsigned int previous_0x0B_val;
    unsigned int previous_0x0C_val;
    unsigned int previous_0x0D_val;


    volatile unsigned char control_config; // This indicates when all set values from the CAN interface have been received

    unsigned int state_message; // This is a state message for the modbus module
    unsigned int current_state_msg; // This stores the preliminary state message

    unsigned int can_high_voltage_set_point; // This is the high voltage set point set over the can interface (it is only used if can mode is selected)
    unsigned int can_pulse_top_set_point; // This is the pulse top set point set over the can interface (it is only used if can mode is selected)
    unsigned int can_heater_voltage_set_point; // This is the heater voltage set point set over the can interface (it is only used if can mode is selected)

    unsigned int ethernet_htr_ref;
    unsigned int ethernet_top_ref;
    unsigned int ethernet_hv_ref;

    unsigned int discrete_commands_always;
    unsigned int analog_references_always;
    unsigned int modbus_controls_enabled;
    unsigned int modbus_references_enabled;
    unsigned int ethernet_references_enabled;

    unsigned int last_period; // This is for PRF calculation
    unsigned int trigger_complete; // This is for trigger
    unsigned int period_filtered;
    unsigned int limiting_active;
    unsigned int rep_rate_deci_hertz;
    unsigned int over_prf;
    unsigned int pulses_on;

    unsigned int testDAC;

    unsigned long tick_timer;
    unsigned long tick_period_timer;

    unsigned int accumulator_counter; // This counts the number of converstion on the internal ADC (used for averaging)

    unsigned int dac_write_error_count; // This counts the total number of dac write errors
    unsigned int dac_write_failure_count; // This counts the total number of unsessful dac transmissions (After N write errors it gives us)
    unsigned int dac_write_failure; // This indicates that the previous attempt to write to the dac failed

    unsigned int heater_voltage_current_limited; // This counter is used to track how long the heater is opperating in current limited mode. 
    unsigned int previous_state_pin_customer_hv_on; // This stores the previous state of customer HV on input.  An On -> Off transion of this pin is used to generate a reset in discrete control mode


    // These are the Data Structures for the DAC outputs on the converter logic board
    AnalogOutput analog_output_high_voltage;
    AnalogOutput analog_output_top_voltage;
    AnalogOutput analog_output_heater_voltage;
    unsigned int dac_digital_hv_enable;
    unsigned int dac_digital_heater_enable;
    unsigned int dac_digital_top_enable;
    unsigned int dac_digital_trigger_enable;
    unsigned int dac_digital_watchdog_oscillator; //

    // These are the Data Structures for the on board DAC outputs
    AnalogOutput monitor_heater_voltage;
    AnalogOutput monitor_heater_current;
    AnalogOutput monitor_cathode_voltage;
    AnalogOutput monitor_grid_voltage;



    // These are the Data Structures for the Digital Data from the FPGA on the Converter Logic board
    //  TYPE_DIGITAL_INPUT fpga_coverter_logic_pcb_rev_mismatch;
    //  TYPE_DIGITAL_INPUT fpga_firmware_major_rev_mismatch;
    //  TYPE_DIGITAL_INPUT fpga_firmware_minor_rev_mismatch;
    //  TYPE_DIGITAL_INPUT fpga_arc;
    //  TYPE_DIGITAL_INPUT fpga_arc_high_voltage_inihibit_active;
    //  TYPE_DIGITAL_INPUT fpga_heater_voltage_less_than_4_5_volts;
    //  TYPE_DIGITAL_INPUT fpga_module_temp_greater_than_65_C;
    //  TYPE_DIGITAL_INPUT fpga_module_temp_greater_than_75_C;
    //  TYPE_DIGITAL_INPUT fpga_pulse_width_limiting_active;
    //  TYPE_DIGITAL_INPUT fpga_prf_fault;
    //  TYPE_DIGITAL_INPUT fpga_current_monitor_pulse_width_fault;
    //  TYPE_DIGITAL_INPUT fpga_grid_module_hardware_fault;
    //  TYPE_DIGITAL_INPUT fpga_grid_module_over_voltage_fault;
    //  TYPE_DIGITAL_INPUT fpga_grid_module_under_voltage_fault;
    //  TYPE_DIGITAL_INPUT fpga_grid_module_bias_voltage_fault;
    //  TYPE_DIGITAL_INPUT fpga_hv_regulation_warning;
    //  TYPE_DIGITAL_INPUT fpga_dipswitch_1_on;
    //  TYPE_DIGITAL_INPUT fpga_test_mode_toggle_switch_set_to_test;
    //  TYPE_DIGITAL_INPUT fpga_local_mode_toggle_switch_set_to_local;


    // These are Data Structures for the inputs to the PIC's internal ADC

    AnalogInput input_hv_v_mon;
    AnalogInput input_hv_i_mon;
    AnalogInput input_gun_i_peak;
    AnalogInput input_htr_v_mon;
    AnalogInput input_htr_i_mon;
    AnalogInput input_top_v_mon;
    AnalogInput input_bias_v_mon;
    AnalogInput input_24_v_mon;
    AnalogInput input_temperature_mon;
    //  TYPE_DIGITAL_INPUT adc_digital_warmup_flt;
    //  TYPE_DIGITAL_INPUT adc_digital_watchdog_flt;
    //  TYPE_DIGITAL_INPUT adc_digital_arc_flt;
    //  TYPE_DIGITAL_INPUT adc_digital_over_temp_flt;
    //  TYPE_DIGITAL_INPUT adc_digital_pulse_width_duty_flt;
    //  TYPE_DIGITAL_INPUT adc_digital_grid_flt;
    AnalogInput input_dac_monitor;

    TYPE_DIGITAL_INPUT interlock_relay_closed;
    TYPE_DIGITAL_INPUT enable_relay_closed;
    TYPE_DIGITAL_INPUT gh_digital_bias_flt;
    TYPE_DIGITAL_INPUT gh_digital_hw_flt;
    TYPE_DIGITAL_INPUT gh_digital_top_htr_ov_flt;
    TYPE_DIGITAL_INPUT gh_digital_top_htr_uv_flt;
    TYPE_DIGITAL_INPUT digital_temp_lt_75;


    unsigned int previous_state;
    unsigned int arc_counter;
    unsigned int PRF;
	unsigned int Trigger_Period;
	
	double Trigger_Period_Double;
	double Scaled_Trigger_Period_Double;

    //  AnalogInput  pos_5v_mon;  // an13
    //  AnalogInput  pos_15v_mon; // an14
    //  AnalogInput  neg_15v_mon; // an15

} TYPE_GLOBAL_DATA_A37730;

extern TYPE_GLOBAL_DATA_A37730 global_data_A37730;





// ---------------------- FAULT & STATUS   CONFIGURATION ---------------------------- //




#define _FAULT_ADC_HTR_V_MON_UNDER_RELATIVE            _FAULT_0 // CHECKED_DP// Heater Fault
#define _FAULT_ADC_HV_V_MON_OVER_RELATIVE              _FAULT_1 // CHECKED_DP
#define _FAULT_ADC_HV_V_MON_UNDER_RELATIVE             _FAULT_1 // CHECKED_DP
#define _FAULT_ADC_HTR_V_MON_OVER_RELATIVE             _FAULT_2 // CHECKED_DP// Heater Fault
//#define _FAULT_ADC_HTR_V_MON_UNDER_RELATIVE            _FAULT_2 // CHECKED_DP// Heater Fault
#define _FAULT_ADC_HTR_I_MON_OVER_ABSOLUTE             _FAULT_3 // CHECKED_DP// Heater Fault
#define _FAULT_ADC_HTR_I_MON_UNDER_ABSOLUTE            _FAULT_4 // CHECKED_DP// Heater Fault
#define _FAULT_ADC_TOP_V_MON_OVER_RELATIVE             _FAULT_5 // CHECKED_DP
#define _FAULT_ADC_TOP_V_MON_UNDER_RELATIVE            _FAULT_5 // CHECKED_DP
#define _FAULT_ADC_BIAS_V_MON_OVER_ABSOLUTE            _FAULT_6 // CHECKED_DP 
#define _FAULT_ADC_BIAS_V_MON_UNDER_ABSOLUTE           _FAULT_6 // CHECKED_DP
#define _FAULT_UNUSED_1                                _FAULT_7
#define _FAULT_ADC_DIGITAL_ARC                         _FAULT_8
#define _FAULT_DIGITAL_OVER_TEMP                       _FAULT_9
#define _FAULT_ADC_DIGITAL_GRID                        _FAULT_A
#define _FAULT_GRID_HEATER_HARDWARE                    _FAULT_A
#define _FAULT_DIGITAL_TOP_HEATER_OV                   _FAULT_A
#define _FAULT_DIGITAL_TOP_HEATER_UV                   _FAULT_A
#define _FAULT_DIGITAL_BIAS                            _FAULT_A
#define _FPGA_CURRENT_MONITOR_PULSE_WIDTH_FAULT        _FAULT_B
#define _FAULT_OVER_PRF                                _FAULT_C
#define _FAULT_HEATER_VOLTAGE_CURRENT_LIMITED          _FAULT_D
#define _FAULT_HEATER_RAMP_TIMEOUT                     _FAULT_E
#define _FAULT_MUX_CONFIG_FAILURE                      _FAULT_F




#define _STATUS_CUSTOMER_HV_ON                         _WARNING_0
#define _STATUS_CUSTOMER_BEAM_ENABLE                   _WARNING_1
#define _STATUS_ADC_DIGITAL_HEATER_NOT_READY           _WARNING_2
#define _STATUS_DAC_WRITE_FAILURE                      _WARNING_3
#define _STATUS_INTERLOCK_INHIBITING_HV                _WARNING_4
//#define _FPGA_CUSTOMER_HARDWARE_REV_MISMATCH           _WARNING_6
#define _STATUS_ENABLE_RELAY_CLOSED                    _WARNING_5
//#define _FAULT_CONVERTER_LOGIC_ADC_READ_FAILURE        _WARNING_5
#define _FPGA_ARC_COUNTER_GREATER_ZERO                 _WARNING_6
#define _FPGA_ARC_HIGH_VOLTAGE_INHIBIT_ACTIVE          _WARNING_6
//#define _FPGA_MODULE_TEMP_GREATER_THAN_65_C            _WARNING_8
#define _FPGA_MODULE_TEMP_GREATER_THAN_75_C            _WARNING_7
#define _FAULT_ADC_DIGITAL_PULSE_WIDTH_DUTY            _WARNING_8
#define _FPGA_PULSE_WIDTH_LIMITING                     _WARNING_8
#define _FPGA_GRID_MODULE_HARDWARE_FAULT               _WARNING_9
#define _FPGA_GRID_MODULE_OVER_VOLTAGE_FAULT           _WARNING_A
#define _FPGA_GRID_MODULE_UNDER_VOLTAGE_FAULT          _WARNING_A
#define _FPGA_GRID_MODULE_BIAS_VOLTAGE_FAULT           _WARNING_B
#define _FPGA_HV_REGULATION_WARNING                    _WARNING_C




#define STATE_FAULT_HEATER_FAILURE           00
#define STATE_FAULT_WARMUP_HEATER_OFF        10
#define STATE_FAULT_HEATER_OFF               20
#define STATE_START_UP                       30
#define STATE_WAIT_FOR_CONFIG                40
#define STATE_RESET_FAULTS                   50
#define STATE_HEATER_DISABLED                60
#define STATE_HEATER_RAMP_UP                 70
#define STATE_HEATER_WARM_UP                 80
#define STATE_FAULT_HEATER_ON                90
#define STATE_HV_OFF_1SEC_DELAY              100
#define STATE_HEATER_WARM_UP_DONE            110
#define STATE_POWER_SUPPLY_RAMP_UP           120
#define STATE_HV_ON                          130
#define STATE_TOP_ON                         140
#define STATE_TOP_READY                      150
#define STATE_BEAM_ENABLE                    160


#define STATE_MESSAGE_FAULT_HEATER_OFF         0x0101
#define STATE_MESSAGE_START_UP                 0x0001
#define STATE_MESSAGE_HEATER_RAMP_UP           0x0003
#define STATE_MESSAGE_HEATER_WARM_UP           0x0007
#define STATE_MESSAGE_FAULT_HEATER_ON          0x010B
#define STATE_MESSAGE_HEATER_WARM_UP_DONE      0x000B
#define STATE_MESSAGE_HV_ON                    0x002B
#define STATE_MESSAGE_BEAM_ENABLE              0x00AB



#ifdef __noModbusLibrary


#define UART1_BAUDRATE             19200        // U1 Baud Rate

#define MODBUS_U1MODE_VALUE        (UART_EN & UART_IDLE_STOP & UART_DIS_WAKE & UART_DIS_LOOPBACK & UART_DIS_ABAUD & UART_NO_PAR_8BIT & UART_2STOPBITS)
#define MODBUS_U1STA_VALUE         (UART_INT_TX & UART_TX_PIN_NORMAL & UART_TX_ENABLE & UART_INT_RX_CHAR & UART_ADR_DETECT_DIS)
#define MODBUS_U1BRG_VALUE         (((FCY_CLK/UART1_BAUDRATE)/16)-1)

#define UART1TX_ON_TRIS		(TRISDbits.TRISD7)
#define UART1TX_ON_IO		(PORTDbits.RD7)

/*
  --- Timer1 Setup ---
  Period of 200ms
 */
#define A37730_T1CON_VALUE     (T1_ON & T1_IDLE_CON & T1_GATE_OFF & T1_PS_1_256 & T1_SOURCE_INT)
#define A37730_PR1_VALUE_US    200000   // 200ms
#define A37730_PR1_VALUE       ((FCY_CLK/1000000)*A37730_PR1_VALUE_US/256)

#define SLAVE_ADDRESS 0x07  //Slave address

#define CRC_POLY 0xA001				// Reverse CR16 polynomial


// Modbus states
#define MODBUS_STATE_IDLE           0x01
#define MODBUS_STATE_RECEIVING      0x02
#define MODBUS_STATE_PROCESSING     0x03
#define MODBUS_STATE_TRANSMITTING   0x04

// PLC slave address
#define MODBUS_SLAVE_ADDR       0x07

// Modbus exception codes
#define ILLEGAL_FUNCTION           0x01
#define ILLEGAL_ADDRESS            0x02
#define ILLEGAL_VALUE              0x03
#define DEVICE_FAILURE             0x04

//Other Errors
#define ETMMODBUS_ERROR_CRC          10
#define ETMMODBUS_ERROR_SLAVE_ADDR   20
#define ETMMODBUS_ERROR_FUNCTION     30

#define ETMMODBUS_COMMAND_OK         40


// Modbus functions
#define FUNCTION_READ_BITS              0x01
#define FUNCTION_READ_REGISTERS         0x03
#define FUNCTION_READ_INPUT_REGISTERS   0x04
#define FUNCTION_WRITE_BIT              0x05
#define FUNCTION_WRITE_REGISTER         0x06

#define EXCEPTION_FLAGGED               0x09

//#define ETMMODBUS_CMD_QUEUE_SIZE   16

typedef struct {
    unsigned char function_code;
    unsigned char received_function_code;
    unsigned char data_length_bytes;
    unsigned char exception_code;
    unsigned char done;
    unsigned int output_address;
    unsigned int data_address;
    unsigned int qty_bits;
    unsigned int qty_reg;
    unsigned int write_value;
    unsigned int data[125];
    unsigned char bit_data[125];
} MODBUS_MESSAGE;

//extern MODBUS_MESSAGE  current_command_ptr;

#define ETMMODBUS_COMMAND_SIZE_MIN    8

#define SLAVE_BIT_ARRAY_SIZE          64
#define SLAVE_HOLD_REG_ARRAY_SIZE     64
#define SLAVE_INPUT_REG_ARRAY_SIZE    64

#define MODBUS_200ms_DELAY           20


#define modbus_slave_hold_reg_0x00  ModbusSlaveHoldingRegister[0]
#define modbus_slave_hold_reg_0x01  ModbusSlaveHoldingRegister[1]
#define modbus_slave_hold_reg_0x02  ModbusSlaveHoldingRegister[2]
#define modbus_slave_hold_reg_0x03  ModbusSlaveHoldingRegister[3]
#define modbus_slave_hold_reg_0x04  ModbusSlaveHoldingRegister[4]
#define modbus_slave_hold_reg_0x05  ModbusSlaveHoldingRegister[5]
#define modbus_slave_hold_reg_0x06  ModbusSlaveHoldingRegister[6]
#define modbus_slave_hold_reg_0x07  ModbusSlaveHoldingRegister[7]
#define modbus_slave_hold_reg_0x08  ModbusSlaveHoldingRegister[8]
#define modbus_slave_hold_reg_0x09  ModbusSlaveHoldingRegister[9]
#define modbus_slave_hold_reg_0x0A  ModbusSlaveHoldingRegister[10]
#define modbus_slave_hold_reg_0x0B  ModbusSlaveHoldingRegister[11]
#define modbus_slave_hold_reg_0x0C  ModbusSlaveHoldingRegister[12]
#define modbus_slave_hold_reg_0x0D  ModbusSlaveHoldingRegister[13]
#define modbus_slave_hold_reg_0x0E  ModbusSlaveHoldingRegister[14]
#define modbus_slave_hold_reg_0x0F  ModbusSlaveHoldingRegister[15]
#define modbus_slave_hold_reg_0x10  ModbusSlaveHoldingRegister[16]
#define modbus_slave_hold_reg_0x11  ModbusSlaveHoldingRegister[17]
#define modbus_slave_hold_reg_0x12  ModbusSlaveHoldingRegister[18]
#define modbus_slave_hold_reg_0x13  ModbusSlaveHoldingRegister[19]
#define modbus_slave_hold_reg_0x14  ModbusSlaveHoldingRegister[20]
#define modbus_slave_hold_reg_0x15  ModbusSlaveHoldingRegister[21]
#define modbus_slave_hold_reg_0x16  ModbusSlaveHoldingRegister[22]
#define modbus_slave_hold_reg_0x17  ModbusSlaveHoldingRegister[23]
#define modbus_slave_hold_reg_0x18  ModbusSlaveHoldingRegister[24]
#define modbus_slave_hold_reg_0x19  ModbusSlaveHoldingRegister[25]
#define modbus_slave_hold_reg_0x1A  ModbusSlaveHoldingRegister[26]
#define modbus_slave_hold_reg_0x1B  ModbusSlaveHoldingRegister[27]
#define modbus_slave_hold_reg_0x1C  ModbusSlaveHoldingRegister[28]
#define modbus_slave_hold_reg_0x1D  ModbusSlaveHoldingRegister[29]
#define modbus_slave_hold_reg_0x1E  ModbusSlaveHoldingRegister[30]
#define modbus_slave_hold_reg_0x1F  ModbusSlaveHoldingRegister[31]
#define modbus_slave_hold_reg_0x20  ModbusSlaveHoldingRegister[32]
#define modbus_slave_hold_reg_0x21  ModbusSlaveHoldingRegister[33]
#define modbus_slave_hold_reg_0x22  ModbusSlaveHoldingRegister[34]
#define modbus_slave_hold_reg_0x23  ModbusSlaveHoldingRegister[35]
#define modbus_slave_hold_reg_0x24  ModbusSlaveHoldingRegister[36]
#define modbus_slave_hold_reg_0x25  ModbusSlaveHoldingRegister[37]
#define modbus_slave_hold_reg_0x26  ModbusSlaveHoldingRegister[38]
#define modbus_slave_hold_reg_0x27  ModbusSlaveHoldingRegister[39]
#define modbus_slave_hold_reg_0x28  ModbusSlaveHoldingRegister[40]
#define modbus_slave_hold_reg_0x29  ModbusSlaveHoldingRegister[41]
#define modbus_slave_hold_reg_0x2A  ModbusSlaveHoldingRegister[42]
#define modbus_slave_hold_reg_0x2B  ModbusSlaveHoldingRegister[43]
#define modbus_slave_hold_reg_0x2C  ModbusSlaveHoldingRegister[44]
#define modbus_slave_hold_reg_0x2D  ModbusSlaveHoldingRegister[45]
#define modbus_slave_hold_reg_0x2E  ModbusSlaveHoldingRegister[46]
#define modbus_slave_hold_reg_0x2F  ModbusSlaveHoldingRegister[47]
#define modbus_slave_hold_reg_0x30  ModbusSlaveHoldingRegister[48]
#define modbus_slave_hold_reg_0x31  ModbusSlaveHoldingRegister[49]
#define modbus_slave_hold_reg_0x32  ModbusSlaveHoldingRegister[50]
#define modbus_slave_hold_reg_0x33  ModbusSlaveHoldingRegister[51]
#define modbus_slave_hold_reg_0x34  ModbusSlaveHoldingRegister[52]
#define modbus_slave_hold_reg_0x35  ModbusSlaveHoldingRegister[53]
#define modbus_slave_hold_reg_0x36  ModbusSlaveHoldingRegister[54]
#define modbus_slave_hold_reg_0x37  ModbusSlaveHoldingRegister[55]
#define modbus_slave_hold_reg_0x38  ModbusSlaveHoldingRegister[56]
#define modbus_slave_hold_reg_0x39  ModbusSlaveHoldingRegister[57]
#define modbus_slave_hold_reg_0x3A  ModbusSlaveHoldingRegister[58]
#define modbus_slave_hold_reg_0x3B  ModbusSlaveHoldingRegister[59]
#define modbus_slave_hold_reg_0x3C  ModbusSlaveHoldingRegister[60]
#define modbus_slave_hold_reg_0x3D  ModbusSlaveHoldingRegister[61]
#define modbus_slave_hold_reg_0x3E  ModbusSlaveHoldingRegister[62]
#define modbus_slave_hold_reg_0x3F  ModbusSlaveHoldingRegister[63]


#define modbus_slave_input_reg_0x00  ModbusSlaveInputRegister[0]
#define modbus_slave_input_reg_0x01  ModbusSlaveInputRegister[1]
#define modbus_slave_input_reg_0x02  ModbusSlaveInputRegister[2]
#define modbus_slave_input_reg_0x03  ModbusSlaveInputRegister[3]
#define modbus_slave_input_reg_0x04  ModbusSlaveInputRegister[4]
#define modbus_slave_input_reg_0x05  ModbusSlaveInputRegister[5]
#define modbus_slave_input_reg_0x06  ModbusSlaveInputRegister[6]
#define modbus_slave_input_reg_0x07  ModbusSlaveInputRegister[7]
#define modbus_slave_input_reg_0x08  ModbusSlaveInputRegister[8]
#define modbus_slave_input_reg_0x09  ModbusSlaveInputRegister[9]
#define modbus_slave_input_reg_0x0A  ModbusSlaveInputRegister[10]
#define modbus_slave_input_reg_0x0B  ModbusSlaveInputRegister[11]
#define modbus_slave_input_reg_0x0C  ModbusSlaveInputRegister[12]
#define modbus_slave_input_reg_0x0D  ModbusSlaveInputRegister[13]
#define modbus_slave_input_reg_0x0E  ModbusSlaveInputRegister[14]
#define modbus_slave_input_reg_0x0F  ModbusSlaveInputRegister[15]
#define modbus_slave_input_reg_0x10  ModbusSlaveInputRegister[16]
#define modbus_slave_input_reg_0x11  ModbusSlaveInputRegister[17]
#define modbus_slave_input_reg_0x12  ModbusSlaveInputRegister[18]
#define modbus_slave_input_reg_0x13  ModbusSlaveInputRegister[19]
#define modbus_slave_input_reg_0x14  ModbusSlaveInputRegister[20]
#define modbus_slave_input_reg_0x15  ModbusSlaveInputRegister[21]
#define modbus_slave_input_reg_0x16  ModbusSlaveInputRegister[22]
#define modbus_slave_input_reg_0x17  ModbusSlaveInputRegister[23]
#define modbus_slave_input_reg_0x18  ModbusSlaveInputRegister[24]
#define modbus_slave_input_reg_0x19  ModbusSlaveInputRegister[25]
#define modbus_slave_input_reg_0x1A  ModbusSlaveInputRegister[26]
#define modbus_slave_input_reg_0x1B  ModbusSlaveInputRegister[27]
#define modbus_slave_input_reg_0x1C  ModbusSlaveInputRegister[28]
#define modbus_slave_input_reg_0x1D  ModbusSlaveInputRegister[29]
#define modbus_slave_input_reg_0x1E  ModbusSlaveInputRegister[30]
#define modbus_slave_input_reg_0x1F  ModbusSlaveInputRegister[31]
#define modbus_slave_input_reg_0x20  ModbusSlaveInputRegister[32]
#define modbus_slave_input_reg_0x21  ModbusSlaveInputRegister[33]
#define modbus_slave_input_reg_0x22  ModbusSlaveInputRegister[34]
#define modbus_slave_input_reg_0x23  ModbusSlaveInputRegister[35]
#define modbus_slave_input_reg_0x24  ModbusSlaveInputRegister[36]
#define modbus_slave_input_reg_0x25  ModbusSlaveInputRegister[37]
#define modbus_slave_input_reg_0x26  ModbusSlaveInputRegister[38]
#define modbus_slave_input_reg_0x27  ModbusSlaveInputRegister[39]
#define modbus_slave_input_reg_0x28  ModbusSlaveInputRegister[40]
#define modbus_slave_input_reg_0x29  ModbusSlaveInputRegister[41]
#define modbus_slave_input_reg_0x2A  ModbusSlaveInputRegister[42]
#define modbus_slave_input_reg_0x2B  ModbusSlaveInputRegister[43]
#define modbus_slave_input_reg_0x2C  ModbusSlaveInputRegister[44]
#define modbus_slave_input_reg_0x2D  ModbusSlaveInputRegister[45]
#define modbus_slave_input_reg_0x2E  ModbusSlaveInputRegister[46]
#define modbus_slave_input_reg_0x2F  ModbusSlaveInputRegister[47]
#define modbus_slave_input_reg_0x30  ModbusSlaveInputRegister[48]
#define modbus_slave_input_reg_0x31  ModbusSlaveInputRegister[49]
#define modbus_slave_input_reg_0x32  ModbusSlaveInputRegister[50]
#define modbus_slave_input_reg_0x33  ModbusSlaveInputRegister[51]
#define modbus_slave_input_reg_0x34  ModbusSlaveInputRegister[52]
#define modbus_slave_input_reg_0x35  ModbusSlaveInputRegister[53]
#define modbus_slave_input_reg_0x36  ModbusSlaveInputRegister[54]
#define modbus_slave_input_reg_0x37  ModbusSlaveInputRegister[55]
#define modbus_slave_input_reg_0x38  ModbusSlaveInputRegister[56]
#define modbus_slave_input_reg_0x39  ModbusSlaveInputRegister[57]
#define modbus_slave_input_reg_0x3A  ModbusSlaveInputRegister[58]
#define modbus_slave_input_reg_0x3B  ModbusSlaveInputRegister[59]
#define modbus_slave_input_reg_0x3C  ModbusSlaveInputRegister[60]
#define modbus_slave_input_reg_0x3D  ModbusSlaveInputRegister[61]
#define modbus_slave_input_reg_0x3E  ModbusSlaveInputRegister[62]
#define modbus_slave_input_reg_0x3F  ModbusSlaveInputRegister[63]


#define modbus_slave_bit_0x00        ModbusSlaveBit[0]
#define modbus_slave_bit_0x01        ModbusSlaveBit[1]
#define modbus_slave_bit_0x02        ModbusSlaveBit[2]
#define modbus_slave_bit_0x03        ModbusSlaveBit[3]
#define modbus_slave_bit_0x04        ModbusSlaveBit[4]
#define modbus_slave_bit_0x05        ModbusSlaveBit[5]
#define modbus_slave_bit_0x06        ModbusSlaveBit[6]
#define modbus_slave_bit_0x07        ModbusSlaveBit[7]
#define modbus_slave_bit_0x08        ModbusSlaveBit[8]
#define modbus_slave_bit_0x09        ModbusSlaveBit[9]
#define modbus_slave_bit_0x0A        ModbusSlaveBit[10]
#define modbus_slave_bit_0x0B        ModbusSlaveBit[11]
#define modbus_slave_bit_0x0C        ModbusSlaveBit[12]
#define modbus_slave_bit_0x0D        ModbusSlaveBit[13]
#define modbus_slave_bit_0x0E        ModbusSlaveBit[14]
#define modbus_slave_bit_0x0F        ModbusSlaveBit[15]
#define modbus_slave_bit_0x10        ModbusSlaveBit[16]
#define modbus_slave_bit_0x11        ModbusSlaveBit[17]
#define modbus_slave_bit_0x12        ModbusSlaveBit[18]
#define modbus_slave_bit_0x13        ModbusSlaveBit[19]
#define modbus_slave_bit_0x14        ModbusSlaveBit[20]
#define modbus_slave_bit_0x15        ModbusSlaveBit[21]
#define modbus_slave_bit_0x16        ModbusSlaveBit[22]
#define modbus_slave_bit_0x17        ModbusSlaveBit[23]
#define modbus_slave_bit_0x18        ModbusSlaveBit[24]
#define modbus_slave_bit_0x19        ModbusSlaveBit[25]
#define modbus_slave_bit_0x1A        ModbusSlaveBit[26]
#define modbus_slave_bit_0x1B        ModbusSlaveBit[27]
#define modbus_slave_bit_0x1C        ModbusSlaveBit[28]
#define modbus_slave_bit_0x1D        ModbusSlaveBit[29]
#define modbus_slave_bit_0x1E        ModbusSlaveBit[30]
#define modbus_slave_bit_0x1F        ModbusSlaveBit[31]
#define modbus_slave_bit_0x20        ModbusSlaveBit[32]
#define modbus_slave_bit_0x21        ModbusSlaveBit[33]
#define modbus_slave_bit_0x22        ModbusSlaveBit[34]
#define modbus_slave_bit_0x23        ModbusSlaveBit[35]
#define modbus_slave_bit_0x24        ModbusSlaveBit[36]
#define modbus_slave_bit_0x25        ModbusSlaveBit[37]
#define modbus_slave_bit_0x26        ModbusSlaveBit[38]
#define modbus_slave_bit_0x27        ModbusSlaveBit[39]
#define modbus_slave_bit_0x28        ModbusSlaveBit[40]
#define modbus_slave_bit_0x29        ModbusSlaveBit[41]
#define modbus_slave_bit_0x2A        ModbusSlaveBit[42]
#define modbus_slave_bit_0x2B        ModbusSlaveBit[43]
#define modbus_slave_bit_0x2C        ModbusSlaveBit[44]
#define modbus_slave_bit_0x2D        ModbusSlaveBit[45]
#define modbus_slave_bit_0x2E        ModbusSlaveBit[46]
#define modbus_slave_bit_0x2F        ModbusSlaveBit[47]
#define modbus_slave_bit_0x30        ModbusSlaveBit[48]
#define modbus_slave_bit_0x31        ModbusSlaveBit[49]
#define modbus_slave_bit_0x32        ModbusSlaveBit[50]
#define modbus_slave_bit_0x33        ModbusSlaveBit[51]
#define modbus_slave_bit_0x34        ModbusSlaveBit[52]
#define modbus_slave_bit_0x35        ModbusSlaveBit[53]
#define modbus_slave_bit_0x36        ModbusSlaveBit[54]
#define modbus_slave_bit_0x37        ModbusSlaveBit[55]
#define modbus_slave_bit_0x38        ModbusSlaveBit[56]
#define modbus_slave_bit_0x39        ModbusSlaveBit[57]
#define modbus_slave_bit_0x3A        ModbusSlaveBit[58]
#define modbus_slave_bit_0x3B        ModbusSlaveBit[59]
#define modbus_slave_bit_0x3C        ModbusSlaveBit[60]
#define modbus_slave_bit_0x3D        ModbusSlaveBit[61]
#define modbus_slave_bit_0x3E        ModbusSlaveBit[62]
#define modbus_slave_bit_0x3F        ModbusSlaveBit[63]

#endif


#endif
