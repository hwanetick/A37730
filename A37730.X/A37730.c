// This is firmware for the Gun Driver Board



#include "A37730.h"
#include "A37730_CONFIG.h"

_FOSC(EC & CSW_FSCM_OFF); 
_FWDT(WDT_ON & WDTPSA_512 & WDTPSB_8);  // 8 Second watchdog timer
_FBORPOR(PWRT_64 & PWRT_OFF & BORV_45 & PBOR_OFF & MCLR_EN);
_FBS(WR_PROTECT_BOOT_OFF & NO_BOOT_CODE & NO_BOOT_EEPROM & NO_BOOT_RAM);
_FSS(WR_PROT_SEC_OFF & NO_SEC_CODE & NO_SEC_EEPROM & NO_SEC_RAM);
_FGS(CODE_PROT_OFF);
_FICD(PGD);


void ETMCanSpoofPulseSyncNextPulseLevel();
void ETMCanSpoofAFCHighSpeedDataLog();
unsigned int next_pulse_count = 0;
unsigned int spoof_counter = 0;



void DoStateMachine(void); // This handles the state machine for the interface board
void InitializeA37730(void); // Initialize the A37730 for operation
void DoStartupLEDs(void); // Used to flash the LEDs at startup
void ResetAllFaultInfo(void); // Clears all fault/status bits and resets all fault/status counters
unsigned int CheckHeaterFault(void);  // Check for any fault the requires the heater to be turned off
unsigned int CheckFault(void); // Checks for any fault that does not require the heater to be turned off 
unsigned int CheckPreHVFault(void);
unsigned int CheckPreTopFault(void);

// Helper functions for DoA37730
void DoA37730(void);
/*
  DoA37730 is called every time the processor cycles through it's control loop
  If _T2IF is set (indicates 10mS has passed) it executes everything that happens on 10mS time scale
*/

void DoPostTriggerProcess(void);
void UpdateFaults(void); // Update the fault bits based on analog/digital parameters
void UpdateLEDandStatusOutuputs(void);  // Updates the LED and status outputs based on the system state

void SetStateMessage (unsigned int message);  // Sets bits for modbus state message
unsigned int GetModbusResetEnable(void);


#ifdef __noModbusLibrary

unsigned char modbus_cmd_byte[8];

unsigned int ETM_modbus_state;

unsigned int ModbusTimer;
unsigned int ModbusTest;

unsigned int ModbusSlaveHoldingRegister[SLAVE_HOLD_REG_ARRAY_SIZE];
unsigned int ModbusSlaveInputRegister[SLAVE_INPUT_REG_ARRAY_SIZE];
unsigned int ModbusSlaveBit[SLAVE_BIT_ARRAY_SIZE];


unsigned char  modbus_transmission_needed = 0;
unsigned char  modbus_receiving_flag = 0;
unsigned char  ETM_last_modbus_fail = 0;
  
unsigned char  modbus_slave_invalid_data = 0;

// This is used to time the Do control loop every 10ms
unsigned long timer_write_holding_var_10ms;
unsigned long timer_write_holding_var_1s;

//static MODBUS_RESP_SMALL*  ETMmodbus_resp_ptr[ETMMODBUS_CMD_QUEUE_SIZE];

BUFFERBYTE64 uart1_input_buffer;   
BUFFERBYTE64 uart1_output_buffer; 

MODBUS_MESSAGE  current_command_ptr;

//static unsigned char normal_reply_length;


void ETMModbusInit(void);
void ETMModbusSlaveDoModbus(void);
void ReceiveCommand(MODBUS_MESSAGE * ptr);
void SendResponse(MODBUS_MESSAGE * ptr);
void ProcessCommand (MODBUS_MESSAGE * ptr);
void CheckValidData(MODBUS_MESSAGE * ptr);
void CheckDeviceFailure(MODBUS_MESSAGE * ptr);
void ClearModbusMessage(MODBUS_MESSAGE * ptr);
unsigned int LookForMessage (void);
unsigned int checkCRC(unsigned char * ptr, unsigned int size);
void SetCustomIP(void);

#endif

/*
  Helper Function used to Enable/Disable Supplies on Converter logic board
*/
void EnableHeater(void);
void DisableHeater(void);
void EnableHighVoltage(void);
void DisableHighVoltage(void);
void EnableTopSupply(void);
void EnableBeam(void);
void DisableBeam(void);



// Digital Input Functions (NEEDS and ETM Module)
//void ETMDigitalInitializeInput(TYPE_DIGITAL_INPUT* input, unsigned int initial_value, unsigned int filter_time);
//void ETMDigitalUpdateInput(TYPE_DIGITAL_INPUT* input, unsigned int current_value);




// -------------------------- GLOBAL VARIABLES --------------------------- //
TYPE_GLOBAL_DATA_A37730 global_data_A37730;
LTC265X U29_LTC2654;

int main(void) {
  global_data_A37730.control_state = STATE_START_UP;
  while (1) {
    DoStateMachine();
  }
}


void DoStateMachine(void) {
  switch (global_data_A37730.control_state) {

    
  case STATE_START_UP:
    InitializeA37730();
    DisableBeam();
    DisableHighVoltage();
    DisableHeater();
    _CONTROL_NOT_CONFIGURED = 1;
    _CONTROL_NOT_READY = 1;
    global_data_A37730.heater_start_up_attempts = 0;
    global_data_A37730.run_time_counter = 0;
    global_data_A37730.fault_holdoff_state = 0;
#ifndef __CAN_REFERENCE
    _CONTROL_NOT_CONFIGURED = 0;
#endif
    global_data_A37730.control_state = STATE_WAIT_FOR_CONFIG;
    break;

  case STATE_WAIT_FOR_CONFIG:
    DisableBeam();
    DisableHighVoltage();
    DisableHeater(); 
    global_data_A37730.current_state_msg = STATE_MESSAGE_START_UP;
    while (global_data_A37730.control_state == STATE_WAIT_FOR_CONFIG) {
      DoA37730();
      DoStartupLEDs();
      if ((global_data_A37730.run_time_counter >= LED_STARTUP_FLASH_TIME) && (_CONTROL_NOT_CONFIGURED == 0)) {
        global_data_A37730.control_state = STATE_RESET_FAULTS;
      }
    }
    break;
    
  case STATE_RESET_FAULTS:
    ResetAllFaultInfo();
    global_data_A37730.control_state = STATE_HEATER_DISABLED;
    break;
    
    
  case STATE_HEATER_DISABLED:
    DisableHeater();
    PIN_TRIG_PULSE_WIDTH_LIMITER = !OLL_TRIG_PULSE_DISABLE; //do not actively limit PW
    global_data_A37730.current_state_msg = STATE_MESSAGE_START_UP;
    global_data_A37730.analog_output_heater_voltage.set_point = 0;
    if (!global_data_A37730.request_heater_enable) {
      global_data_A37730.heater_start_up_attempts = 0;
    }
    global_data_A37730.heater_voltage_current_limited = 0;
    while (global_data_A37730.control_state == STATE_HEATER_DISABLED) {
      DoA37730();
      if (global_data_A37730.request_heater_enable) {
        global_data_A37730.control_state = STATE_HEATER_RAMP_UP;
      }
      if (CheckHeaterFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
      }
    }    
    break;


  case STATE_HEATER_RAMP_UP:
    _CONTROL_NOT_READY = 1;
    global_data_A37730.analog_output_heater_voltage.set_point = 0;
    global_data_A37730.heater_ramp_interval = 0;
    global_data_A37730.heater_start_up_attempts++;
    global_data_A37730.heater_ramp_up_time = MAX_HEATER_RAMP_UP_TIME;
    DisableBeam();
    DisableHighVoltage();
    EnableHeater();
    global_data_A37730.current_state_msg = STATE_MESSAGE_HEATER_RAMP_UP;
    while (global_data_A37730.control_state == STATE_HEATER_RAMP_UP) {
      DoA37730();
      if (global_data_A37730.analog_output_heater_voltage.set_point >= global_data_A37730.heater_voltage_target) {
        global_data_A37730.control_state = STATE_HEATER_WARM_UP;
      }
      if (!global_data_A37730.request_heater_enable) {
        global_data_A37730.control_state = STATE_HEATER_DISABLED;
      }
      if (CheckHeaterFault()) {
        if (global_data_A37730.heater_start_up_attempts > MAX_HEATER_START_UP_ATTEMPTS) {
          global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
        } else {
          global_data_A37730.control_state = STATE_FAULT_WARMUP_HEATER_OFF;
        }
      }
    }
    break;
    

  case STATE_HEATER_WARM_UP:
    _CONTROL_NOT_READY = 1;
    DisableBeam();
    DisableHighVoltage();
    global_data_A37730.current_state_msg = STATE_MESSAGE_HEATER_WARM_UP;
    global_data_A37730.heater_ramp_up_time = 0;
    global_data_A37730.heater_ramp_interval = 0;
    global_data_A37730.heater_warm_up_time_remaining = HEATER_WARM_UP_TIME;
    global_data_A37730.fault_holdoff_count = 0;
    global_data_A37730.fault_holdoff_state = FAULT_HOLDOFF_STATE;
    while (global_data_A37730.control_state == STATE_HEATER_WARM_UP) {
      DoA37730();
      if (ETMTickRunOnceEveryNMilliseconds(1000, &timer_write_holding_var_1s)) {
        global_data_A37730.fault_holdoff_count++;
        if (global_data_A37730.fault_holdoff_count >= CURRENT_LIMITED_FAULT_HOLDOFF_TIME) {
          global_data_A37730.fault_holdoff_state = 0;                 
        }
      }
      if (global_data_A37730.heater_warm_up_time_remaining == 0) {
        global_data_A37730.control_state = STATE_HEATER_WARM_UP_DONE;
      }
      if (!global_data_A37730.request_heater_enable) {
        global_data_A37730.control_state = STATE_HEATER_DISABLED;
      }
      if (CheckHeaterFault()) {
        if (global_data_A37730.heater_start_up_attempts > MAX_HEATER_START_UP_ATTEMPTS) {
          global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
        } else {
          global_data_A37730.control_state = STATE_FAULT_WARMUP_HEATER_OFF;
        }
      }
    }
    break;


  case STATE_HEATER_WARM_UP_DONE:
    _CONTROL_NOT_READY = 1;
    DisableBeam();
    DisableHighVoltage();
    global_data_A37730.current_state_msg = STATE_MESSAGE_HEATER_WARM_UP_DONE;
    global_data_A37730.heater_start_up_attempts = 0;
    while (global_data_A37730.control_state == STATE_HEATER_WARM_UP_DONE) {
      DoA37730();
      if (global_data_A37730.request_hv_enable) {
        global_data_A37730.control_state = STATE_POWER_SUPPLY_RAMP_UP;
      }
      if (!global_data_A37730.request_heater_enable) {
        global_data_A37730.control_state = STATE_HEATER_DISABLED;
      }
      if (CheckHeaterFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
      }
    }
    break;
    

  case STATE_POWER_SUPPLY_RAMP_UP:
    _CONTROL_NOT_READY = 1;
    DisableBeam();
    EnableHighVoltage();
    global_data_A37730.current_state_msg = STATE_MESSAGE_HV_ON;
    global_data_A37730.power_supply_startup_remaining = GUN_DRIVER_POWER_SUPPLY_STARTUP_TIME;
    while (global_data_A37730.control_state == STATE_POWER_SUPPLY_RAMP_UP) {
      DoA37730();

      if (global_data_A37730.power_supply_startup_remaining == 0) {
        global_data_A37730.control_state = STATE_HV_ON;
      }      
      if (!global_data_A37730.request_hv_enable) {
        global_data_A37730.control_state = STATE_HEATER_WARM_UP_DONE;
      }
      if (CheckPreHVFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_ON;
      }
      if (CheckHeaterFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
      }
    }
    break;


  case STATE_HV_ON:
    _CONTROL_NOT_READY = 1;
    DisableBeam();
    global_data_A37730.current_state_msg = STATE_MESSAGE_HV_ON;
    global_data_A37730.tick_timer = ETMTickGet();
    while (global_data_A37730.control_state == STATE_HV_ON) {
      DoA37730();
      if (ETMTickGreaterThanNMilliseconds(1000, global_data_A37730.tick_timer)) {
        global_data_A37730.control_state = STATE_TOP_ON;
      }
      if (!global_data_A37730.request_hv_enable) {
        global_data_A37730.control_state = STATE_HEATER_WARM_UP_DONE;
      }
      if (CheckPreTopFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_ON;
      }
      if (CheckHeaterFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
      }
    }
    break;  

    
  case STATE_TOP_ON:
    _CONTROL_NOT_READY = 1;
    DisableBeam();
    EnableTopSupply();
    global_data_A37730.current_state_msg = STATE_MESSAGE_HV_ON;
    global_data_A37730.tick_timer = ETMTickGet();
    while (global_data_A37730.control_state == STATE_TOP_ON) {
      DoA37730();
      if (ETMTickGreaterThanNMilliseconds(1000, global_data_A37730.tick_timer)) {
        global_data_A37730.control_state = STATE_TOP_READY;
      }
      if (!global_data_A37730.request_hv_enable) {
        global_data_A37730.control_state = STATE_HEATER_WARM_UP_DONE;
      }
      if (CheckPreTopFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_ON;
      }
      if (CheckHeaterFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
      }
    }
    break;
    
  case STATE_TOP_READY:
    _CONTROL_NOT_READY = 1;
    DisableBeam();
    global_data_A37730.current_state_msg = STATE_MESSAGE_HV_ON;
    while (global_data_A37730.control_state == STATE_TOP_READY) {
      DoA37730();
      if (global_data_A37730.request_beam_enable) {        
        global_data_A37730.control_state = STATE_BEAM_ENABLE;  
      }   
      if (!global_data_A37730.request_hv_enable) {
        global_data_A37730.control_state = STATE_HEATER_WARM_UP_DONE;
      }
      if (CheckFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_ON;
      }
      if (CheckHeaterFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
      }
    }
    break;

  case STATE_BEAM_ENABLE:
    EnableBeam();
    global_data_A37730.current_state_msg = STATE_MESSAGE_BEAM_ENABLE;
    _CONTROL_NOT_READY = 0;
    while (global_data_A37730.control_state == STATE_BEAM_ENABLE) {
      DoA37730();
      if (!global_data_A37730.request_beam_enable) {
        global_data_A37730.control_state = STATE_TOP_READY;
      }
      if (!global_data_A37730.request_hv_enable) {
        global_data_A37730.control_state = STATE_HEATER_WARM_UP_DONE;
      }
      if (CheckFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_ON;
      }
      if (CheckHeaterFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
      }
    }
    break;
 
  case STATE_FAULT_HEATER_ON:
    _CONTROL_NOT_READY = 1;
    DisableBeam();
    DisableHighVoltage();
    global_data_A37730.current_state_msg = STATE_MESSAGE_FAULT_HEATER_ON;   
    ETMAnalogClearFaultCounters(&global_data_A37730.input_hv_v_mon);
    ETMAnalogClearFaultCounters(&global_data_A37730.input_top_v_mon);
    while (global_data_A37730.control_state == STATE_FAULT_HEATER_ON) {
      DoA37730();
      if (global_data_A37730.reset_active) {
	    global_data_A37730.control_state = STATE_HEATER_WARM_UP_DONE;
      }
      if (!global_data_A37730.request_heater_enable) {
        global_data_A37730.control_state = STATE_HEATER_DISABLED;
      }
      if (CheckHeaterFault()) {
        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
      }
    }
    break;

  case STATE_FAULT_HEATER_OFF:
    _CONTROL_NOT_READY = 1;
#ifdef __CAN_REFERENCE
    _CONTROL_NOT_CONFIGURED = 1;
    global_data_A37730.control_config = 0;
#endif
    DisableBeam();
    DisableHighVoltage();
    DisableHeater();
    global_data_A37730.current_state_msg = STATE_MESSAGE_FAULT_HEATER_OFF;
    ETMAnalogClearFaultCounters(&global_data_A37730.input_htr_v_mon);
    ETMAnalogClearFaultCounters(&global_data_A37730.input_htr_i_mon);
    ETMAnalogClearFaultCounters(&global_data_A37730.input_bias_v_mon);
    while (global_data_A37730.control_state == STATE_FAULT_HEATER_OFF) {
      DoA37730();
      if (global_data_A37730.reset_active) {
        global_data_A37730.control_state = STATE_WAIT_FOR_CONFIG;
      }
    }
    break;


  case STATE_FAULT_WARMUP_HEATER_OFF:
    _CONTROL_NOT_READY = 1;
#ifdef __CAN_REFERENCE
    _CONTROL_NOT_CONFIGURED = 1;
    global_data_A37730.control_config = 0;
#endif
    DisableBeam();
    DisableHighVoltage();
    DisableHeater();
    global_data_A37730.current_state_msg = STATE_MESSAGE_FAULT_HEATER_OFF;
    ETMAnalogClearFaultCounters(&global_data_A37730.input_htr_v_mon);
    ETMAnalogClearFaultCounters(&global_data_A37730.input_htr_i_mon);
    ETMAnalogClearFaultCounters(&global_data_A37730.input_bias_v_mon);
    global_data_A37730.fault_restart_remaining = HEATER_AUTO_RESTART_TIME;
    while (global_data_A37730.control_state == STATE_FAULT_WARMUP_HEATER_OFF) {
      DoA37730();
      if (global_data_A37730.fault_restart_remaining == 0) {
        global_data_A37730.control_state = STATE_WAIT_FOR_CONFIG;
        global_data_A37730.reset_active = 0;
      }
//      if (global_data_A37730.heater_start_up_attempts > MAX_HEATER_START_UP_ATTEMPTS) {
//        global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
//      }
    }
    break;


  case STATE_FAULT_HEATER_FAILURE:
    _CONTROL_NOT_READY = 1;
    DisableBeam();
    DisableHighVoltage();
    DisableHeater();
    global_data_A37730.current_state_msg = STATE_MESSAGE_FAULT_HEATER_OFF;
//    _FAULT_HEATER_STARTUP_FAILURE = 1;
    while (global_data_A37730.control_state == STATE_FAULT_HEATER_FAILURE) {
      // Can't leave this state without power cycle
      DoA37730();
    }
    break;


  default:
    global_data_A37730.control_state = STATE_FAULT_HEATER_OFF;
    break;

  }
}


void InitializeA37730(void) {
//  IPCONFIG ip_config;   //ETHERNET
  // Initialize the status register and load the inhibit and fault masks

  _CONTROL_REGISTER = 0;
  _NOT_LOGGED_REGISTER = 0;

  // --------- BEGIN IO PIN CONFIGURATION ------------------

  PIN_HV_ON_SELECT = OLL_SELECT_SERIAL_CONTROL;             //serial control
  PIN_BEAM_ENABLE_SELECT = OLL_SELECT_SERIAL_CONTROL;       //serial control
  PIN_TRIGGER_INPUT_SELECT = !OLL_SELECT_OPTICAL_TRIGGER;   //optical trig
  PIN_INTEGRATOR_1 = 0;
  PIN_INTEGRATOR_2 = 0;
	  
  // ---- Configure the dsPIC ADC Module Analog Inputs------------ //
  ADPCFG = 0xFFFF;             // all are digital I/O
 
  // Initialize all I/O Registers
  TRISA = A37730_TRISA_VALUE;
  TRISB = A37730_TRISB_VALUE;
  TRISC = A37730_TRISC_VALUE;
  TRISD = A37730_TRISD_VALUE;
  TRISF = A37730_TRISF_VALUE;
  TRISG = A37730_TRISG_VALUE;

  // Config SPI2 for DAC
  ConfigureSPI(ETM_SPI_PORT_1, A37730_SPI1CON_VALUE, 0, A37730_SPI1STAT_VALUE, SPI_CLK_1_MBIT, FCY_CLK);  
  

  // ---------- Configure Timers ----------------- //

  
      // Initialize TMR3
  // Setup Timer 3 to measure interpulse period.
  PR3   = A37730_PR3_VALUE;
  TMR3  = 0;
  _T3IF = 0;
//  _T3IP = 5;
  _T3IP = 2;
  T3CON = A37730_T3CON_VALUE;
  
  _INT4IF = 0;		// Clear Interrupt flag
  _INT4IE = 1;		// Enable INT4 Interrupt
  _INT4EP = 0; 	    // Interrupt on rising edge
  _INT4IP = 7;		// Set interrupt to highest priority
//  
//    

  

  //Configure EEPROM
  ETMEEPromUseExternal();
  ETMEEPromConfigureExternalDevice(EEPROM_SIZE_8K_BYTES, FCY_CLK, 400000, EEPROM_I2C_ADDRESS_0, 1);
   
  PIN_HV_ON_SERIAL = !OLL_SERIAL_ENABLE;
  PIN_BEAM_ENABLE_SERIAL = !OLL_SERIAL_ENABLE;         
  
  
  // ------------- Configure Internal ADC --------- //
  ADCON1 = ADCON1_SETTING;             // Configure the high speed ADC module based on H file parameters
  ADCON2 = ADCON2_SETTING;             // Configure the high speed ADC module based on H file parameters
  ADCON3 = ADCON3_SETTING;             // Configure the high speed ADC module based on H file parameters
  ADCHS  = ADCHS_SETTING;              // Configure the high speed ADC module based on H file parameters
  
  ADPCFG = ADPCFG_SETTING;             // Set which pins are analog and which are digital I/O
  ADCSSL = ADCSSL_SETTING;             // Set which analog pins are scanned

  _ADIF = 0;
  _ADIP = 6; // This needs to be higher priority than the CAN interrupt (Which defaults to 4)
  _ADIE = 1;
  _ADON = 1;
  
  
#ifdef __MODE_MODBUS_MONITOR
  ETMModbusInit();
#endif

#if 0  
  // Initialize TCPmodbus Module
  ip_config.remote_ip_addr   = ETMEEPromReadWord(EEPROM_REGISTER_REMOTE_IP_ADDRESS);
  ip_config.remote_ip_addr <<= 16;
  ip_config.remote_ip_addr  += ETMEEPromReadWord(EEPROM_REGISTER_REMOTE_IP_ADDRESS + 1);
  ip_config.ip_addr          = ETMEEPromReadWord(EEPROM_REGISTER_IP_ADDRESS);
  ip_config.ip_addr        <<= 16;
  ip_config.ip_addr         += ETMEEPromReadWord(EEPROM_REGISTER_IP_ADDRESS + 1);

  Nop();
  Nop();

  //ip_config.remote_ip_addr = 0x0F46A8C0;  // 192.168.70.15
  //ip_config.ip_addr        = 0x6346A8C0;  // 192.168.70.99

  
  
  if ((ip_config.remote_ip_addr == 0xFFFFFFFF) || (ip_config.remote_ip_addr == 0x00000000)) {
    ip_config.remote_ip_addr = DEFAULT_REMOTE_IP_ADDRESS;
  }
  if ((ip_config.ip_addr == 0xFFFFFFFF) || (ip_config.ip_addr == 0x00000000)) {
    ip_config.ip_addr = DEFAULT_IP_ADDRESS;
  }
#else
  
//    ip_config.ip_addr =  ((unsigned long)modbus_slave_hold_reg_0x0D << 24) & 0xFF000000;   //ETHERNET
//    ip_config.ip_addr += ((unsigned long)modbus_slave_hold_reg_0x0C << 16) & 0x00FF0000;
//    ip_config.ip_addr += ((unsigned long)modbus_slave_hold_reg_0x0B << 8) & 0x0000FF00;
//    ip_config.ip_addr += (unsigned long)modbus_slave_hold_reg_0x0A & 0x000000FF;
//  
//    if ((ip_config.ip_addr == 0xFFFFFFFF) || (ip_config.ip_addr == 0x00000000)) {
//      ip_config.ip_addr = DEFAULT_IP_ADDRESS;
//    }
//  
//    ip_config.remote_ip_addr = DEFAULT_REMOTE_IP_ADDRESS;

#endif
//  TCPmodbus_init(&ip_config); //ETHERNET

  

#ifdef __CAN_ENABLED
  // Initialize the Can module
  ETMCanSlaveInitialize(CAN_PORT_2, FCY_CLK, ETM_CAN_ADDR_GUN_DRIVER_BOARD, _PIN_RC4, 4, _PIN_RC3, _PIN_RC3);
  ETMCanSlaveLoadConfiguration(37474, BOARD_DASH_NUMBER, FIRMWARE_AGILE_REV, FIRMWARE_BRANCH, FIRMWARE_MINOR_REV);
#endif

  SetupLTC265X(&U29_LTC2654, ETM_SPI_PORT_2, FCY_CLK, LTC265X_SPI_2_5_M_BIT, _PIN_RG15, _PIN_RC1);
  
  
  if (ETMTickNotInitialized()) {
    ETMTickInitialize(FCY_CLK, ETM_TICK_USE_TIMER_2);  
  }
  
#ifdef __ETHERNET_REFERENCE
    
  global_data_A37730.ethernet_hv_ref  = ETMEEPromReadWord(0x682);
  global_data_A37730.ethernet_top_ref = ETMEEPromReadWord(0x681);
  global_data_A37730.ethernet_htr_ref = ETMEEPromReadWord(0x680);  
  
#endif

  // Initialize PIC ADC Inputs

  ETMAnalogInitializeInput(&global_data_A37730.input_hv_v_mon,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_HV_VMON_FIXED_SCALE),
			   ADC_HV_VMON_FIXED_OFFSET,
			   ANALOG_INPUT_0,
			   NO_OVER_TRIP,
			   NO_UNDER_TRIP,
			   ADC_HV_VMON_RELATIVE_TRIP_SCALE,
			   ADC_HV_VMON_RELATIVE_TRIP_FLOOR,
			   ADC_HV_VMON_RELATIVE_TRIP_COUNT,
			   NO_ABSOLUTE_COUNTER);

  ETMAnalogInitializeInput(&global_data_A37730.input_hv_i_mon,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_HV_IMON_FIXED_SCALE),
			   ADC_HV_IMON_FIXED_OFFSET,
			   ANALOG_INPUT_1,
			   NO_OVER_TRIP,
			   NO_UNDER_TRIP,
			   NO_TRIP_SCALE,
			   NO_FLOOR,
			   NO_RELATIVE_COUNTER,
			   NO_ABSOLUTE_COUNTER);
  

  ETMAnalogInitializeInput(&global_data_A37730.input_gun_i_peak,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_GUN_I_PEAK_FIXED_SCALE),
			   ADC_GUN_I_PEAK_FIXED_OFFSET,
			   ANALOG_INPUT_2,
			   NO_OVER_TRIP,
			   NO_UNDER_TRIP,
			   NO_TRIP_SCALE,
			   NO_FLOOR,
			   NO_RELATIVE_COUNTER,
			   NO_ABSOLUTE_COUNTER);

  ETMAnalogInitializeInput(&global_data_A37730.input_htr_v_mon,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_HTR_V_MON_FIXED_SCALE),
			   ADC_HTR_V_MON_FIXED_OFFSET,
			   ANALOG_INPUT_3,
			   NO_OVER_TRIP,
			   NO_UNDER_TRIP,
			   ADC_HTR_V_MON_RELATIVE_TRIP_SCALE,
			   ADC_HTR_V_MON_RELATIVE_TRIP_FLOOR,
			   ADC_HTR_V_MON_RELATIVE_TRIP_COUNT,
			   NO_ABSOLUTE_COUNTER);

  ETMAnalogInitializeInput(&global_data_A37730.input_htr_i_mon,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_HTR_I_MON_FIXED_SCALE),
			   ADC_HTR_I_MON_FIXED_OFFSET,
			   ANALOG_INPUT_4,
			   ADC_HTR_I_MON_OVER_LIMIT_ABSOLUTE,
			   ADC_HTR_I_MON_UNDER_LIMIT_ABSOLUTE,
			   NO_TRIP_SCALE,
			   NO_FLOOR,
			   NO_RELATIVE_COUNTER,
			   ADC_HTR_I_MON_ABSOLUTE_TRIP_TIME);

  ETMAnalogInitializeInput(&global_data_A37730.input_top_v_mon,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_TOP_V_MON_FIXED_SCALE),
			   ADC_TOP_V_MON_FIXED_OFFSET,
			   ANALOG_INPUT_5,
			   NO_OVER_TRIP,
			   NO_UNDER_TRIP,
			   ADC_TOP_V_MON_RELATIVE_TRIP_SCALE,
			   ADC_TOP_V_MON_RELATIVE_TRIP_FLOOR,
			   ADC_TOP_V_MON_RELATIVE_TRIP_TIME,
			   NO_ABSOLUTE_COUNTER);

  ETMAnalogInitializeInput(&global_data_A37730.input_bias_v_mon,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_BIAS_V_MON_FIXED_SCALE),
			   ADC_BIAS_V_MON_FIXED_OFFSET,
			   ANALOG_INPUT_6,
			   ADC_BIAS_V_MON_OVER_LIMIT_ABSOLUTE,
			   ADC_BIAS_V_MON_UNDER_LIMIT_ABSOLUTE,
			   NO_TRIP_SCALE,
			   NO_FLOOR,
			   NO_RELATIVE_COUNTER,
			   ADC_BIAS_V_MON_ABSOLUTE_TRIP_TIME);

  ETMAnalogInitializeInput(&global_data_A37730.input_24_v_mon,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_24_V_MON_FIXED_SCALE),
			   ADC_24_V_MON_FIXED_OFFSET,
			   ANALOG_INPUT_7,
			   NO_OVER_TRIP,
			   NO_UNDER_TRIP,
			   NO_TRIP_SCALE,
			   NO_FLOOR,
			   NO_RELATIVE_COUNTER,
			   NO_ABSOLUTE_COUNTER);

  ETMAnalogInitializeInput(&global_data_A37730.input_temperature_mon,
			   MACRO_DEC_TO_SCALE_FACTOR_16(ADC_TEMPERATURE_MON_FIXED_SCALE),
			   ADC_TEMPERATURE_MON_FIXED_OFFSET,
			   ANALOG_INPUT_8,
			   NO_OVER_TRIP,
			   NO_UNDER_TRIP,
			   NO_TRIP_SCALE,
			   NO_FLOOR,
			   NO_RELATIVE_COUNTER,
			   NO_ABSOLUTE_COUNTER);





  // ----------------- Initialize PIC's internal ADC Inputs --------------------- //






  // ------------- Initialize Converter Logic Board DAC Outputs ------------------------------ //
  ETMAnalogInitializeOutput(&global_data_A37730.analog_output_high_voltage,
			    MACRO_DEC_TO_SCALE_FACTOR_16(DAC_HIGH_VOLTAGE_FIXED_SCALE),
			    DAC_HIGH_VOLTAGE_FIXED_OFFSET,
			    ANALOG_OUTPUT_0,
			    HIGH_VOLTAGE_MAX_SET_POINT,
			    HIGH_VOLTAGE_MIN_SET_POINT,
			    0);

  ETMAnalogInitializeOutput(&global_data_A37730.analog_output_top_voltage,
			    MACRO_DEC_TO_SCALE_FACTOR_16(DAC_TOP_VOLTAGE_FIXED_SCALE),
			    DAC_TOP_VOLTAGE_FIXED_OFFSET,
			    ANALOG_OUTPUT_1,
			    TOP_VOLTAGE_MAX_SET_POINT,
			    TOP_VOLTAGE_MIN_SET_POINT,
			    0);
  
  ETMAnalogInitializeOutput(&global_data_A37730.analog_output_heater_voltage,
			    MACRO_DEC_TO_SCALE_FACTOR_16(DAC_HEATER_VOLTAGE_FIXED_SCALE),
			    DAC_HEATER_VOLTAGE_FIXED_OFFSET,
			    ANALOG_OUTPUT_2,
			    HEATER_VOLTAGE_MAX_SET_POINT,
			    HEATER_VOLTAGE_MIN_SET_POINT,
			    0);  


  //Reset faults/warnings and inputs
  ResetAllFaultInfo();
  
  
  
}


void SetCustomIP(void) {
//  IPCONFIG ip_config;  //ETHERNET
//  
//    //Format:  192.168.70.99
//    //          A   B  C  D 
//
//  ip_config.ip_addr =  ((unsigned long)modbus_slave_hold_reg_0x0D << 24) & 0xFF000000;
//  ip_config.ip_addr += ((unsigned long)modbus_slave_hold_reg_0x0C << 16) & 0x00FF0000;
//  ip_config.ip_addr += ((unsigned long)modbus_slave_hold_reg_0x0B << 8) & 0x0000FF00;
//  ip_config.ip_addr += (unsigned long)modbus_slave_hold_reg_0x0A & 0x000000FF;
//  
//  if ((ip_config.ip_addr == 0xFFFFFFFF) || (ip_config.ip_addr == 0x00000000)) {
//    ip_config.ip_addr = DEFAULT_IP_ADDRESS;
//  }
//  ip_config.remote_ip_addr = DEFAULT_REMOTE_IP_ADDRESS;
//      
//  TCPmodbus_task(1);  //close socket
//
//  TCPmodbus_init(&ip_config);
    Nop();
}


void DoStartupLEDs(void) {
  switch (((global_data_A37730.run_time_counter >> 4) & 0b11)) {
    
  case 0:
    PIN_LED_GREEN_1 = OLL_LED_ON;
    PIN_LED_GREEN_2 = !OLL_LED_ON;
    PIN_LED_RED = !OLL_LED_ON;
    break;
    
  case 1:
    PIN_LED_GREEN_1 = !OLL_LED_ON;
    PIN_LED_GREEN_2 = OLL_LED_ON;
    PIN_LED_RED = !OLL_LED_ON;
    break;
    
  case 2:
    PIN_LED_GREEN_1 = !OLL_LED_ON;
    PIN_LED_GREEN_2 = !OLL_LED_ON;
    PIN_LED_RED = OLL_LED_ON;
    break;
    
  case 3:
    PIN_LED_GREEN_1 = !OLL_LED_ON;
    PIN_LED_GREEN_2 = !OLL_LED_ON;
    PIN_LED_RED = !OLL_LED_ON;
    break;
  }
}


void ResetAllFaultInfo(void) {

  _FAULT_REGISTER = 0;
  _WARNING_REGISTER = 0;

  // Initialize Digital Input Filters for FPGA Status
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_coverter_logic_pcb_rev_mismatch       , 0, 30);   
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_firmware_major_rev_mismatch           , 0, 30);   
////  ETMDigitalInitializeInput(&global_data_A37730.fpga_firmware_minor_rev_mismatch           , 0, 30);   
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_arc                                   , 0, 5);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_arc_high_voltage_inihibit_active      , 0, 0);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_heater_voltage_less_than_4_5_volts    , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_module_temp_greater_than_65_C         , 0, 30); 
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_module_temp_greater_than_75_C         , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_pulse_width_limiting_active           , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_prf_fault                             , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_current_monitor_pulse_width_fault     , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_grid_module_hardware_fault            , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_grid_module_over_voltage_fault        , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_grid_module_under_voltage_fault       , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_grid_module_bias_voltage_fault        , 0, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.fpga_hv_regulation_warning                 , 0, 30);
//
  // Initialize Digital Input Filters For PIC "Digital" Inputs
  ETMDigitalInitializeInput(&global_data_A37730.gh_digital_bias_flt              , 1, 30);
  ETMDigitalInitializeInput(&global_data_A37730.gh_digital_hw_flt                , 1, 30);
  ETMDigitalInitializeInput(&global_data_A37730.gh_digital_top_htr_ov_flt        , 1, 30);
  ETMDigitalInitializeInput(&global_data_A37730.gh_digital_top_htr_uv_flt        , 1, 30);
  ETMDigitalInitializeInput(&global_data_A37730.digital_temp_lt_75               , 1, 30);
//  ETMDigitalInitializeInput(&global_data_A37730.                         , 1, 30);

 
  ETMDigitalInitializeInput(&global_data_A37730.interlock_relay_closed           , 0, 4);
  ETMDigitalInitializeInput(&global_data_A37730.enable_relay_closed              , 0, 4);
 
  
  // Reset all the Analog input fault counters

  ETMAnalogClearFaultCounters(&global_data_A37730.input_hv_v_mon);
  ETMAnalogClearFaultCounters(&global_data_A37730.input_hv_i_mon);
  ETMAnalogClearFaultCounters(&global_data_A37730.input_gun_i_peak);
  ETMAnalogClearFaultCounters(&global_data_A37730.input_htr_v_mon);
  ETMAnalogClearFaultCounters(&global_data_A37730.input_htr_i_mon);
  ETMAnalogClearFaultCounters(&global_data_A37730.input_top_v_mon);
  ETMAnalogClearFaultCounters(&global_data_A37730.input_bias_v_mon);
  ETMAnalogClearFaultCounters(&global_data_A37730.input_24_v_mon);

//  ETMAnalogClearFaultCounters(&global_data_A37730.pos_5v_mon);
//  ETMAnalogClearFaultCounters(&global_data_A37730.pos_15v_mon);
//  ETMAnalogClearFaultCounters(&global_data_A37730.neg_15v_mon);


  global_data_A37730.dac_write_error_count = 0;
  global_data_A37730.dac_write_failure = 0;
  global_data_A37730.dac_write_failure_count = 0;
}


unsigned int CheckHeaterFault(void) {
  unsigned int fault = 0;
  fault =  _FAULT_ADC_HTR_V_MON_OVER_RELATIVE;
  fault |= _FAULT_ADC_HTR_V_MON_UNDER_RELATIVE;
  fault |= _FAULT_HEATER_VOLTAGE_CURRENT_LIMITED;
  fault |= _FAULT_ADC_HTR_I_MON_OVER_ABSOLUTE;
  fault |= _FAULT_ADC_HTR_I_MON_UNDER_ABSOLUTE;
  fault |= _FAULT_DIGITAL_OVER_TEMP;
  fault |= _FAULT_GRID_HEATER_HARDWARE;
  fault |= _FAULT_HEATER_RAMP_TIMEOUT;
  if (fault) {
    return 1;
  } else {
    return 0;
  }
}


unsigned int CheckFault(void) {
  unsigned int fault = 0;
  fault  = _FAULT_ADC_HV_V_MON_OVER_RELATIVE;
  fault |= _FAULT_ADC_HV_V_MON_UNDER_RELATIVE;
  fault |= _FAULT_ADC_TOP_V_MON_OVER_RELATIVE;
  fault |= _FAULT_ADC_TOP_V_MON_UNDER_RELATIVE;
  fault |= _FAULT_ADC_BIAS_V_MON_OVER_ABSOLUTE;
  fault |= _FAULT_ADC_BIAS_V_MON_UNDER_ABSOLUTE;
  fault |= _FAULT_ADC_DIGITAL_ARC;
  fault |= _STATUS_INTERLOCK_INHIBITING_HV;
  fault |= _FPGA_CURRENT_MONITOR_PULSE_WIDTH_FAULT;
  fault |= _FAULT_OVER_PRF;
  if (fault) {
    return 1;
  } else {
    return 0;
  }
}

unsigned int CheckPreTopFault(void) {
  unsigned int fault = 0;
  fault  = _FAULT_ADC_HV_V_MON_OVER_RELATIVE;
  fault |= _FAULT_ADC_HV_V_MON_UNDER_RELATIVE;
  fault |= _FAULT_ADC_DIGITAL_ARC;
  fault |= _STATUS_INTERLOCK_INHIBITING_HV;
  fault |= _FPGA_CURRENT_MONITOR_PULSE_WIDTH_FAULT;
  fault |= _FAULT_OVER_PRF;
  if (fault) {
    return 1;
  } else {
    return 0;
  }
}



unsigned int CheckPreHVFault(void) {
  unsigned int fault = 0;
  fault  = _FAULT_ADC_HV_V_MON_OVER_RELATIVE;
  fault |= _FAULT_ADC_DIGITAL_ARC;
  fault |= _STATUS_INTERLOCK_INHIBITING_HV;
  fault |= _FPGA_CURRENT_MONITOR_PULSE_WIDTH_FAULT;
  fault |= _FAULT_OVER_PRF;
  if (fault) {
    return 1;
  } else {
    return 0;
  }
}


void DoA37730(void) {

  unsigned long temp32;
//  TCPmodbus_task(0); //ETHERNET
    
  if (global_data_A37730.trigger_complete) {
    global_data_A37730.tick_period_timer  = ETMTickGet();
    DoPostTriggerProcess();
    global_data_A37730.trigger_complete = 0;
  }
  
  if (ETMTickGreaterThanN100uS(15, global_data_A37730.tick_period_timer)) {    //  1.5 ms delay
       PIN_TRIG_PULSE_WIDTH_LIMITER = !OLL_TRIG_PULSE_DISABLE;  //stop limiting trigger width 
  }
    
#ifdef __CAN_ENABLED
  ETMCanSlaveDoCan();
#endif

#ifndef __CAN_REQUIRED
  ClrWdt();
#endif
  
#ifdef __MODE_MODBUS_MONITOR
  ETMModbusSlaveDoModbus();
#endif

  ETMDigitalUpdateInput(&global_data_A37730.interlock_relay_closed, PIN_INTERLOCK_RELAY_STATUS);
  
#ifdef __MODBUS_CONTROLS

  if (modbus_slave_bit_0x01) {
    global_data_A37730.request_heater_enable = 1;
  } else {
    global_data_A37730.request_heater_enable = 0;
  }
  
  if (modbus_slave_bit_0x02) {
    PIN_HV_ON_SERIAL = OLL_SERIAL_ENABLE;
  } else {
    PIN_HV_ON_SERIAL = !OLL_SERIAL_ENABLE;
  }
  
  if (modbus_slave_bit_0x03) {
    PIN_BEAM_ENABLE_SERIAL = OLL_SERIAL_ENABLE;
  } else {
    PIN_BEAM_ENABLE_SERIAL = !OLL_SERIAL_ENABLE;
  }
#endif
  
  
#ifdef __CAN_CONTROLS
  if (!ETMCanSlaveGetSyncMsgSystemHVDisable()) {
    PIN_HV_ON_SERIAL = OLL_SERIAL_ENABLE;
  } else {
    PIN_HV_ON_SERIAL = !OLL_SERIAL_ENABLE;
  }
  
  if (!ETMCanSlaveGetSyncMsgPulseSyncDisableXray()) {
    PIN_BEAM_ENABLE_SERIAL = OLL_SERIAL_ENABLE; 
  } else {
    PIN_BEAM_ENABLE_SERIAL = !OLL_SERIAL_ENABLE;
  }
#endif
  
  
  
//--------- Following happens every 10ms ------------//  

//  if (_T2IF) {
//    // Run once every 10ms
//    _T2IF = 0;
  
  if (ETMTickRunOnceEveryNMilliseconds(10, &timer_write_holding_var_10ms)) {
      
    unsigned int timer_report;
  
    if (PIN_CUSTOMER_HV_ON == ILL_PIN_CUSTOMER_HV_ON_ENABLE_HV) {
      global_data_A37730.request_hv_enable = 1;
      _STATUS_CUSTOMER_HV_ON = 1;
    } else {
      global_data_A37730.request_hv_enable = 0;
      _STATUS_CUSTOMER_HV_ON = 0;
    }

    if (PIN_CUSTOMER_BEAM_ENABLE == ILL_PIN_CUSTOMER_BEAM_ENABLE_BEAM_ENABLED) {
      global_data_A37730.request_beam_enable = 1;
      _STATUS_CUSTOMER_BEAM_ENABLE = 1;
    } else {
      global_data_A37730.request_beam_enable = 0;
      _STATUS_CUSTOMER_BEAM_ENABLE = 0;
    }
    
    SetStateMessage (global_data_A37730.current_state_msg);
        
        
#ifdef __CAN_CONTROLS
    if (global_data_A37730.control_state != STATE_FAULT_WARMUP_HEATER_OFF) {
      if (ETMCanSlaveGetSyncMsgResetEnable()) {
        global_data_A37730.reset_active = 1;
      } else {
        global_data_A37730.reset_active = 0;
      } 
    } else {
      global_data_A37730.reset_active = 1;
    }  
#endif
 
    
#ifdef __ETHERNET_CONTROLS
   //set Reset bit from ethernet function     
    if (global_data_A37730.control_state != STATE_FAULT_WARMUP_HEATER_OFF) {
      if (global_data_A37730.ethernet_reset_cmd) {
        global_data_A37730.ethernet_reset_cmd = 0;
        global_data_A37730.reset_active = 1;
      } else {
        global_data_A37730.reset_active = 0;
      } 
    } else {
      global_data_A37730.reset_active = 1;
    }  
      
#endif
    
    
#ifdef __DISCRETE_CONTROLS

    unsigned int state_pin_customer_hv_on = PIN_CUSTOMER_HV_ON;
    if (global_data_A37730.control_state != STATE_FAULT_WARMUP_HEATER_OFF) {
      if ((state_pin_customer_hv_on == !ILL_PIN_CUSTOMER_HV_ON_ENABLE_HV) && (global_data_A37730.previous_state_pin_customer_hv_on == ILL_PIN_CUSTOMER_HV_ON_ENABLE_HV)) {
        global_data_A37730.reset_active = 1;
      } else {
        global_data_A37730.reset_active = 0;
      }
      global_data_A37730.previous_state_pin_customer_hv_on = state_pin_customer_hv_on;
    
    } else {
      global_data_A37730.reset_active = 1;
    }  
      
#endif

#ifdef __MODBUS_CONTROLS  

    if (global_data_A37730.control_state != STATE_FAULT_WARMUP_HEATER_OFF) {
      if (GetModbusResetEnable()) {
        global_data_A37730.reset_active = 1;
      } else {
        global_data_A37730.reset_active = 0;
      } 
    } else {
      global_data_A37730.reset_active = 1;
    }  
      
#endif     
    
    
#ifdef __MODBUS_CONTROLS
    ModbusTimer++;
#endif

    
    if (PIN_GH_BIAS_FAULT == ILL_GH_FAULT_ACTIVE) {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_bias_flt, 1);
    } else {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_bias_flt, 0);
    }
    
    if (PIN_GH_HARDWARE_FAULT == ILL_GH_FAULT_ACTIVE) {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_hw_flt, 1);
    } else {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_hw_flt, 0); 
    }
    
    if (PIN_GH_TOP_HTR_OV_FAULT == ILL_GH_FAULT_ACTIVE) {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_top_htr_ov_flt, 1);
    } else {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_top_htr_ov_flt, 0);
    }
    
    if (PIN_GH_TOP_HTR_UV_FAULT == ILL_GH_FAULT_ACTIVE) {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_top_htr_uv_flt, 1);
    } else {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_top_htr_uv_flt, 0);
    }
    
    if (PIN_TEMP_LT_75C == ILL_PIN_TEMP_IS_LT_75C) {
      ETMDigitalUpdateInput(&global_data_A37730.digital_temp_lt_75, 1);
    } else {
      ETMDigitalUpdateInput(&global_data_A37730.digital_temp_lt_75, 0);
    }
    
    if (PIN_ENABLE_RELAY_STATUS == ILL_PIN_RELAY_STATUS_CLOSED) {
      ETMDigitalUpdateInput(&global_data_A37730.enable_relay_closed, 1);
    } else {
      ETMDigitalUpdateInput(&global_data_A37730.gh_digital_top_htr_uv_flt, 0);
    }
    
    if (PIN_INTERLOCK_RELAY_STATUS == ILL_PIN_RELAY_STATUS_CLOSED) {
      ETMDigitalUpdateInput(&global_data_A37730.interlock_relay_closed, 1);
    } else {
      ETMDigitalUpdateInput(&global_data_A37730.interlock_relay_closed, 0);
    }
    
    
    // Update to counter used to flash the LEDs at startup and time transmits to DACs
    if (global_data_A37730.power_supply_startup_remaining) {
      global_data_A37730.power_supply_startup_remaining--;
    }

    if (global_data_A37730.heater_warm_up_time_remaining) {
      global_data_A37730.heater_warm_up_time_remaining--;
    }

    if (global_data_A37730.heater_ramp_up_time) {
      global_data_A37730.heater_ramp_up_time--;
    }

    if (global_data_A37730.fault_restart_remaining) {
      global_data_A37730.fault_restart_remaining--;
    }



//    global_data_A37730.watchdog_counter++;
    global_data_A37730.run_time_counter++;

    if (global_data_A37730.run_time_counter & 0x0010) {
      PIN_LED_OPERATIONAL = 1;
    } else {
      PIN_LED_OPERATIONAL = 0;
    }

    
    temp32 = 1562500;
    temp32 /= global_data_A37730.period_filtered;
    global_data_A37730.rep_rate_deci_hertz = temp32;
    if (_T3IF || (global_data_A37730.rep_rate_deci_hertz < 25)) {
      // We are pulsing at less than 2.5Hz
      // Set the rep rate to zero
      global_data_A37730.rep_rate_deci_hertz = 0;
    } 
    if (_T3IF) {
      //ResetCounter();
      Nop();
    }
    
    if (global_data_A37730.rep_rate_deci_hertz > MAX_PRF_DECI_HZ) {
      global_data_A37730.over_prf++;
    } else if (global_data_A37730.over_prf) {
      global_data_A37730.over_prf--;
    }


//    // Read all the data from the external ADC
//    UpdateADCResults();
//
//    // Start the next acquisition from the external ADC
//    ADCStartAcquisition();
    
    
//    if ((global_data_A37730.previous_0x0A_val != modbus_slave_hold_reg_0x0A) ||
//        (global_data_A37730.previous_0x0B_val != modbus_slave_hold_reg_0x0B) ||
//        (global_data_A37730.previous_0x0C_val != modbus_slave_hold_reg_0x0C) ||
//        (global_data_A37730.previous_0x0D_val != modbus_slave_hold_reg_0x0D)) {
//      SetCustomIP();
//    }
//    
//    global_data_A37730.previous_0x0A_val = modbus_slave_hold_reg_0x0A;
//    global_data_A37730.previous_0x0B_val = modbus_slave_hold_reg_0x0B;
//    global_data_A37730.previous_0x0C_val = modbus_slave_hold_reg_0x0C;
//    global_data_A37730.previous_0x0D_val = modbus_slave_hold_reg_0x0D;
    

    
    // Scale and Calibrate the internal ADC Readings
    ETMAnalogScaleCalibrateADCReading(&global_data_A37730.input_gun_i_peak);
    ETMAnalogScaleCalibrateADCReading(&global_data_A37730.input_hv_i_mon);
    ETMAnalogScaleCalibrateADCReading(&global_data_A37730.input_hv_v_mon);
    ETMAnalogScaleCalibrateADCReading(&global_data_A37730.input_htr_v_mon);
    ETMAnalogScaleCalibrateADCReading(&global_data_A37730.input_htr_i_mon);
    ETMAnalogScaleCalibrateADCReading(&global_data_A37730.input_top_v_mon);
    ETMAnalogScaleCalibrateADCReading(&global_data_A37730.input_bias_v_mon);
    ETMAnalogScaleCalibrateADCReading(&global_data_A37730.input_temperature_mon);

//    ETMCanSlaveSetDebugRegister(0xA, global_data_A37730.pos_5v_mon.reading_scaled_and_calibrated);
//    ETMCanSlaveSetDebugRegister(0xB, global_data_A37730.pos_15v_mon.reading_scaled_and_calibrated);
//    ETMCanSlaveSetDebugRegister(0xC, global_data_A37730.neg_15v_mon.reading_scaled_and_calibrated);
//    ETMCanSlaveSetDebugRegister(0xA, global_data_A37730.run_time_counter);
//    ETMCanSlaveSetDebugRegister(0xB, global_data_A37730.fault_restart_remaining);
//    ETMCanSlaveSetDebugRegister(0xC, global_data_A37730.power_supply_startup_remaining);
    ETMCanSlaveSetDebugRegister(0xD, global_data_A37730.heater_warm_up_time_remaining);
    ETMCanSlaveSetDebugRegister(0xE, global_data_A37730.heater_ramp_up_time);
    ETMCanSlaveSetDebugRegister(0xF, global_data_A37730.control_state);
    
    
    slave_board_data.log_data[0] = global_data_A37730.input_gun_i_peak.reading_scaled_and_calibrated;
    slave_board_data.log_data[1] = global_data_A37730.input_hv_v_mon.reading_scaled_and_calibrated;
    slave_board_data.log_data[2] = global_data_A37730.input_top_v_mon.reading_scaled_and_calibrated;    //gdoc says low energy
    slave_board_data.log_data[3] = global_data_A37730.input_top_v_mon.reading_scaled_and_calibrated;    //gdoc says high energy
    slave_board_data.log_data[4] = global_data_A37730.input_temperature_mon.reading_scaled_and_calibrated;
    slave_board_data.log_data[5] = global_data_A37730.heater_warm_up_time_remaining;
    slave_board_data.log_data[6] = global_data_A37730.input_htr_i_mon.reading_scaled_and_calibrated;
    slave_board_data.log_data[7] = global_data_A37730.input_htr_v_mon.reading_scaled_and_calibrated;
    slave_board_data.log_data[8] = global_data_A37730.analog_output_high_voltage.set_point;
    slave_board_data.log_data[9] = global_data_A37730.heater_voltage_target;
    slave_board_data.log_data[10] = global_data_A37730.analog_output_top_voltage.set_point;       //gdoc says low energy
    slave_board_data.log_data[11] = global_data_A37730.analog_output_top_voltage.set_point;       //gdoc says high energy
    slave_board_data.log_data[12] = global_data_A37730.input_bias_v_mon.reading_scaled_and_calibrated;
    slave_board_data.log_data[13] = global_data_A37730.control_state;
    slave_board_data.log_data[14] = 0;
    slave_board_data.log_data[15] = 0;          //GUN_DRIVER_LOAD_TYPE;

    if (global_data_A37730.control_state == STATE_HEATER_RAMP_UP) {
      timer_report = (global_data_A37730.heater_ramp_up_time + HEATER_WARM_UP_TIME) / 100;
    } else if (global_data_A37730.heater_warm_up_time_remaining > 100) {
      timer_report = global_data_A37730.heater_warm_up_time_remaining / 100;
    } else {
      timer_report = 0;
    }
    
#ifdef __MODE_MODBUS_MONITOR
    modbus_slave_hold_reg_0x21 = global_data_A37730.input_htr_v_mon.reading_scaled_and_calibrated;
    modbus_slave_hold_reg_0x22 = global_data_A37730.input_htr_i_mon.reading_scaled_and_calibrated;
    modbus_slave_hold_reg_0x23 = global_data_A37730.input_top_v_mon.reading_scaled_and_calibrated;
    modbus_slave_hold_reg_0x24 = global_data_A37730.input_hv_v_mon.reading_scaled_and_calibrated;
    modbus_slave_hold_reg_0x25 = global_data_A37730.input_temperature_mon.reading_scaled_and_calibrated / 100;
    modbus_slave_hold_reg_0x26 = global_data_A37730.input_bias_v_mon.reading_scaled_and_calibrated / 10;
    modbus_slave_hold_reg_0x27 = global_data_A37730.input_gun_i_peak.reading_scaled_and_calibrated / 10;
    modbus_slave_hold_reg_0x28 = timer_report;                //global_data_A37730.heater_warm_up_time_remaining;
    
    modbus_slave_hold_reg_0x31 = global_data_A37730.state_message;
    modbus_slave_hold_reg_0x32 = _FAULT_REGISTER;
    modbus_slave_hold_reg_0x33 = _WARNING_REGISTER; 
    
#endif

    ETMCanSlaveSetDebugRegister(7, global_data_A37730.dac_write_failure_count);


#ifdef __CAN_REFERENCE
    ETMAnalogSetOutput(&global_data_A37730.analog_output_high_voltage, global_data_A37730.can_high_voltage_set_point);
    ETMAnalogSetOutput(&global_data_A37730.analog_output_top_voltage, global_data_A37730.can_pulse_top_set_point);
    global_data_A37730.heater_voltage_target                = global_data_A37730.can_heater_voltage_set_point;
    //global_data_A37730.analog_output_high_voltage.set_point = global_data_A37730.can_high_voltage_set_point;
    //global_data_A37730.analog_output_top_voltage.set_point  = global_data_A37730.can_pulse_top_set_point;
#endif


#ifdef __MODBUS_REFERENCE   
    ETMAnalogSetOutput(&global_data_A37730.analog_output_high_voltage, modbus_slave_hold_reg_0x13);
    ETMAnalogSetOutput(&global_data_A37730.analog_output_top_voltage, modbus_slave_hold_reg_0x12);
    global_data_A37730.heater_voltage_target = modbus_slave_hold_reg_0x11;
    
    if (modbus_slave_hold_reg_0x13 < HIGH_VOLTAGE_MIN_SET_POINT || modbus_slave_hold_reg_0x13 > HIGH_VOLTAGE_MAX_SET_POINT) {
        modbus_slave_invalid_data = 1;
    }
    
    if (modbus_slave_hold_reg_0x12 > TOP_VOLTAGE_MAX_SET_POINT) {
        modbus_slave_invalid_data = 1;
    }
    
    if (modbus_slave_hold_reg_0x11 > MAX_PROGRAM_HTR_VOLTAGE) {
        modbus_slave_invalid_data = 1;
    }
    
#endif

#ifdef __ETHERNET_REFERENCE
    
    ETMAnalogSetOutput(&global_data_A37730.analog_output_high_voltage, global_data_A37730.ethernet_hv_ref);
    ETMAnalogSetOutput(&global_data_A37730.analog_output_top_voltage, global_data_A37730.ethernet_top_ref);
    global_data_A37730.heater_voltage_target = global_data_A37730.ethernet_htr_ref;

    
#endif
    
   
    if (global_data_A37730.heater_voltage_target > MAX_PROGRAM_HTR_VOLTAGE) {
      global_data_A37730.heater_voltage_target = MAX_PROGRAM_HTR_VOLTAGE;
    }
    
    
    
    // Ramp the heater voltage
//    if (global_data_A37730.fault_holdoff_state == FAULT_HOLDOFF_STATE) {
//      global_data_A37730.heater_voltage_current_limited = 0;
//    }
//    
//    if (global_data_A37730.control_state == STATE_HEATER_RAMP_UP) {
//      global_data_A37730.heater_ramp_interval++;
//      if (global_data_A37730.heater_ramp_interval >= HEATER_RAMP_UP_TIME_PERIOD) {
//        global_data_A37730.heater_ramp_interval = 0;
//        global_data_A37730.analog_output_heater_voltage.set_point += HEATER_RAMP_UP_INCREMENT;  
//      }  
//        
//    } else if (global_data_A37730.control_state > STATE_HEATER_RAMP_UP) {
//      global_data_A37730.heater_ramp_interval++;
//      if (global_data_A37730.heater_ramp_interval >= HEATER_REGULATION_TIME_PERIOD) {
//        global_data_A37730.heater_ramp_interval = 0;
//        if (global_data_A37730.input_htr_i_mon.reading_scaled_and_calibrated < MAX_HEATER_CURRENT_DURING_RAMP_UP) {
//          global_data_A37730.analog_output_heater_voltage.set_point += HEATER_REGULATION_INCREMENT;
//          if (global_data_A37730.heater_voltage_current_limited) {
//            global_data_A37730.heater_voltage_current_limited--;
//          }
//        } else {
//          global_data_A37730.heater_voltage_current_limited++;
//          if (global_data_A37730.heater_voltage_current_limited > HEATER_VOLTAGE_CURRENT_LIMITED_FAULT_TIME) {
//            global_data_A37730.heater_voltage_current_limited = HEATER_VOLTAGE_CURRENT_LIMITED_FAULT_TIME;
//          }
//        }
//      }
//    }
    
    if ((global_data_A37730.control_state == STATE_HEATER_RAMP_UP)||(global_data_A37730.fault_holdoff_state == FAULT_HOLDOFF_STATE)) {
      global_data_A37730.heater_voltage_current_limited = 0;
    }
    global_data_A37730.heater_ramp_interval++;
    
    if (global_data_A37730.heater_ramp_interval >= HEATER_REGULATION_TIME_PERIOD) {
      global_data_A37730.heater_ramp_interval = 0;
      if (global_data_A37730.input_htr_i_mon.reading_scaled_and_calibrated < MAX_HEATER_CURRENT_DURING_RAMP_UP) {
        global_data_A37730.analog_output_heater_voltage.set_point += HEATER_REGULATION_INCREMENT;
        if (global_data_A37730.heater_voltage_current_limited) {
          global_data_A37730.heater_voltage_current_limited--;
        }
      } else {
        global_data_A37730.heater_voltage_current_limited++;
        if (global_data_A37730.heater_voltage_current_limited > HEATER_VOLTAGE_CURRENT_LIMITED_FAULT_TIME) {
          global_data_A37730.heater_voltage_current_limited = HEATER_VOLTAGE_CURRENT_LIMITED_FAULT_TIME;
        }
      }
    }  
    
    if (global_data_A37730.analog_output_heater_voltage.set_point > global_data_A37730.heater_voltage_target) {
      global_data_A37730.analog_output_heater_voltage.set_point = global_data_A37730.heater_voltage_target;
    }

    // update the DAC programs based on the new set points.
    ETMAnalogScaleCalibrateDACSetting(&global_data_A37730.analog_output_high_voltage);
    ETMAnalogScaleCalibrateDACSetting(&global_data_A37730.analog_output_top_voltage);
    ETMAnalogScaleCalibrateDACSetting(&global_data_A37730.analog_output_heater_voltage);
    

    // Send out Data to local DAC and offboard.  Each channel will be updated once every 40mS
    // Do not send out while in state "STATE_WAIT_FOR_CONFIG" because the module is not ready to receive data and
    // you will just get data transfer errors
    if (global_data_A37730.control_state != STATE_WAIT_FOR_CONFIG) {
      switch ((global_data_A37730.run_time_counter & 0b111)) {
	
      case 0:
        WriteLTC265X(&U29_LTC2654, LTC265X_WRITE_AND_UPDATE_DAC_A, global_data_A37730.analog_output_high_voltage.dac_setting_scaled_and_calibrated);
        ETMCanSlaveSetDebugRegister(0, global_data_A37730.analog_output_high_voltage.dac_setting_scaled_and_calibrated);
        break;
	

      case 1:
        WriteLTC265X(&U29_LTC2654, LTC265X_WRITE_AND_UPDATE_DAC_B, global_data_A37730.analog_output_top_voltage.dac_setting_scaled_and_calibrated);
        ETMCanSlaveSetDebugRegister(1, global_data_A37730.analog_output_top_voltage.dac_setting_scaled_and_calibrated);
        break;

    
      case 2:
        WriteLTC265X(&U29_LTC2654, LTC265X_WRITE_AND_UPDATE_DAC_C, global_data_A37730.analog_output_heater_voltage.dac_setting_scaled_and_calibrated);
        ETMCanSlaveSetDebugRegister(2, global_data_A37730.analog_output_heater_voltage.dac_setting_scaled_and_calibrated);
        break;

      
      case 3:
        ETMCanSlaveSetDebugRegister(3, global_data_A37730.dac_digital_hv_enable);
        break;


      case 4:
        ETMCanSlaveSetDebugRegister(4, global_data_A37730.dac_digital_heater_enable);
        break;

      
      case 5:
        ETMCanSlaveSetDebugRegister(5, global_data_A37730.dac_digital_top_enable);
        break;

    
      case 6:
        ETMCanSlaveSetDebugRegister(6, global_data_A37730.dac_digital_trigger_enable);
        break;
    
  
      case 7:

        break;
      }
    }
  
    // Update Faults
    UpdateFaults();
    
    // Mange LED and Status Outputs
    UpdateLEDandStatusOutuputs();
  }
}

void DoPostTriggerProcess(void) {
  if (PIN_TRIG_PULSE_INTERRUPT) {  //check if trigger still active after limit applied
      global_data_A37730.limiting_active = 1;
  }
  
  global_data_A37730.period_filtered = RCFilterNTau(global_data_A37730.period_filtered, global_data_A37730.last_period, RC_FILTER_4_TAU);
    
//  if (ETMCanSlaveGetSyncMsgHighSpeedLogging()) {
//    // Log Pulse by Pulse data
//    ETMCanSlaveLogPulseData(ETM_CAN_DATA_LOG_REGISTER_PULSE_SYNC_FAST_LOG_0,
//			    psb_data.pulses_on,
//			    *(unsigned int*)&trigger_width,
//			    *(unsigned int*)&data_grid_start,
//			    log_data_rep_rate_deci_hertz);
//  }
  
  global_data_A37730.pulses_on++;       // This counts the pulses
//  ETMCanSlavePulseSyncSendNextPulseLevel(psb_data.energy, psb_data.pulses_on, log_data_rep_rate_deci_hertz);
}

  

void UpdateFaults(void) {
  
//  if ((global_data_A37730.control_state == STATE_FAULT_HEATER_FAILURE) ||
//      (global_data_A37730.control_state == STATE_FAULT_WARMUP_HEATER_OFF) ||
//      (global_data_A37730.control_state == STATE_FAULT_HEATER_OFF) ||
//      (global_data_A37730.control_state == STATE_FAULT_HEATER_ON) ||
//      (global_data_A37730.control_state == STATE_WAIT_FOR_CONFIG)) {
//    // Do not evalute any more fault conditions
//    return;
//  }
  
  if (global_data_A37730.control_state < STATE_HEATER_RAMP_UP) {
    // Do not evalute any more fault conditions
    return;
  }
  
   
  if (global_data_A37730.heater_voltage_current_limited >= HEATER_VOLTAGE_CURRENT_LIMITED_FAULT_TIME) {
    _FAULT_HEATER_VOLTAGE_CURRENT_LIMITED = 1;
  } else if (global_data_A37730.reset_active) {
    _FAULT_HEATER_VOLTAGE_CURRENT_LIMITED = 0;
  }
 
  // Evaluate the readings from the pins

    
//    if (global_data_A37730.adc_digital_arc_flt.filtered_reading == 0) {
//      _FAULT_ADC_DIGITAL_ARC = 1;
//    } else if (global_data_A37730.reset_active) {
//      _FAULT_ADC_DIGITAL_ARC = 0;
//    }
    
    if (global_data_A37730.digital_temp_lt_75.filtered_reading == 0) {
      _FAULT_DIGITAL_OVER_TEMP = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_DIGITAL_OVER_TEMP = 0;
    }
  
    if (global_data_A37730.gh_digital_bias_flt.filtered_reading == 1) {
      _FAULT_DIGITAL_BIAS = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_DIGITAL_BIAS = 0;
    }

    if (global_data_A37730.gh_digital_hw_flt.filtered_reading == 1) {
      _FAULT_GRID_HEATER_HARDWARE = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_GRID_HEATER_HARDWARE = 0;
    }
  
    if (global_data_A37730.gh_digital_top_htr_ov_flt.filtered_reading == 1) {
      _FAULT_DIGITAL_TOP_HEATER_OV = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_DIGITAL_TOP_HEATER_OV = 0;
    }
  
    if (global_data_A37730.gh_digital_top_htr_uv_flt.filtered_reading == 1) {
      _FAULT_DIGITAL_TOP_HEATER_UV = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_DIGITAL_TOP_HEATER_UV = 0;
    }

    if (global_data_A37730.enable_relay_closed.filtered_reading == 1) {
      _STATUS_ENABLE_RELAY_CLOSED = 1;
    } else {
      _STATUS_ENABLE_RELAY_CLOSED = 0;
    }
  

  
    // ------------------- Evaluate the analog readings from the Coverter Logic Board ADC ---------------------//
    global_data_A37730.input_htr_v_mon.target_value = global_data_A37730.analog_output_heater_voltage.set_point;
    global_data_A37730.input_hv_v_mon.target_value = global_data_A37730.analog_output_high_voltage.set_point;
    global_data_A37730.input_top_v_mon.target_value = global_data_A37730.analog_output_top_voltage.set_point;

    // If the set point is less that 1.5 V clear the under current counter
    if (global_data_A37730.analog_output_heater_voltage.set_point < 1500) {
      global_data_A37730.input_htr_v_mon.absolute_under_counter = 0;
    }
 

    // If the high voltage is not on, clear the high voltage, top, and bias error counters
//    if (global_data_A37730.control_state < STATE_HV_ON) {
//      ETMAnalogClearFaultCounters(&global_data_A37730.input_hv_v_mon);
//      ETMAnalogClearFaultCounters(&global_data_A37730.input_top_v_mon);
//      ETMAnalogClearFaultCounters(&global_data_A37730.input_bias_v_mon);
//    }

    
    if (ETMAnalogCheckOverAbsolute(&global_data_A37730.input_htr_i_mon)) {
      _FAULT_ADC_HTR_I_MON_OVER_ABSOLUTE = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_HTR_I_MON_OVER_ABSOLUTE = 0;
    }

    // Only check for heater under current after the ramp up process is complete
    if (global_data_A37730.control_state > STATE_HEATER_RAMP_UP) {
      if (ETMAnalogCheckUnderAbsolute(&global_data_A37730.input_htr_i_mon)) {
        _FAULT_ADC_HTR_I_MON_UNDER_ABSOLUTE = 1;
      }
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_HTR_I_MON_UNDER_ABSOLUTE = 0;  
    }  

    if (ETMAnalogCheckOverRelative(&global_data_A37730.input_htr_v_mon)) {
      _FAULT_ADC_HTR_V_MON_OVER_RELATIVE = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_HTR_V_MON_OVER_RELATIVE = 0;
    }
      
    if (ETMAnalogCheckUnderRelative(&global_data_A37730.input_htr_v_mon)) {
      _FAULT_ADC_HTR_V_MON_UNDER_RELATIVE = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_HTR_V_MON_UNDER_RELATIVE = 0;
    }

    if (global_data_A37730.control_state >= STATE_POWER_SUPPLY_RAMP_UP) {
      if (ETMAnalogCheckOverRelative(&global_data_A37730.input_hv_v_mon)) {
        _FAULT_ADC_HV_V_MON_OVER_RELATIVE = 1;
      }
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_HV_V_MON_OVER_RELATIVE = 0;
    }
    
    if (global_data_A37730.control_state >= STATE_POWER_SUPPLY_RAMP_UP) {
      if (global_data_A37730.interlock_relay_closed.filtered_reading == 0) {
        _STATUS_INTERLOCK_INHIBITING_HV = 1;
      }
    } else if (global_data_A37730.reset_active) {
      _STATUS_INTERLOCK_INHIBITING_HV = 0;
    }
    

    // Only check for HV undervoltage after HV is enabled
    if (global_data_A37730.control_state >= STATE_HV_ON) {
      if (ETMAnalogCheckUnderRelative(&global_data_A37730.input_hv_v_mon)) {
        _FAULT_ADC_HV_V_MON_UNDER_RELATIVE = 1;
      }
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_HV_V_MON_UNDER_RELATIVE = 0;
    }
    
    // Only check for top supply overvoltage after top is enabled
    if (global_data_A37730.control_state >= STATE_TOP_READY) {
      if (ETMAnalogCheckOverRelative(&global_data_A37730.input_top_v_mon)) {
        _FAULT_ADC_TOP_V_MON_OVER_RELATIVE = 1;
      }
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_TOP_V_MON_OVER_RELATIVE = 0;
    }
    
    // Only check for top supply undervoltage after top is enabled
    if (global_data_A37730.control_state >= STATE_TOP_READY) {
      if (ETMAnalogCheckUnderRelative(&global_data_A37730.input_top_v_mon)) {
        _FAULT_ADC_TOP_V_MON_UNDER_RELATIVE = 1;
      }
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_TOP_V_MON_UNDER_RELATIVE = 0;
    }

    if (ETMAnalogCheckOverAbsolute(&global_data_A37730.input_bias_v_mon)) {
      _FAULT_ADC_BIAS_V_MON_OVER_ABSOLUTE = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_BIAS_V_MON_OVER_ABSOLUTE = 0;
    }
    
    if (ETMAnalogCheckUnderAbsolute(&global_data_A37730.input_bias_v_mon)) {
      _FAULT_ADC_BIAS_V_MON_UNDER_ABSOLUTE = 1;
    } else if (global_data_A37730.reset_active) {
      _FAULT_ADC_BIAS_V_MON_UNDER_ABSOLUTE = 0;
    }
      
    if (global_data_A37730.over_prf >= OVER_PRF_COUNT) {
      _FAULT_OVER_PRF = 1;
      global_data_A37730.over_prf = 0;
    } else if (global_data_A37730.reset_active) {
      _FAULT_OVER_PRF = 0;
    }

  
  if ((global_data_A37730.heater_ramp_up_time == 0) && (global_data_A37730.control_state == STATE_HEATER_RAMP_UP)) {
    _FAULT_HEATER_RAMP_TIMEOUT = 1;
  } else if (global_data_A37730.reset_active) {
    _FAULT_HEATER_RAMP_TIMEOUT = 0;
  }
}


void UpdateLEDandStatusOutuputs(void) {
  // System OK Status
//  if (global_data_A37730.control_state <= STATE_FAULT_HEATER_ON) {
  if (_FAULT_REGISTER != 0) {
    PIN_LED_FAULT_STATE = OLL_LED_ON;
  } else {
    PIN_LED_FAULT_STATE = !OLL_LED_ON;
  }
}


void SetStateMessage (unsigned int message) {
  
  if (global_data_A37730.request_hv_enable) {
    message |= 0x0010;
  } else {
    message &= ~0x0010;
  }

  if (global_data_A37730.request_beam_enable) {
    message |= 0x0040;
  } else {
    message &= ~0x0040;
  }
  
  if (_WARNING_REGISTER) {
    message |= 0x0200;
  } else {
    message &= ~0x0200;
  } 
  
  global_data_A37730.state_message = message;
  
}
   
unsigned int GetModbusResetEnable(void) {
  if (modbus_slave_bit_0x04) {
    modbus_slave_bit_0x04 = 0;  
    return 1;
  } else {
    return 0;
  }
}

void EnableHeater(void) {
  /* 
     Set the heater ref
     Set the heater enable control voltage
  */
  global_data_A37730.analog_output_heater_voltage.enabled = 1;
  PIN_CPU_HTR_ENABLE = OLL_CPU_ENABLE;
}


void DisableHeater(void) {
  /* 
     Set the heater ref to zero
     Clear the heater enable control voltage
   */
  global_data_A37730.analog_output_heater_voltage.enabled = 0;
  PIN_CPU_HTR_ENABLE = !OLL_CPU_ENABLE;
}


void EnableHighVoltage(void) {
  /*
    Set the HVPS reference
    Set the HVPS enable control voltage
  */
  global_data_A37730.analog_output_high_voltage.enabled = 1;
  PIN_CPU_HV_ENABLE = OLL_CPU_ENABLE;
}

void EnableTopSupply(void)  {
  /*
     Set the grid top reference
     Set the grid top enable control voltage
   */
  global_data_A37730.analog_output_top_voltage.enabled = 1;
  PIN_CPU_TOP_ENABLE = OLL_CPU_ENABLE;
}


void DisableHighVoltage(void) {
  /*
    Set the HVPS reference to zero
    Set the grid top reference to zero 
    Clear the HVPS enable control voltage
    Clear the grid top enable control voltage
  */
  global_data_A37730.analog_output_top_voltage.enabled = 0;
  global_data_A37730.analog_output_high_voltage.enabled = 0;
  PIN_CPU_TOP_ENABLE = !OLL_CPU_ENABLE;
  PIN_CPU_HV_ENABLE = !OLL_CPU_ENABLE;
}


void EnableBeam(void) {
  PIN_CPU_BEAM_ENABLE = OLL_CPU_ENABLE;
}


void DisableBeam(void) {
  PIN_CPU_BEAM_ENABLE = !OLL_CPU_ENABLE;
}



//typedef struct {
//  unsigned fpga_firmware_rev:8;
//  unsigned unused_bits:2;
//  unsigned customer_hardware_rev:6;
//  unsigned arc:1;
//  unsigned arc_high_voltage_inihibit_active:1;
//  unsigned heater_voltage_less_than_4_5_volts:1;
//  unsigned module_temp_greater_than_65_C:1;
//  unsigned module_temp_greater_than_75_C:1;
//  unsigned pulse_width_limiting_active:1;
//  unsigned prf_fault:1;
//  unsigned current_monitor_pulse_width_fault:1;
//  unsigned grid_module_hardware_fault:1;
//  unsigned grid_module_over_voltage_fault:1;
//  unsigned grid_module_under_voltage_fault:1;
//  unsigned grid_module_bias_voltage_fault:1;
//  unsigned hv_regulation_warning:1;
//  unsigned dipswitch_1_on:1;
//  unsigned test_mode_toggle_switch_set_to_test:1;
//  unsigned local_mode_toggle_switch_set_to_local:1;
//} TYPE_FPGA_DATA;

//
////////    
//    // Check the Arc Count (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_arc, fpga_bits.arc); 
//    if (global_data_A37730.fpga_arc.filtered_reading) {
//      _FPGA_ARC_COUNTER_GREATER_ZERO = 1;
//    } else {
//      _FPGA_ARC_COUNTER_GREATER_ZERO = 0;
//    }
//    
//    // Check Arc High Voltage Inhibit Active (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_arc_high_voltage_inihibit_active, fpga_bits.arc_high_voltage_inihibit_active); 
//    if (global_data_A37730.fpga_arc_high_voltage_inihibit_active.filtered_reading) {
//      _FPGA_ARC_HIGH_VOLTAGE_INHIBIT_ACTIVE = 1;
//    } else {
//      _FPGA_ARC_HIGH_VOLTAGE_INHIBIT_ACTIVE = 0;
//    }
//
//    // Check the heater voltage less than 4.5 Volts (NOT LATCHED)
////    ETMDigitalUpdateInput(&global_data_A37730.fpga_heater_voltage_less_than_4_5_volts, fpga_bits.heater_voltage_less_than_4_5_volts);
////    if (global_data_A37730.fpga_heater_voltage_less_than_4_5_volts.filtered_reading) {
////      _FPGA_HEATER_VOLTAGE_LESS_THAN_4_5_VOLTS = 1;
////    } else {
////      _FPGA_HEATER_VOLTAGE_LESS_THAN_4_5_VOLTS = 0;
////    }
//
//    // Check module temp greater than 65 C (NOT LATCHED)
////    ETMDigitalUpdateInput(&global_data_A37730.fpga_module_temp_greater_than_65_C, fpga_bits.module_temp_greater_than_65_C);
////    if (global_data_A37730.fpga_module_temp_greater_than_65_C.filtered_reading) {
////      _FPGA_MODULE_TEMP_GREATER_THAN_65_C = 1;
////    } else {
////      _FPGA_MODULE_TEMP_GREATER_THAN_65_C = 0;
////    }
//
//    // Check module temp greater than 75 C (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_module_temp_greater_than_75_C, fpga_bits.module_temp_greater_than_75_C);
//    if (global_data_A37730.fpga_module_temp_greater_than_75_C.filtered_reading) {
//      _FPGA_MODULE_TEMP_GREATER_THAN_75_C = 1;
//    } else {
//      _FPGA_MODULE_TEMP_GREATER_THAN_75_C = 0;
//    }
//    
//    // Check Pulse Width Limiting (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_pulse_width_limiting_active, fpga_bits.pulse_width_limiting_active);
//    if (global_data_A37730.fpga_pulse_width_limiting_active.filtered_reading) {
//      _FPGA_PULSE_WIDTH_LIMITING = 1;
//    } else {
//      _FPGA_PULSE_WIDTH_LIMITING = 0;
//    }
//    
    // Check prf fault (LATCHED)
//    if (global_data_A37730.fpga_prf_fault.filtered_reading) {
//      _FAULT_OVER_PRF = 1;
//    } else if (global_data_A37730.reset_active) {
//      _FAULT_OVER_PRF = 0;
//    }
//
//    // Check Current Monitor Pulse Width Fault (LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_current_monitor_pulse_width_fault, fpga_bits.current_monitor_pulse_width_fault);
//    if (global_data_A37730.fpga_current_monitor_pulse_width_fault.filtered_reading) {
//      _FPGA_CURRENT_MONITOR_PULSE_WIDTH_FAULT = 1;
//    } else if (global_data_A37730.reset_active) {
//      _FPGA_CURRENT_MONITOR_PULSE_WIDTH_FAULT = 0;
//    }
//
//    // Check grid module hardware fault (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_grid_module_hardware_fault, fpga_bits.grid_module_hardware_fault);
//    if (global_data_A37730.fpga_grid_module_hardware_fault.filtered_reading) {
//      _FPGA_GRID_MODULE_HARDWARE_FAULT = 1;
//    } else {
//      _FPGA_GRID_MODULE_HARDWARE_FAULT = 0;
//    }
//
//    // Check grid module over voltage (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_grid_module_over_voltage_fault, fpga_bits.grid_module_over_voltage_fault);
//    if (global_data_A37730.fpga_grid_module_over_voltage_fault.filtered_reading) {
//      _FPGA_GRID_MODULE_OVER_VOLTAGE_FAULT = 1;
//    } else {
//      _FPGA_GRID_MODULE_OVER_VOLTAGE_FAULT = 0;
//    }
//
//    // Check grid module under voltage (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_grid_module_under_voltage_fault, fpga_bits.grid_module_under_voltage_fault);
//    if (global_data_A37730.fpga_grid_module_under_voltage_fault.filtered_reading) {
//      _FPGA_GRID_MODULE_UNDER_VOLTAGE_FAULT = 1;
//    } else {
//      _FPGA_GRID_MODULE_UNDER_VOLTAGE_FAULT = 0;
//    }
//
//    // Check grid module bias voltage (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_grid_module_bias_voltage_fault, fpga_bits.grid_module_bias_voltage_fault);
//    if (global_data_A37730.fpga_grid_module_bias_voltage_fault.filtered_reading) {
//      _FPGA_GRID_MODULE_BIAS_VOLTAGE_FAULT = 1;
//    } else {
//      _FPGA_GRID_MODULE_BIAS_VOLTAGE_FAULT = 0;
//    }
//
//    // High Voltage regulation Warning (NOT LATCHED)
//    ETMDigitalUpdateInput(&global_data_A37730.fpga_hv_regulation_warning, fpga_bits.hv_regulation_warning);
//    if (global_data_A37730.fpga_hv_regulation_warning.filtered_reading) {
//      _FPGA_HV_REGULATION_WARNING = 1;
//    } else {
//      _FPGA_HV_REGULATION_WARNING = 0;
//    }



void ETMDigitalInitializeInput(TYPE_DIGITAL_INPUT* input, unsigned int initial_value, unsigned int filter_time) {
  if (filter_time > 0x7000) {
    filter_time = 0x7000;
  }
  input->filter_time = filter_time;
  if (initial_value == 0) {
    input->accumulator = 0;
    input->filtered_reading = 0;
  } else {
    input->accumulator = (filter_time << 1);
    input->filtered_reading = 1;
  }
}


void ETMDigitalUpdateInput(TYPE_DIGITAL_INPUT* input, unsigned int current_value) {
  if (input->filter_time < 2) {
    input->filtered_reading = current_value;
  } else {
    if (current_value) {
      if (++input->accumulator > (input->filter_time << 1)) {
	input->accumulator--;
      }
    } else {
      if (input->accumulator) {
	input->accumulator--;
      }
    }
    if (input->accumulator >= input->filter_time) {
      if (input->filtered_reading == 0) {
	// we are changing state from low to high
	input->accumulator = (input->filter_time << 1);
      }
      input->filtered_reading = 1;
    } else {
      if (input->filtered_reading == 1) {
	// we are changing state from high to low
	input->accumulator = 0;
      }
      input->filtered_reading = 0;
    }
  }
}


void __attribute__((interrupt, no_auto_psv)) _ADCInterrupt(void) {
  _ADIF = 0;
  
  // Copy Data From Buffer to RAM
  if (_BUFS) {
    // read ADCBUF 0-7
    global_data_A37730.input_gun_i_peak.adc_accumulator        += ADCBUF0;
    global_data_A37730.input_hv_i_mon.adc_accumulator          += ADCBUF1;
    global_data_A37730.input_hv_v_mon.adc_accumulator          += ADCBUF2;
    global_data_A37730.input_htr_v_mon.adc_accumulator         += ADCBUF3;
    
    global_data_A37730.input_htr_i_mon.adc_accumulator         += ADCBUF4;
    global_data_A37730.input_top_v_mon.adc_accumulator         += ADCBUF5;
    global_data_A37730.input_bias_v_mon.adc_accumulator        += ADCBUF6;
    global_data_A37730.input_temperature_mon.adc_accumulator   += ADCBUF7;
    
  } else {
    // read ADCBUF 8-F
    global_data_A37730.input_gun_i_peak.adc_accumulator        += ADCBUF8;
    global_data_A37730.input_hv_i_mon.adc_accumulator          += ADCBUF9;
    global_data_A37730.input_hv_v_mon.adc_accumulator          += ADCBUFA;
    global_data_A37730.input_htr_v_mon.adc_accumulator         += ADCBUFB;
    
    global_data_A37730.input_htr_i_mon.adc_accumulator         += ADCBUFC;
    global_data_A37730.input_top_v_mon.adc_accumulator         += ADCBUFD;
    global_data_A37730.input_bias_v_mon.adc_accumulator        += ADCBUFE;
    global_data_A37730.input_temperature_mon.adc_accumulator   += ADCBUFF;
    
    
  }
  
  global_data_A37730.accumulator_counter += 1;
  
  if (global_data_A37730.accumulator_counter >= 128) {
    global_data_A37730.accumulator_counter = 0;    

    // average the 128 12 bit samples into a single 16 bit sample
    global_data_A37730.input_gun_i_peak.adc_accumulator   >>= 3; // This is now a 16 bit number average of previous 128 samples
    global_data_A37730.input_hv_i_mon.adc_accumulator  >>= 3; 
    global_data_A37730.input_hv_v_mon.adc_accumulator  >>= 3;
    global_data_A37730.input_htr_v_mon.adc_accumulator  >>= 3;
    global_data_A37730.input_htr_i_mon.adc_accumulator  >>= 3;
    global_data_A37730.input_top_v_mon.adc_accumulator  >>= 3;
    global_data_A37730.input_bias_v_mon.adc_accumulator  >>= 3;
    global_data_A37730.input_temperature_mon.adc_accumulator  >>= 3;
    

    // Store the filtred results
    global_data_A37730.input_gun_i_peak.filtered_adc_reading = global_data_A37730.input_gun_i_peak.adc_accumulator;
    global_data_A37730.input_hv_i_mon.filtered_adc_reading = global_data_A37730.input_hv_i_mon.adc_accumulator;
    global_data_A37730.input_hv_v_mon.filtered_adc_reading = global_data_A37730.input_hv_v_mon.adc_accumulator;
    global_data_A37730.input_htr_v_mon.filtered_adc_reading = global_data_A37730.input_htr_v_mon.adc_accumulator;
    global_data_A37730.input_htr_i_mon.filtered_adc_reading = global_data_A37730.input_htr_i_mon.adc_accumulator;
    global_data_A37730.input_top_v_mon.filtered_adc_reading = global_data_A37730.input_top_v_mon.adc_accumulator;
    global_data_A37730.input_bias_v_mon.filtered_adc_reading = global_data_A37730.input_bias_v_mon.adc_accumulator;
    global_data_A37730.input_temperature_mon.filtered_adc_reading = global_data_A37730.input_temperature_mon.adc_accumulator;
    
    // clear the accumulators 
    global_data_A37730.input_gun_i_peak.adc_accumulator  = 0; 
    global_data_A37730.input_hv_i_mon.adc_accumulator  = 0; 
    global_data_A37730.input_hv_v_mon.adc_accumulator  = 0;
    global_data_A37730.input_htr_v_mon.adc_accumulator  = 0;
    global_data_A37730.input_htr_i_mon.adc_accumulator  = 0;
    global_data_A37730.input_top_v_mon.adc_accumulator  = 0;
    global_data_A37730.input_bias_v_mon.adc_accumulator  = 0;
    global_data_A37730.input_temperature_mon.adc_accumulator  = 0;
  }
}


/////////////////////////////// 

#define MIN_PERIOD 150 // 960uS 1041 Hz// 
void __attribute__((interrupt(__save__(CORCON,SR)), no_auto_psv)) _INT4Interrupt(void) {
  // A trigger was received.
  // THIS DOES NOT MEAN THAT A PULSE WAS GENERATED
  __delay32(64); //6.4us
  PIN_TRIG_PULSE_WIDTH_LIMITER = OLL_TRIG_PULSE_DISABLE;  //limit trigger width
  
  if ((TMR3 > MIN_PERIOD) || _T3IF) {
    // Calculate the Trigger PRF
    // TMR3 is used to time the time between INT4 interrupts
    global_data_A37730.last_period = TMR3;
    TMR3 = 0;
    if (_T3IF) {
      // The timer exceed it's period of 400mS - (Will happen if the PRF is less than 2.5Hz)
      global_data_A37730.last_period = 62501;  // This will indicate that the PRF is Less than 2.5Hz
    }

    _T3IF = 0;
    
    global_data_A37730.trigger_complete = 1;
  }
  _INT4IF = 0;		// Clear Interrupt flag
}  

//void ETMAnalogClearFaultCounters(AnalogInput* ptr_analog_input) {
//  ptr_analog_input->absolute_under_counter = 0;
//  ptr_analog_input->absolute_over_counter = 0;
//  ptr_analog_input->over_trip_counter = 0;
//  ptr_analog_input->under_trip_counter = 0;
//}



void ETMCanSlaveExecuteCMDBoardSpecific(ETMCanMessage* message_ptr) {
  unsigned int index_word;

  index_word = message_ptr->word3;
  switch (index_word) {

    case ETM_CAN_REGISTER_GUN_DRIVER_SET_1_GRID_TOP_SET_POINT:
      global_data_A37730.can_pulse_top_set_point = message_ptr->word1;
      global_data_A37730.control_config |= 1;
      if (global_data_A37730.control_config == 3){
      _CONTROL_NOT_CONFIGURED = 0;
      }
      break;

    case ETM_CAN_REGISTER_GUN_DRIVER_SET_1_HEATER_CATHODE_SET_POINT:
      global_data_A37730.can_high_voltage_set_point = message_ptr->word1;
      global_data_A37730.can_heater_voltage_set_point = message_ptr->word0;

      global_data_A37730.control_config |= 2;
      if (global_data_A37730.control_config == 3){
      _CONTROL_NOT_CONFIGURED = 0;
      }
      break;

    default:
//      local_can_errors.invalid_index++;
      break;
      
    }

}

#ifdef __noModbusLibrary

void ETMModbusInit(void) {
	  // Initialize application specific hardware
  UART1TX_ON_TRIS = 0;
  UART1TX_ON_IO = 1;    // always enable TX1

    // Configure UART Interrupts
  _U1RXIE = 0;
  _U1RXIP = 5;
  
  _U1TXIE = 0;
  _U1TXIP = 5;

#if 0  // T1 is used by Ethernet tick    
          // Initialize TMR1
  PR1   = A37730_PR1_VALUE;
  TMR1  = 0;
  _T1IF = 0;
  _T1IP = 2;
  T1CON = A37730_T1CON_VALUE;
#endif  

  // ----------------- UART #1 Setup and Data Buffer -------------------------//
  // Setup the UART input and output buffers
  uart1_input_buffer.write_location = 0;  
  uart1_input_buffer.read_location = 0;
  uart1_output_buffer.write_location = 0;
  uart1_output_buffer.read_location = 0;
  
  U1MODE = MODBUS_U1MODE_VALUE;
  U1BRG = MODBUS_U1BRG_VALUE;
  U1STA = MODBUS_U1STA_VALUE;
  
  _U1TXIF = 0;	// Clear the Transmit Interrupt Flag
  _U1TXIE = 1;	// Enable Transmit Interrupts
  _U1RXIF = 0;	// Clear the Recieve Interrupt Flag
  _U1RXIE = 1;	// Enable Recieve Interrupts
  
  //Load startup values from EEPROM
  int i;
  
  for (i=0; i<SLAVE_HOLD_REG_ARRAY_SIZE; i++) {
    ModbusSlaveHoldingRegister[i] = ETMEEPromReadWord(0x600 + i);
  }
  for (i=0; i<SLAVE_BIT_ARRAY_SIZE; i++) {
    ModbusSlaveBit[i] = ETMEEPromReadWord(0x640 + i);
  }
  
  //Initialize control bits as disabled
  modbus_slave_bit_0x01 = 0;
  modbus_slave_bit_0x02 = 0;
  modbus_slave_bit_0x03 = 0;
  modbus_slave_bit_0x04 = 0;
//  modbus_slave_bit_0x05 = 0;
//  modbus_slave_bit_0x06 = 0;
//  modbus_slave_bit_0x07 = 0;
  
  U1MODEbits.UARTEN = 1;	// And turn the peripheral on
  
  modbus_transmission_needed = 0;
  modbus_receiving_flag = 0;
  ETM_last_modbus_fail = 0;
  
  modbus_slave_invalid_data = 0;
  
  ModbusTimer = 0;
  ModbusTest = 0;
  PIN_RS485_ENABLE = 0;
  
  ETM_modbus_state = MODBUS_STATE_IDLE;
}



void ETMModbusSlaveDoModbus(void) {
  if (!modbus_transmission_needed) {
    if (LookForMessage()) {
      //Execute command with following functions
      PIN_RS485_ENABLE = 1;
      ReceiveCommand(&current_command_ptr);
      ProcessCommand(&current_command_ptr);
      SendResponse(&current_command_ptr);
      modbus_transmission_needed = 1;
//      while ((!U1STAbits.UTXBF) && (BufferByte64BytesInBuffer(&uart1_output_buffer))) {
//          U1TXREG = BufferByte64ReadByte(&uart1_output_buffer);
//      }
      if (!U1STAbits.UTXBF) {
        U1TXREG = BufferByte64ReadByte(&uart1_output_buffer);
      }
    }
  } else if ((U1STAbits.TRMT == 1) && (!BufferByte64BytesInBuffer(&uart1_output_buffer))) {
    PIN_RS485_ENABLE = 0;
    modbus_transmission_needed = 0;
  }   
}


unsigned int LookForMessage (void) {
    
  unsigned int crc, crc_in, i;
  unsigned char address;
  
  while (BufferByte64BytesInBuffer(&uart1_input_buffer) >= ETMMODBUS_COMMAND_SIZE_MIN) {
    address = BufferByte64ReadByte(&uart1_input_buffer);
    if (address == MODBUS_SLAVE_ADDR) {
        modbus_cmd_byte[0] = MODBUS_SLAVE_ADDR;
        for (i=1; i<8; i++) {
          modbus_cmd_byte[i] = uart1_input_buffer.data[(uart1_input_buffer.read_location + (i-1)) & 0x3F];
        }
        crc_in = (modbus_cmd_byte[7] << 8) + modbus_cmd_byte[6];
        crc = checkCRC(modbus_cmd_byte, 6);
        if (crc_in != crc) {
          continue;
        }
        uart1_input_buffer.read_location = (uart1_input_buffer.read_location + 7) & 0x3F;
        return 1;
    }   
  }
  return 0;    
}

//this is the function for parsing and processing 
void ReceiveCommand(MODBUS_MESSAGE * cmd_ptr) {
  
  if (modbus_cmd_byte[1] & 0x80) {
    cmd_ptr->received_function_code = modbus_cmd_byte[1];
    cmd_ptr->function_code = EXCEPTION_FLAGGED;
    cmd_ptr->exception_code = ILLEGAL_FUNCTION;
  } else {
    cmd_ptr->function_code = modbus_cmd_byte[1] & 0x7F;
    cmd_ptr->data_address = (modbus_cmd_byte[2] << 8) + modbus_cmd_byte[3];
    switch (cmd_ptr->function_code) {
      case FUNCTION_READ_BITS:
        cmd_ptr->qty_bits = (modbus_cmd_byte[4] << 8) + modbus_cmd_byte[5];
        break;
            
      case FUNCTION_READ_REGISTERS:  
      case FUNCTION_READ_INPUT_REGISTERS:  
        cmd_ptr->qty_reg = (modbus_cmd_byte[4] << 8) + modbus_cmd_byte[5];
        
        if (cmd_ptr->qty_reg > 24) {                          // Limit to 24 registers read per message
          cmd_ptr->qty_reg = 24;
        }
        break;
    
      case FUNCTION_WRITE_BIT:
      case FUNCTION_WRITE_REGISTER:
        cmd_ptr->write_value = (modbus_cmd_byte[4] << 8) + modbus_cmd_byte[5];
        break;
    
      default:
        cmd_ptr->received_function_code = cmd_ptr->function_code;
        cmd_ptr->function_code = EXCEPTION_FLAGGED;
        cmd_ptr->exception_code = ILLEGAL_FUNCTION;
        break;
    }                      
  }    
}

void ProcessCommand (MODBUS_MESSAGE * ptr) {
  unsigned int coil_index;
  unsigned char bit_index;
  unsigned int byte_index;
  unsigned char byte_count;
  unsigned char last_bits;
  unsigned char data_index;
  unsigned int data_length_words;
  
  switch (ptr->function_code) {
      
    case FUNCTION_READ_BITS:

      if (((ptr->data_address + ptr->qty_bits) > SLAVE_BIT_ARRAY_SIZE) ||
          (ptr->data_address >= SLAVE_BIT_ARRAY_SIZE)){
        ptr->received_function_code = ptr->function_code;
        ptr->function_code = EXCEPTION_FLAGGED;
        ptr->exception_code = ILLEGAL_ADDRESS;
        break;  
      }

      if (ptr->qty_bits <= 8) {
        ptr->data_length_bytes = 1;
      } else if (ptr->qty_bits & 0x0007) {
        ptr->data_length_bytes = ((ptr->qty_bits /8) + 1) & 0xff;
      } else {
        ptr->data_length_bytes = (ptr->qty_bits /8) & 0xff;
      }
        
      byte_count = ptr->qty_bits / 8;
      last_bits = ptr->qty_bits & 0x07;
      
      int i;
      for (i=0; i<(byte_count+1); i++) {
          ptr->bit_data[i] = 0;
      }
      
      if (ptr->qty_bits == 1) {
        if (ModbusSlaveBit[ptr->data_address]) {
          ptr->bit_data[0] = 0x01;
        }
      } else if (ptr->qty_bits <= 8) {
        coil_index = ptr->data_address; 
        bit_index = 0;
        while (bit_index < ptr->qty_bits) {
          if (ModbusSlaveBit[coil_index] != 0) {
            ptr->bit_data[0] |= (0x01 << bit_index);
          }    
          coil_index++;
          bit_index++;            
        }      
      } else {
        byte_index = 0;
        coil_index = ptr->data_address;
        while (byte_index < byte_count) { 
          bit_index = 0;
          ptr->bit_data[byte_index] = 0;
          while (bit_index < 8) {
            if (ModbusSlaveBit[coil_index]) {
              ptr->bit_data[byte_index] |= (0x01 << bit_index);
            }
            bit_index++;
            coil_index++;
          } 
          byte_index++;
        }
        if (last_bits) {
          bit_index = 0;
          ptr->bit_data[byte_index] = 0;
          while (bit_index < 8) {
            if (bit_index < last_bits) {
              if (ModbusSlaveBit[coil_index] != 0) {
                ptr->bit_data[byte_index] |= (0x01 << bit_index);
              }  
              coil_index++;
            }
            bit_index++;            
          }                
        }
      }
      break;
      
    case FUNCTION_READ_REGISTERS:         
        
      if (((ptr->data_address + ptr->qty_reg) > SLAVE_HOLD_REG_ARRAY_SIZE) ||
          (ptr->data_address >= SLAVE_HOLD_REG_ARRAY_SIZE)){
        ptr->received_function_code = ptr->function_code;
        ptr->function_code = EXCEPTION_FLAGGED;
        ptr->exception_code = ILLEGAL_ADDRESS;
        break;  
      }
      data_length_words = ptr->qty_reg;
      byte_index = 0;
      data_index = 0;
      while (data_length_words) {
        ptr->data[data_index] =  ModbusSlaveHoldingRegister[ptr->data_address + byte_index];
        byte_index++;
        data_index++;
        data_length_words--;
      } 
      break;
      
    case FUNCTION_READ_INPUT_REGISTERS:
        
      if (((ptr->data_address + ptr->qty_reg) > SLAVE_INPUT_REG_ARRAY_SIZE) ||
          (ptr->data_address >= SLAVE_INPUT_REG_ARRAY_SIZE)){
        ptr->received_function_code = ptr->function_code;
        ptr->function_code = EXCEPTION_FLAGGED;
        ptr->exception_code = ILLEGAL_ADDRESS;
        break;  
      }
      data_length_words = ptr->qty_reg;
      byte_index = 0;
      data_index = 0;
      while (data_length_words) {
        ptr->data[data_index] =  ModbusSlaveInputRegister[ptr->data_address + byte_index];
        byte_index++;
        data_index++;
        data_length_words--;
      }
      break;
      
    case FUNCTION_WRITE_BIT:
      if (ptr->data_address >= SLAVE_BIT_ARRAY_SIZE) {
        ptr->received_function_code = ptr->function_code;
        ptr->function_code = EXCEPTION_FLAGGED;
        ptr->exception_code = ILLEGAL_ADDRESS;
        break;  
      }
      coil_index = ptr->data_address;
      if ((ptr->write_value == 0x0000) || (ptr->write_value == 0xFF00)) {
        ModbusSlaveBit[coil_index] = ptr->write_value;
        ETMEEPromWriteWord(0x640 + coil_index, ptr->write_value);
      } else {
        ptr->received_function_code = ptr->function_code;
        ptr->function_code = EXCEPTION_FLAGGED;
        ptr->exception_code = ILLEGAL_VALUE;
      }     
      break;
      
    case FUNCTION_WRITE_REGISTER:
      if (ptr->data_address >= SLAVE_HOLD_REG_ARRAY_SIZE) {
        ptr->received_function_code = ptr->function_code;
        ptr->function_code = EXCEPTION_FLAGGED;
        ptr->exception_code = ILLEGAL_ADDRESS;
        break;  
      }
      byte_index = ptr->data_address;

      if ((byte_index > 9) && (byte_index < 14)) {                     // If holding reg's 0x0A - 0x0D change
        if (ptr->write_value < 256) {
          ModbusSlaveHoldingRegister[byte_index] = ptr->write_value;
          ETMEEPromWriteWord(0x600 + byte_index, ptr->write_value);             
          SetCustomIP();                                                 // IP address change was made
        }
      } else {
        ModbusSlaveHoldingRegister[byte_index] = ptr->write_value;
        ETMEEPromWriteWord(0x600 + byte_index, ptr->write_value);  
      }
      break;
      
    default:
  	  break;
  }
}    


void CheckValidData(MODBUS_MESSAGE * ptr) {
    
  if ((modbus_slave_invalid_data != 0) && (ptr->function_code == FUNCTION_WRITE_REGISTER)) {     
    ptr->received_function_code = ptr->function_code;
    ptr->function_code = EXCEPTION_FLAGGED;
    ptr->exception_code = ILLEGAL_VALUE;
  }     
}
    
void CheckDeviceFailure(MODBUS_MESSAGE * ptr) {
  
//  if (_FAULT_REGISTER != 0) {
//    ptr->received_function_code = ptr->function_code;
//    ptr->function_code = EXCEPTION_FLAGGED;
//    ptr->exception_code = DEVICE_FAILURE; 
//  }
}


void SendResponse(MODBUS_MESSAGE * ptr) {
  unsigned int crc;
  unsigned int data_length_words;
  unsigned int address_16_msb;
  unsigned char address_msb;
  unsigned char address_lsb;
  unsigned int index;
  unsigned int data_16_msb;
  unsigned char data_msb;
  unsigned char data_lsb;
  unsigned int crc_16_msb;
  unsigned char crc_msb;
  unsigned char crc_lsb;
  unsigned int length_16_bytes;
  
  //BUFFERBYTE64 local_buffer;
  unsigned char output_data[64];
  
  // clear input/output buffer first
  //uart1_input_buffer.write_location = 0;  
  //uart1_input_buffer.read_location = 0;
  //uart1_output_buffer.write_location = 0;
  //uart1_output_buffer.read_location = 0;
  
  switch (ptr->function_code) {
    case FUNCTION_READ_BITS:
      BufferByte64WriteByte(&uart1_output_buffer, MODBUS_SLAVE_ADDR);
       output_data[0] = MODBUS_SLAVE_ADDR;
      BufferByte64WriteByte(&uart1_output_buffer, ptr->function_code);
       output_data[1] = ptr->function_code;
      BufferByte64WriteByte(&uart1_output_buffer, ptr->data_length_bytes);	// number of bytes to follow
       output_data[2] = ptr->data_length_bytes;
      data_length_words = ptr->data_length_bytes;
      index = 0;
      while (index < data_length_words) {
        BufferByte64WriteByte(&uart1_output_buffer, ptr->bit_data[index]);
         output_data[3 + index] = ptr->bit_data[index];
        index++;
      }
      crc = checkCRC(output_data, 3 + data_length_words);
      crc_16_msb = crc >> 8;
      crc_msb = (unsigned char)crc_16_msb & 0xff;
      crc_lsb = (unsigned char)crc & 0xff;
      BufferByte64WriteByte(&uart1_output_buffer, crc_lsb);
      BufferByte64WriteByte(&uart1_output_buffer, crc_msb);
      break;
      
    case FUNCTION_READ_REGISTERS: 
    case FUNCTION_READ_INPUT_REGISTERS:
      BufferByte64WriteByte(&uart1_output_buffer, MODBUS_SLAVE_ADDR);
       output_data[0] = MODBUS_SLAVE_ADDR;
      BufferByte64WriteByte(&uart1_output_buffer, ptr->function_code);
       output_data[1] = ptr->function_code;
      data_length_words = ptr->qty_reg;
      ptr->data_length_bytes = ((unsigned char)data_length_words * 2) & 0xff;
      BufferByte64WriteByte(&uart1_output_buffer, ptr->data_length_bytes);	// number of bytes to follow
       output_data[2] = ptr->data_length_bytes;
      index = 0;
      while (data_length_words) {
        data_16_msb = ptr->data[index] >> 8;
        data_msb = (unsigned char)data_16_msb & 0xff;
        data_lsb = (unsigned char)ptr->data[index] & 0xff;
        BufferByte64WriteByte(&uart1_output_buffer, data_msb);	// data Hi
         output_data[(index *2) + 3] = data_msb;
        BufferByte64WriteByte(&uart1_output_buffer, data_lsb);	// data Lo
         output_data[(index *2) + 4] = data_lsb;
        index++;
        data_length_words--;
      }  
      length_16_bytes = (unsigned int)ptr->data_length_bytes;
      crc = checkCRC(output_data, 3 + length_16_bytes);
      crc_16_msb = crc >> 8;
      crc_msb = (unsigned char)crc_16_msb & 0xff;
      crc_lsb = (unsigned char)crc & 0xff;
      BufferByte64WriteByte(&uart1_output_buffer, crc_lsb);
      BufferByte64WriteByte(&uart1_output_buffer, crc_msb);
      break;
     
    case FUNCTION_WRITE_BIT:
    case FUNCTION_WRITE_REGISTER:
      BufferByte64WriteByte(&uart1_output_buffer, MODBUS_SLAVE_ADDR);
       output_data[0] = MODBUS_SLAVE_ADDR;
      BufferByte64WriteByte(&uart1_output_buffer, ptr->function_code); 
       output_data[1] = ptr->function_code;
      address_16_msb = ptr->data_address >> 8;
      address_msb = (unsigned char)address_16_msb & 0xff;
      address_lsb = (unsigned char)ptr->data_address & 0xff;
      BufferByte64WriteByte(&uart1_output_buffer, address_msb);	// addr Hi
      BufferByte64WriteByte(&uart1_output_buffer, address_lsb);	// addr Lo
       output_data[2] = address_msb;
       output_data[3] = address_lsb;
      data_16_msb = ptr->write_value >> 8;
      data_msb = (unsigned char)data_16_msb & 0xff;
      data_lsb = (unsigned char)ptr->write_value & 0xff;
      BufferByte64WriteByte(&uart1_output_buffer, data_msb);	// data Hi
      BufferByte64WriteByte(&uart1_output_buffer, data_lsb);	// data Lo
       output_data[4] = data_msb;
       output_data[5] = data_lsb;
      crc = checkCRC(output_data, 6);
      crc_16_msb = crc >> 8;
      crc_msb = (unsigned char)crc_16_msb & 0xff;
      crc_lsb = (unsigned char)crc & 0xff;
      BufferByte64WriteByte(&uart1_output_buffer, crc_lsb);
      BufferByte64WriteByte(&uart1_output_buffer, crc_msb);
      break;
      
    case EXCEPTION_FLAGGED:
      BufferByte64WriteByte(&uart1_output_buffer, MODBUS_SLAVE_ADDR);
       output_data[0] = MODBUS_SLAVE_ADDR;
      BufferByte64WriteByte(&uart1_output_buffer, ptr->received_function_code); 
       output_data[1] = ptr->received_function_code;
      BufferByte64WriteByte(&uart1_output_buffer, ptr->exception_code);
       output_data[2] = ptr->exception_code;
      crc = checkCRC(output_data, 3);
      crc_16_msb = crc >> 8;
      crc_msb = (unsigned char)crc_16_msb & 0xff;
      crc_lsb = (unsigned char)crc & 0xff;
      BufferByte64WriteByte(&uart1_output_buffer, crc_lsb);
      BufferByte64WriteByte(&uart1_output_buffer, crc_msb);
      break;
      
    default:
      break;
      
  } 
}
 

void ClearModbusMessage(MODBUS_MESSAGE * ptr) {
  ptr->function_code = 0;
  ptr->received_function_code = 0;
  ptr->data_length_bytes = 0;
  ptr->exception_code = 0;
  ptr->done = 0;
  ptr->data_address = 0;
  ptr->qty_bits = 0;
  ptr->qty_reg = 0;
  ptr->write_value = 0;
//  ptr->data[125];
//  ptr->bit_data[125];
}

//-----------------------------------------------------------------------------
// CRC_Check
//-----------------------------------------------------------------------------
//
// Return Value : accum -- the end result of the CRC value.
// Parameter    : *ptr -- pointer to the array for CRC calculation.
//				: size -- size of the array
//
// This function calculates the 16-bit CRC value for the input array.
//
//-----------------------------------------------------------------------------
unsigned int checkCRC(unsigned char * ptr, unsigned int size)
{
    unsigned int i, j;
    unsigned int accum, element;

    accum = 0xffff;

    for (j = 0; j < size; j++)
    {
        element = ptr[j];

        for (i = 8; i > 0; i--)		
        {
            if (((element ^ accum) & 0x0001) > 0)
                accum = (unsigned int)((accum >> 1) ^ ((unsigned int)CRC_POLY));
            else
                accum >>= 1;

            element >>= 1;
        }
    }

    return (accum);
}


//-----------------------------------------------------------------------------
//   UART Interrupts
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
        
void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt(void) {
  _U1RXIF = 0;

  modbus_receiving_flag = 1;

  while (U1STAbits.URXDA) {
    BufferByte64WriteByte(&uart1_input_buffer, U1RXREG);
  }
  
}



void __attribute__((interrupt, no_auto_psv)) _U1TXInterrupt(void) {
  _U1TXIF = 0;
  while ((!U1STAbits.UTXBF) && (BufferByte64BytesInBuffer(&uart1_output_buffer))) {
    /*
      There is at least one byte available for writing in the output buffer and the transmit buffer is not full.
      Move a byte from the output buffer into the transmit buffer
    */
    U1TXREG = BufferByte64ReadByte(&uart1_output_buffer);
  }

}


#endif