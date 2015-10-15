/*! \file jtag430.c
  \author Travis Goodspeed <travis at radiantmachines.com>
  \brief MSP430 JTAG (16-bit)
*/

#include "platform.h"
#include "command.h"
#include "jtag430.h"

//! Handles classic MSP430 JTAG commands.  Forwards others to JTAG.
void jtag430_handle_fn(uint8_t const app,
		       uint8_t const verb,
		       uint32_t const len);

// define the jtag430 app's app_t
app_t const jtag430_app = {
  /* app number */
  JTAG430,
  
  /* handle fn */
  jtag430_handle_fn,
  
  /* name */
  "JTAG430",
  
  /* desc */
    ""
#if 0
  "\tThe JTAG430 app adds to the basic JTAG app\n"
  "\tsupport for JTAG'ing MSP430 devices.\n"
#endif
};

unsigned int jtag430mode=MSP430X2MODE;

unsigned int drwidth=16;

//! Shift an address width of data
uint32_t jtag430_shift_addr( uint32_t addr )
{
	if (!in_run_test_idle())
	{
		debugstr("Not in run-test-idle state");
		return 0;
	}

	// get intot the right state
	jtag_capture_dr();
	jtag_shift_register();

	// shift DR, then idle
	return jtag_trans_n(addr, drwidth, MSB);
}

//! Set a register.
void jtag430_setr(u8 reg, u16 val){
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  jtag_dr_shift_16(0x3401);// release low byte
  jtag_ir_shift_8(IR_DATA_16BIT);
  
  //0x4030 is "MOV #foo, r0"
  //Right-most field is register, so 0x4035 loads r5
  jtag_dr_shift_16(0x4030+reg);
  CLRTCLK;
  SETTCLK;
  jtag_dr_shift_16(val);// Value for the register
  CLRTCLK;
  jtag_ir_shift_8(IR_ADDR_CAPTURE);
  SETTCLK;
  CLRTCLK ;// Now reg is set to new value.
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  jtag_dr_shift_16(0x2401);// low byte controlled by JTAG
}

//! Set the program counter.
void jtag430_setpc(unsigned int adr){
  jtag430_setr(0,adr);
}

//! Halt the CPU
void jtag430_haltcpu(){
  //jtag430_setinstrfetch();
  
  jtag_ir_shift_8(IR_DATA_16BIT);
  jtag_dr_shift_16(0x3FFF);//JMP $+0
  
  CLRTCLK;
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  jtag_dr_shift_16(0x2409);//set JTAG_HALT bit
  SETTCLK;
}

//! Release the CPU
void jtag430_releasecpu(){
  CLRTCLK;
  //debugstr("Releasing target MSP430.");
  
  /*
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  jtag_dr_shift_16(0x2C01); //Apply reset.
  jtag_dr_shift_16(0x2401); //Release reset.
  */
  jtag_ir_shift_8(IR_CNTRL_SIG_RELEASE);
  SETTCLK;
}

//! Read data from address
unsigned int jtag430_readmem(unsigned int adr){
  unsigned int toret;
  jtag430_haltcpu();
  
  CLRTCLK;
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  
  if(adr>0xFF)
    jtag_dr_shift_16(0x2409);//word read
  else
    jtag_dr_shift_16(0x2419);//byte read
  jtag_ir_shift_8(IR_ADDR_16BIT);
  jtag430_shift_addr(adr);//address
  jtag_ir_shift_8(IR_DATA_TO_ADDR);
  SETTCLK;

  CLRTCLK;
  toret=jtag_dr_shift_16(0x0000);//16 bit return
  
  return toret;
}

//! Write data to address.
void jtag430_writemem(unsigned int adr, unsigned int data){
  CLRTCLK;
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  if(adr>0xFF)
    jtag_dr_shift_16(0x2408);//word write
  else
    jtag_dr_shift_16(0x2418);//byte write
  jtag_ir_shift_8(IR_ADDR_16BIT);
  jtag430_shift_addr(adr);
  jtag_ir_shift_8(IR_DATA_TO_ADDR);
  jtag_dr_shift_16(data);
  SETTCLK;
}

//! Write data to flash memory.  Must be preconfigured.
void jtag430_writeflashword(unsigned int adr, unsigned int data){
  
  CLRTCLK;
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  jtag_dr_shift_16(0x2408);//word write
  jtag_ir_shift_8(IR_ADDR_16BIT);
  jtag430_shift_addr(adr);
  jtag_ir_shift_8(IR_DATA_TO_ADDR);
  jtag_dr_shift_16(data);
  SETTCLK;
  
  //Return to read mode.
  CLRTCLK;
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  jtag_dr_shift_16(0x2409);
  
  /*
  jtag430_writemem(adr,data);
  CLRTCLK;
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  jtag_dr_shift_16(0x2409);
  */
  
  //Pulse TCLK
  jtag430_tclk_flashpulses(35); //35 standard
}

//! Configure flash, then write a word.
void jtag430_writeflash(unsigned int adr, unsigned int data){
  jtag430_haltcpu();
  
  //FCTL1=0xA540, enabling flash write
  jtag430_writemem(0x0128, 0xA540);
  //FCTL2=0xA540, selecting MCLK as source, DIV=1
  jtag430_writemem(0x012A, 0xA540);
  //FCTL3=0xA500, should be 0xA540 for Info Seg A on 2xx chips.
  jtag430_writemem(0x012C, 0xA500); //all but info flash.
  //if(jtag430_readmem(0x012C));
  
  //Write the word itself.
  jtag430_writeflashword(adr,data);
  
  //FCTL1=0xA500, disabling flash write
  jtag430_writemem(0x0128, 0xA500);
  
  //jtag430_releasecpu();
}



//! Power-On Reset
void jtag430_por(){
  // Perform Reset
  jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
  jtag_dr_shift_16(0x2C01); // apply
  jtag_dr_shift_16(0x2401); // remove
  CLRTCLK;
  SETTCLK;
  CLRTCLK;
  SETTCLK;
  CLRTCLK;
  jtagid = jtag_ir_shift_8(IR_ADDR_CAPTURE); // get JTAG identifier
  SETTCLK;
  
  jtag430_writemem(0x0120, 0x5A80);   // Diabled Watchdog
}



#define ERASE_GLOB 0xA50E
#define ERASE_ALLMAIN 0xA50C
#define ERASE_MASS 0xA506
#define ERASE_MAIN 0xA504
#define ERASE_SGMT 0xA502

static const unsigned int cpuid_fast_flash[] = {
    0x0253, 0x2452, 0x2553, 0x255C, 0x2955, 0x4020, 0x427A,
    0xF201, 0xF213, 0xF227, 0xF237, 0xF249, 0xF26F, 0xF46F,
    0x0000  // end marker
    };


//! Configure flash, then write a word.
void jtag430_eraseflash(unsigned int mode, unsigned int adr,
			unsigned int info){
  jtag430_haltcpu();

  unsigned int count = 5300;
  unsigned char cycles = 19, i;

  unsigned int cpuid = jtag430_readmem(0xff0);
  i = cpuid & 0xff;
  cpuid = (i << 8) + ((cpuid >> 8) & 0xff);
  
  for (i = 0; cpuid_fast_flash[i]; i++) {
    if (cpuid == cpuid_fast_flash[i]) {
        count = 10600;
        cycles = 1;
        break;
    }
  }

  while (cycles) {
       --cycles;

      //FCTL1= erase mode
      jtag430_writemem(0x0128, mode);
      //FCTL2=0xA540, selecting MCLK as source, DIV=1
      jtag430_writemem(0x012A, 0xA540);
      //FCTL3=0xA500, should be 0xA540 for Info Seg A on 2xx chips.
      if(info)
        jtag430_writemem(0x012C, 0xA540);
      else
        jtag430_writemem(0x012C, 0xA500);
      
      //Write the erase word.
      jtag430_writemem(adr, 0x55AA);
      //Return to read mode.
      CLRTCLK;
      jtag_ir_shift_8(IR_CNTRL_SIG_16BIT);
      jtag_dr_shift_16(0x2409);
      
      //Send the pulses.
      jtag430_tclk_flashpulses(count);
      
      //FCTL1=0xA500, disabling flash write
      jtag430_writemem(0x0128, 0xA500);
  }

  //jtag430_releasecpu();
}


//! Reset the TAP state machine.
void jtag430_resettap(){
  int i;
  // Settle output
  SETTDI; //430X2
  SETTMS;
  //SETTDI; //classic
  jtag_tcktock();

  // Navigate to reset state.
  // Should be at least six.
  for(i=0;i<4;i++){
    jtag_tcktock();
  }

  // test-logic-reset
  CLRTMS;
  jtag_tcktock();
  SETTMS;
  // idle

    
  /* sacred, by spec.
     Sometimes this isn't necessary.  */
  // fuse check
  CLRTMS;
  delay(50);
  SETTMS;
  CLRTMS;
  delay(50);
  SETTMS;
  /**/
  
}


//! Get the JTAG ID
unsigned char jtag430x2_jtagid(){
  jtag430_resettap();
  jtagid = jtag_ir_shift_8(IR_BYPASS);
  if(jtagid!=0x89 && jtagid!=0x91){
    debugstr("Unknown JTAG ID");
    debughex(jtagid);
  }
  return jtagid;
}
//! Start JTAG, take pins
unsigned char jtag430x2_start(){
  SETRST;//== start with RST=1 for better stability
  jtag_setup();
  
  //Known-good starting position.
  //Might be unnecessary.
  SETTST;
  SETRST;
  
  delay(0xFFFF);
  
  //Entry sequence from Page 67 of SLAU265A for 4-wire MSP430 JTAG
  CLRRST;
  delay(20);//10
  CLRTST;

  delay(10);//5
  SETTST;
  msdelay(10);//5
  SETRST;
  P5DIR&=~RST;
  
  delay(0xFFFF);
  
  //Perform a reset and disable watchdog.
  return jtag430x2_jtagid();
}


//! Start JTAG, take pins
void jtag430_start(){
  jtag_setup();
  
  //Known-good starting position.
  //Might be unnecessary.
  SETTST;
  SETRST;
  delay(0xFFFF);


  #ifndef SBWREWRITE
  //Entry sequence from Page 67 of SLAU265A for 4-wire MSP430 JTAG
  CLRRST;
  delay(100); //100
  CLRTST;
  delay(50);  //50
  SETTST;
  delay(50);  //50
  SETRST;
  P5DIR&=~RST;
  delay(0xFFFF);
  #endif
  
  //Perform a reset and disable watchdog.
  jtag430_por();
  jtag430_writemem(0x120,0x5a80);//disable watchdog
  
  jtag430_haltcpu();
}

//! Stop JTAG.
void jtag430_stop(){
  //debugstr("Exiting JTAG.");
  jtag_setup();
  
  //Known-good starting position.
  //Might be unnecessary.
  //SETTST;
  CLRTST;
  SETRST;
  delay(0xFFFF);
  
  //Entry sequence from Page 67 of SLAU265A for 4-wire MSP430 JTAG
  CLRRST;
  delay(0xFFFF);
  SETRST;
  //P5DIR&=~RST;
  //delay(0xFFFF);
  
}

//! Set CPU to Instruction Fetch
void jtag430_setinstrfetch(){
  
  jtag_ir_shift_8(IR_CNTRL_SIG_CAPTURE);

  // Wait until instruction fetch state.
  while(1){
    if (jtag_dr_shift_16(0x0000) & 0x0080)
      return;
    CLRTCLK;
    SETTCLK;
  }
}





//! Handles classic MSP430 JTAG commands.  Forwards others to JTAG.
void jtag430_handle_fn(uint8_t const app,
		       uint8_t const verb,
		       uint32_t const len)
{
  unsigned long at, l;
  unsigned int i, val;
  
  
#if 0
  /* FIXME
   * Sometimes JTAG doesn't init correctly.
   * This restarts the connection if the masked-rom
   * chip ID cannot be read.  Should print warning
   * for testing server.
   */
  if (jtagid!=0)
    while((i=jtag430_readmem(0xff0))==0xFFFF){
      debugstr("Reconnecting to target MSP430.");
      jtag430x2_start();
      led_toggle();
    }
#endif
  led_off();
  
  
  switch(verb){
  case START:
    //debugstr("Using JTAG430 (instead of JTAG430X2)!");
    
    jtag430x2_start();
    cmddata[0]=jtagid;
    
    jtag430mode=MSP430MODE;
    
    /* So the way this works is that a width of 20 does some
       backward-compatibility finagling, causing the correct value
       to be exchanged for addresses on 16-bit chips as well as the
       new MSP430X chips.  (This has only been verified on the
       MSP430F2xx family.  TODO verify for others.)
    */
    
    drwidth=20;
    
    //Perform a reset and disable watchdog.
    jtag430_por();
    jtag430_writemem(0x120,0x5a80);//disable watchdog
    
    jtag430_haltcpu();
    
    jtag430_resettap();
    txdata(app,verb,1);
    

    break;
  case STOP:
    jtag430_stop();
    txdata(app,verb,0);
    break;
  case JTAG430_HALTCPU:
    jtag430_haltcpu();
    txdata(app,verb,0);
    break;
  case JTAG430_RELEASECPU:
    jtag430_releasecpu();
    txdata(app,verb,0);
    break;
  case JTAG430_SETINSTRFETCH:
    jtag430_setinstrfetch();
    txdata(app,verb,0);
    break;
    
  case JTAG430_READMEM:
  case PEEK:
    at=cmddatalong[0];
    
    //Fetch large blocks for bulk fetches,
    //small blocks for individual peeks.
    if(len>5)
      l=(cmddataword[2]);//always even.
    else
      l=2;
    l&=~1;//clear lsbit
    
    txhead(app,verb,l);
    for(i = 0; i < l; i += 2) {
      jtag430_resettap();
      val=jtag430_readmem(at);
      
      at+=2;
      serial_tx(val&0xFF);
      serial_tx((val&0xFF00)>>8);
    }
    break;
  case JTAG430_WRITEMEM:
  case POKE:
    jtag430_haltcpu();
    jtag430_writemem(cmddataword[0],cmddataword[2]);
    cmddataword[0]=jtag430_readmem(cmddataword[0]);
    txdata(app,verb,2);
    break;
  case JTAG430_WRITEFLASH:
    at=cmddataword[0];
    
    for(i=0;i<(len>>1)-2;i++){
      //debugstr("Poking flash memory.");
      jtag430_writeflash(at+(i<<1),cmddataword[i+2]);
      //Reflash if needed.  Try this twice to save grace?
      if(cmddataword[i]!=jtag430_readmem(at))
	jtag430_writeflash(at+(i<<1),cmddataword[i+2]);
    }
    
    //Return result of first write as a word.
    cmddataword[0]=jtag430_readmem(cmddataword[0]);
    
    txdata(app,verb,2);
    break;
  case JTAG430_ERASEFLASH:
    jtag430_eraseflash(ERASE_MASS,0xFFFE,0);
    txdata(app,verb,0);
    break;
  case JTAG430_ERASEINFO:
    jtag430_eraseflash(ERASE_SGMT,0x1000,1);
    txdata(app,verb,0);
    break;
  case JTAG430_SETPC:
    jtag430_haltcpu();
    //debughex("Setting PC.");
    //debughex(cmddataword[0]);
    jtag430_setpc(cmddataword[0]);
    jtag430_releasecpu();
    txdata(app,verb,0);
    break;
  case JTAG430_SETREG:
    jtag430_setr(cmddata[0],cmddataword[1]);
    txdata(app,verb,0);
    break;
  case JTAG430_GETREG:
    //jtag430_getr(cmddata[0]);
    debugstr("JTAG430_GETREG not yet implemented.");
    cmddataword[0]=0xDEAD;
    txdata(app,verb,2);
    break;
  case JTAG430_COREIP_ID:
    //cmddataword[0]=jtag430_coreid();
    cmddataword[0]=0xdead;
    txdata(app,verb,2);
    break;
  case JTAG430_DEVICE_ID:
    //cmddatalong[0]=jtag430_deviceid();
    cmddataword[0]=0xdead;
    cmddataword[1]=0xbeef;
    txdata(app,verb,4);
    break;
  default:
    //(*(jtag_app.handle))(app,verb,len);
    jtag_app.handle(app,verb,len);
  }
  //jtag430_resettap();  //DO NOT UNCOMMENT
}
