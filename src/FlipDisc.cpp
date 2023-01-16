/*----------------------------------------------------------------------------------*
 * FlipDisc.cpp - Arduino library for flip-disc displays.                           *
 * This library is intended for use with the displays designed by                   *
 * www.Flipo.io - Marcin Saj.                                                       *
 *                                                                                  *
 * The MIT License                                                                  *
 * Marcin Saj 15 Jan 2023                                                           *
 * https://github.com/marcinsaj/FlipDisc                                            *
 *----------------------------------------------------------------------------------*/

#include "FlipDisc.h"

/* 
 *  2-dimensional array with 3 columns.
 * The first column lists all connected displays,
 * the second column is the number of data bytes required to control the display,
 * the third column is a number defining the relative position of the modules
 * in relation to each other and for each type separately. 
 */
uint8_t moduleInitArray[8][3];

/* 
 *  An array of defined display names:
 * -> SEG - 7-segment flip-disc display
 * -> DOTS - 2 or 3 dots module flip-disc display
 * -> FLIP3 - 1x3 flip-disc display
 * -> FLIP7 - 1x7 flip-disc display 
 */
static const uint8_t moduleTypeArray[4] PROGMEM = {SEG, DOTS, FLIP3, FLIP7};

/*
 * Total length of data frame for all displays.
 * We can only control one disc out of all connected display modules at a time. 
 * Despite the fact that we only need two bits to control one disc, we have to send 
 * a data frame of a length equal to the sum of the data for all displays each time.
 * If there is no display module declaration, each empty module up to eight 
 * is supplemented with 3 bytes (largest display requires 3 bytes of data). 
 * If the user does not declare the connected display, incorrect operation 
 * or damage to the display may occur (thats why these 3 bytes).
 */
uint8_t numberAllBytes = 0; 
    
/*----------------------------------------------------------------------------------*
 * Initialization function for a series of displays. The function has 1 default     *
 * argument and 7 optional arguments. The function prepares SPI and configures      *
 * EN_PIN - the output serves as a latch for shift registers on which               *
 * the controllers built into the displays are based. The function also configures  *
 * the PL and CH outputs for the Pulse Shaper Power Supply module.                  *
 * Correct initialization requires names of the serially connected displays.        *
 *----------------------------------------------------------------------------------*/
void FlipDisc::Init(uint8_t MOD1, uint8_t MOD2 = 0xFF, uint8_t MOD3 = 0xFF, 
                                  uint8_t MOD4 = 0xFF, uint8_t MOD5 = 0xFF, 
                                  uint8_t MOD6 = 0xFF, uint8_t MOD7 = 0xFF, 
                                  uint8_t MOD8 = 0xFF)
{
  SPI.begin();
 
  pinMode(PL_PIN, OUTPUT);
  digitalWrite(PL_PIN, LOW);

  pinMode(CH_PIN, OUTPUT);
  digitalWrite(CH_PIN, LOW);
  
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);

  // First charging Pulse Shaper Power Supply module after power up the device
  PrepareCurrentPulse();

  // Saving a list of displays to the array
  moduleInitArray[0][0] = MOD1;
  moduleInitArray[1][0] = MOD2;
  moduleInitArray[2][0] = MOD3;
  moduleInitArray[3][0] = MOD4;
  moduleInitArray[4][0] = MOD5;
  moduleInitArray[5][0] = MOD6;
  moduleInitArray[6][0] = MOD7;
  moduleInitArray[7][0] = MOD8;  
  
/*
 * moduleRelativePosition variable
 * for example, if there is a SEG, DOTS, SEG, SEG, DOTS, FLIP3 in a series
 * of displays, then the relative positions will be 1, 1, 2, 3, 2, 1.
 * Each type of display is counted separately.
 * In simple terms, it can be presented as a separate list for each
 * type of display and numbered starting from 1. Determining
 * the relative position of the display is used to simplify the display
 * handling functions.
 */
  uint8_t moduleRelativePosition = 0;
  
  uint8_t moduleType = 0;
  uint8_t moduleNumberTypes = sizeof(moduleTypeArray);

  // For all types of displays
  for(int i = 0; i < moduleNumberTypes; i++)
  {
    // The initial value of the relative position for each type of the display
    moduleRelativePosition = 0;
    // Reading the first type of display to check
    moduleType = pgm_read_byte(&moduleTypeArray[i]); 
    
    // For the selected display type, search the entire list of serially connected displays
    for(int j = 0; j < 8; j++) 
    {     
      // If the selected display type is found in the list 
      // then the relative position is incremented and stored in the array
      if(moduleInitArray[j][0] == moduleType) 
      {
        moduleRelativePosition = moduleRelativePosition + 1;   
        moduleInitArray[j][2] = moduleRelativePosition;
      }
    }
  }
  
/*
 * Extracting the number of data bytes of each display type, the number of bytes 
 * determines the size of the control frame. Required by the displays architecture,
 * which is based on shift registers.
 * If the user does not declare the connected display, incorrect operation or
 * damage to the display may occur. Therefore, if there is no display module
 * declaration, each empty module up to eight is supplemented with 3 bytes.
 * We can only handle one disc from the entire display series at a time, so it is
 * very important where in the data stream to put the control bits for the selected
 * disc. In fact, with a data frame of a few or a dozen bytes, only two bits are
 * set to "1" the rest of the data is "0".
 */
  uint8_t numberBytes = 0;
  
  for(int i = 0; i < 8; i++)
  {
    switch (moduleInitArray[i][0]) 
    {
      case SEG:
        numberBytes = 3;
        break;

      case DOTS:
        numberBytes = 1;
        break;  

      case FLIP3:
        numberBytes = 1;
        break;
        
      case FLIP7:
        numberBytes = 2;
        break;
      
      case NONE:
        numberBytes = 3;
        break;   
        
      default:
        numberBytes = 3;
        break;
    }
    
    // Saving data about the number of data bytes required by the module
    moduleInitArray[i][1] = numberBytes;

    // Total length of data frame for all displays.
    numberAllBytes = numberAllBytes + numberBytes; 
  }
}

/*----------------------------------------------------------------------------------*
 * Function to control up to eight 7-segment displays.                              *
 * The first argument is the default and the others are optional.                   *
 * This function allows you to display numbers and symbols: 0-9, "°","C" , "F", etc.*
 *----------------------------------------------------------------------------------*/
void FlipDisc::Seg(uint8_t data1, uint8_t data2 = 0xFF, uint8_t data3 = 0xFF, 
                                  uint8_t data4 = 0xFF, uint8_t data5 = 0xFF, 
                                  uint8_t data6 = 0xFF, uint8_t data7 = 0xFF, 
                                  uint8_t data8 = 0xFF)
{
  // Saving a list of data to the array
  uint8_t newDataArray[8];
  newDataArray[0] = data1;
  newDataArray[1] = data2;
  newDataArray[2] = data3;
  newDataArray[3] = data4;
  newDataArray[4] = data5;
  newDataArray[5] = data6;
  newDataArray[6] = data7;
  newDataArray[7] = data8;
  
  for(int i = 0; i < 8; i++)
  {   
    // Call the function to handle the display only if there is data for it
    if(newDataArray[i] != 0xFF) ToSeg(i + 1, newDataArray[i]);
  }
}

/*----------------------------------------------------------------------------------*
 * The function allows you to control one 7-segment display.                        *
 * The first argument is the relative number of the display in the series           *
 * of all displays. For example, if we have a combination of SEG, DOTS, SEG then    *
 * the second SEG display will have a relative number of 2 even though there        *
 * is a DOTS display between the SEG displays.                                      *
 * -> segNumber - relative number of the "SEG" display                              *
 * -> data                                                                          *
 *                                                                                  *
 * Brief:                                                                           *
 * The display consists of 23 discs. The displaySegArray[][] array contains         *
 * information about all disc statuses for the symbol we want to display.           *
 * To flip a selected disc, we need to know the corresponding control outputs.      *
 * Disc statuses "0" and "1" correspond to different control outputs.               *
 * The list of information about all control outputs for corresponding              *
 * disc statuses of all discs and the currently selected digit/symbol to be         *
 * displayed are contained in two tables:                                           *
 * -> setDiscArray[][] - "1"                                                        *
 * -> resetDiscArray[][] - "0"                                                      *
 *----------------------------------------------------------------------------------*/
void FlipDisc::ToSeg(uint8_t segNumber, uint8_t data)
{
  bool newDiscStatus = 0;

  // The display is built with 23 independently controlled flip-discs
  for(int discNumber = 0; discNumber < 23; discNumber++)
  {
   /* 
    * The corresponding cell of the displaySegArray[][] array 
    * contains information about the status of the currently selected disc.
    * -> data - row of a multidimensional bit array
    * -> discNumber - column number containing the status of the disc 
    *    to be displayed "0" or "1".
    */
    newDiscStatus = displaySegArray[data][discNumber];
    
    // Start of SPI data transfer
    digitalWrite(EN_PIN, LOW);

   /* 
    * Send blank data "0" to all control outputs of the other displays before 
    * sending control data to the selected display.
    * Detailed information in the function description SendBlankData().
    */
    SendBlankData(segNumber, SEG, Before);
    
   /*
    * Each of the discs has two sides, one side corresponds to the disk status "1" - color, 
    * the other side "0" - black. 
    * Each of the "0" or "1" statuses requires a different polarity of the current pulse 
    * released into the disc, and thus for each of the statuses we must drive different 
    * controller outputs to achieve the desired effect. 
    * The list of information about the statuses of all discs for the entire display 
    * and the currently selected digit/symbol to be displayed is contained in two tables:
    * -> setDiscArray[][] - "1" 
    * -> resetDiscArray[][] - "0"
    * The arrays contains the addresses of all control outputs corresponding to the setting 
    * of the discs to the "color" side or "black" side.
    * Each separate display disc requires 3 bytes of data to be transferred. 
    * To flip the entire display, we need to send 3 bytes x 23 discs = 69 bytes of data
    */
    for(int byteNumber = 0; byteNumber < 3; byteNumber++)
    {
      if(newDiscStatus == 0) SPI.transfer(pgm_read_byte(&setDiscArray[discNumber][byteNumber]));
      if(newDiscStatus == 1) SPI.transfer(pgm_read_byte(&resetDiscArray[discNumber][byteNumber]));
    }

   /* 
    * Send blank data "0" to all control outputs of the other displays after 
    * sending control data to the selected display.
    * Detailed information in the function description SendBlankData().
    */
    SendBlankData(segNumber, SEG, After);

    // End of SPI data transfer
    digitalWrite(EN_PIN, HIGH);
    
    // Release of 1ms current pulse
    ReleaseCurrentPulse();
  }
  // Finally, clear all display outputs
  ClearAllOutputs();
}

/*----------------------------------------------------------------------------------*
 * The function allows you to control one 2 or 3-dot display.                       *                        
 * We can control one, two or three dots of the selected display at a time.         *
 * The first argument is the relative number of the display in the series           *
 * of all displays. For example, if we have a combination of DOTS, SEG, DOTS, then  *
 * the second DOTS display will have a relative number of 2 even though there       *
 * is a SEG display between the DOTS displays.                                      *
 * -> segNumber - relative number of the "DOTS" display                             *  
 * -> data                                                                          *
 *                                                                                  *
 * Brief:                                                                           *
 * The display consists of 2 or 3 discs.                                            *
 * To flip a selected disc dot, we need to know the corresponding control outputs.  *
 * The list of information about control outputs for corresponding discs            *
 * to be displayed are contained in two tables:                                     *
 * -> setDiscDot[] - "1"                                                            *
 * -> resetDiscDot[] - "0"                                                          *
 *----------------------------------------------------------------------------------*/
void FlipDisc::Dot(uint8_t segNumber, uint8_t dot1, uint8_t dot2 = 0xFF, uint8_t dot3 = 0xFF)
{
  // Saving a list of dot statuses to the array
  uint8_t newDotArray[3];
  newDotArray[0] = dot1;
  newDotArray[1] = dot2;
  newDotArray[2] = dot3;

  // 3 dots - 3 loops
  for(int dot = 0; dot < 3; dot++)
  {  
    // Check if we have new data for dot. 0xFF - no data
    if(newDotArray[dot] != 0xFF)
    {
      // Start of SPI data transfer
      digitalWrite(EN_PIN, LOW);

     /* 
      * Send blank data "0" to all control outputs of the other displays before 
      * sending control data to the selected display.
      * Detailed information in the function description SendBlankData().
      */
      SendBlankData(segNumber, DOTS, Before);

     /*
      * Each of the discs has two sides, one side corresponds to the disk status "1" - color, 
      * the other side "0" - black. 
      * Each of the "0" or "1" statuses requires a different polarity of the current pulse 
      * released into the disc, and thus for each of the statuses we must drive different 
      * controller outputs to achieve the desired effect. 
      * The list of information about the statuses of all discs for the display 
      * and the currently selected dot to be displayed is contained in two tables:
      * -> setDiscDot[] - "1" 
      * -> resetDiscDot[] - "0"
      * Each separate display disc requires 1 byte of data to be transferred. 
      * To flip all 3 dots, we need to send 3 bytes of data.
      */
      if(newDotArray[dot] == 1) SPI.transfer(pgm_read_byte(&setDiscDot[dot]));
      if(newDotArray[dot] == 0) SPI.transfer(pgm_read_byte(&resetDiscDot[dot]));
      
     /* 
      * Send blank data "0" to all control outputs of the other displays after 
      * sending control data to the selected display.
      * Detailed information in the function description SendBlankData().
      */
      SendBlankData(segNumber, DOTS, After);

      // End of SPI data transfer
      digitalWrite(EN_PIN, HIGH);

      // Release of 1ms current pulse 
      ReleaseCurrentPulse();
    }  
  }

  // Finally, clear all display outputs
  ClearAllOutputs();
}

/*----------------------------------------------------------------------------------*
 * Function for sending empty "0" data to complete the control data string.         *
 * -> moduleNumber - (1-8) the relative position of the display in relation         *
 *    to the displays of the selected type                                          *
 * -> moduleType - (SEG, DOTS, FLIP3, FLIP7) selected display type                  *
 * -> dataPosition - ("Before", "After") order of sending data, before control data *
 *    or after control data.                                                        *
 *                                                                                  *
 * Brief:                                                                           *
 * First, based on the arguments, the function determines the absolute position     *
 * of the selected display in the series of all displays. Then, based on the        *
 * information about the types of displays before or after the currently selected   *
 * display, it prepares an empty data frame "0" and sends this data.                *
 * For proper control of the display, the so-called empty supplementary data frames *
 * are absolutely necessary!                                                        *
 * The point is that we can only control one disc out of all connected display      *
 * modules at a time. So in the entire data frame for, e.g. eight 7-segment         *
 * displays, for 24 bytes of data there are only 2 bits set to "1" and              *
 * the remaining 190 bits must be set to "0". The architecture of the embedded      *
 * display controller requires it.                                                  *
 *----------------------------------------------------------------------------------*/
void FlipDisc::SendBlankData(uint8_t moduleNumber, uint8_t moduleType, uint8_t dataPosition)
{
  uint8_t emptyByte = 0;

  for(uint8_t moduleAbsolutePosition = 0; moduleAbsolutePosition < 8; moduleAbsolutePosition++)
  {  
    // Look for the selected display type
    if(moduleInitArray[moduleAbsolutePosition][0] == moduleType)
    { 
     /*
      * Then look for a specific display based on a relative number from a series 
      * of displays of the same type. Based on these two parameters "moduleNumber" and 
      * "moduleType" we determined the absolute position of the display "moduleAbsolutePosition". 
      * Then, based on the absolute position of the display, we calculate the empty data frame "0"
      * and sending this data.
      */
      if(moduleInitArray[moduleAbsolutePosition][2] == moduleNumber)
      {       
        if(dataPosition == Before)
        {
          // In the calculation, we omit the currently selected display "(moduleAbsolutePosition + 1)"
          for(int i = (moduleAbsolutePosition + 1); i < 8; i++) emptyByte = moduleInitArray[i][1] + emptyByte;     
          for(int j = 0; j < emptyByte; j++) SPI.transfer(0);
          return;     
        }
        
        if(dataPosition == After)
        {
          // In the calculation, we omit the currently selected display "(moduleAbsolutePosition - 1)"
          for(int i = 0; i <= (moduleAbsolutePosition - 1); i++) emptyByte = moduleInitArray[i][1] + emptyByte;
          for(int j = 0; j < emptyByte; j++) SPI.transfer(0);
          return;      
        }
      }
    }
  }
}

/*----------------------------------------------------------------------------------*
 * This function is used to disable all outputs of all displays in order to protect *
 * the displays against incorrect control or failure of the power module.           *
 *----------------------------------------------------------------------------------*/
void FlipDisc::ClearAllOutputs(void)
{
  // Start of SPI data transfer
  digitalWrite(EN_PIN, LOW);
  // Clear all outputs of connected displays 
  for(int i = 0; i < numberAllBytes; i++) SPI.transfer(0);
  // End of SPI data transfer
  digitalWrite(EN_PIN, HIGH); 
}

/*----------------------------------------------------------------------------------*
 * This function is used to prepare the Pulse Shaper Power Supply module 
 * for the first time after power up the device.
 *----------------------------------------------------------------------------------*/
void FlipDisc::PrepareCurrentPulse(void)
{
  digitalWrite(PL_PIN, LOW);    // Turn OFF PSPS module output
  digitalWrite(CH_PIN, HIGH);   // Turn ON charging
  delay(100);                   // first charging time 100ms
  digitalWrite(CH_PIN, LOW);    // Turn OFF charging
}

/*----------------------------------------------------------------------------------*
 * The function is used to generate a current pulse of 1ms length 
 * required by flip-disc displays.
 *----------------------------------------------------------------------------------*/
void FlipDisc::ReleaseCurrentPulse(void)
{
  digitalWrite(PL_PIN, LOW);    // Turn OFF PSPS module output
  digitalWrite(CH_PIN, HIGH);   // Turn ON charging
  delayMicroseconds(100);       // Charging time 100us
  digitalWrite(CH_PIN, LOW);    // Turn OFF charging 
  digitalWrite(PL_PIN, HIGH);   // Turn ON PSPS module output
  delay(1);                     // 1ms current pulse
  digitalWrite(PL_PIN, LOW);    // Turn OFF PSPS module output  
}
