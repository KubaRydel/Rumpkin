//  Rumpkin v 0.3 
//  Arduino MIDI patch saver for Korg NTS1
//  Developed by Jakub Jelita Rydel 2020-2021
//  Released under Creative Commons license
//  R
//
// LIBRARIES  //
#include <MIDIUSB.h>
#include <EEPROM.h>
#include <CircularBuffer.h>
//
//
// CONSTANTS //
const int onboardLedPin = 13;                 // Onoard LED pin number
const int eepromPerBankOffset =  30;          // EEPROM offset per bank
//
//
// GLOBAL VARIABLES //
int ii;                                       // Generic iterator
int midiChannel = 16;                         // MIDI channel (0-15; 16 -> unassigned)
midiEventPacket_t midiEvent;                  // midiEventPacket variable to store MIDI events
bool ledStatus = false;                       // Onboard LED status (false - off, true - on)
long ledUpdateTime = millis();                // Most recent time at which LED status was updated
int activeBank=0;                             // Active bank number
byte ccs[29];                                 // Array to store MIDI CCs numbers of NTS1
char *ccsNames[29];                           // Array to store MIDI CCs names of NTS1
byte currentPatch[29];                        // Array to store current patch (CC values)
byte intPatch[29];                            // Initialisation patch
int midiCommand = 0;                          // Variable to store current MIDI command to act upon (0 - none)
CircularBuffer<byte,4> cmdBuffer;             // Buffer for 4 recent arp inervals CC values (for command detection)
long bufferLife = 2500;                       // Time (ms) after which cmdBuffer starts to clear
long bufferUpdateTime = millis();             // Time of most recent entry in buffer (excluding clear)
//
//
//  SETUP //
void setup() {
  Serial.begin(9600);   // Setup serial connection
  // Initialisation patch
  //   #              CC#                   val (init)            CC name
  ii = 0;   ccs[ii] = 14;   intPatch[ii] =  0;    ccsNames[ii] =  "EG type";
  ii = 1;   ccs[ii] = 16;   intPatch[ii] =  0;    ccsNames[ii] =  "EG attack";
  ii = 2;   ccs[ii] = 19;   intPatch[ii] =  64;   ccsNames[ii] =  "EG release";
  ii = 3;   ccs[ii] = 20;   intPatch[ii] =  0;    ccsNames[ii] =  "trem depth";
  ii = 4;   ccs[ii] = 21;   intPatch[ii] =  0;    ccsNames[ii] =  "trem rate";
  ii = 5;   ccs[ii] = 24;   intPatch[ii] =  0;    ccsNames[ii] =  "ocs lfo rate";
  ii = 6;   ccs[ii] = 26;   intPatch[ii] =  64;   ccsNames[ii] =  "osc lfo depth";    
  ii = 7;   ccs[ii] = 28;   intPatch[ii] =  64;   ccsNames[ii] =  "mod time";
  ii = 8;   ccs[ii] = 29;   intPatch[ii] =  64;   ccsNames[ii] =  "mod depth";
  ii = 9;   ccs[ii] = 30;   intPatch[ii] =  64;   ccsNames[ii] =  "del time";
  ii = 10;  ccs[ii] = 31;   intPatch[ii] =  64;   ccsNames[ii] =  "del depth";
  ii = 11;  ccs[ii] = 33;   intPatch[ii] =  64;   ccsNames[ii] =  "del mix";
  ii = 12;  ccs[ii] = 34;   intPatch[ii] =  64;   ccsNames[ii] =  "rev time";
  ii = 13;  ccs[ii] = 35;   intPatch[ii] =  64;   ccsNames[ii] =  "rev depth";
  ii = 14;  ccs[ii] = 36;   intPatch[ii] =  64;   ccsNames[ii] =  "rev mix";
  ii = 15;  ccs[ii] = 42;   intPatch[ii] =  0;    ccsNames[ii] =  "filter type";
  ii = 16;  ccs[ii] = 43;   intPatch[ii] =  64;   ccsNames[ii] =  "filter cutoff";
  ii = 17;  ccs[ii] = 44;   intPatch[ii] =  0;    ccsNames[ii] =  "filter resonance";
  ii = 18;  ccs[ii] = 45;   intPatch[ii] =  0;    ccsNames[ii] =  "filter sweep depth";
  ii = 19;  ccs[ii] = 46;   intPatch[ii] =  64;   ccsNames[ii] =  "filter sweep rate";
  ii = 20;  ccs[ii] = 53;   intPatch[ii] =  0;    ccsNames[ii] =  "osc type";
  ii = 21;  ccs[ii] = 54;   intPatch[ii] =  0;    ccsNames[ii] =  "osc shape";
  ii = 22;  ccs[ii] = 55;   intPatch[ii] =  0;    ccsNames[ii] =  "osc alt";
  ii = 23;  ccs[ii] = 88;   intPatch[ii] =  0;    ccsNames[ii] =  "mod type";
  ii = 24;  ccs[ii] = 89;   intPatch[ii] =  0;    ccsNames[ii] =  "del type";
  ii = 25;  ccs[ii] = 90;   intPatch[ii] =  0;    ccsNames[ii] =  "rev type";
  ii = 26;  ccs[ii] = 117;  intPatch[ii] =  0;    ccsNames[ii] =  "arp pattern";
  ii = 27;  ccs[ii] = 118;  intPatch[ii] =  0;    ccsNames[ii] =  "arp interval";
  ii = 28;  ccs[ii] = 119;  intPatch[ii] =  20;   ccsNames[ii] =  "arp length";
  // Set MIDI channel
  Serial.print("Press any note on NTS1 to set MIDI channel."); 
  do {
    midiEvent = MidiUSB.read();               // Capture incoming MIDI event
    if (midiEvent.header == 9){               // Check if MIDI event is "note on"
      byte midiChannelTemp = midiEvent.byte1 << 4;  
      midiChannel = midiChannelTemp >> 4;     // Get channel from MIDI event by two bit-shifts
    }
    signaliseBank(onboardLedPin, 4);          // Use signaliseBank function to urge input
  }
  while (midiChannel > 15);                   // Continue until MIDI channel is set    
  blink(onboardLedPin, 3);
  Serial.print("MIDI channel set to ");   
  Serial.println(midiChannel);                // Print  MIDI channel over serial
  // Load bank 0
  loadPatch(activeBank);
  // Fill command buffer with FFs (denoting 'empty')
  for (int ii=0; ii<4; ii++){
  cmdBuffer.unshift(0xFF);
  }
}
//
//
//  MAIN LOOP //
void loop() {
  // Every-loop function calls
  signaliseBank(onboardLedPin, activeBank);   // Signalise active bank
  midiEvent = MidiUSB.read();                 // Read incoming MIDI messages
  CCs2currentPatch(midiEvent);                // Save incoming MIDI CCs (current patch) to SRAM
  midiCommand = readMidiCommands(midiEvent);  // Detect sent "commands"
  // Act on commands - TO DO
  if (midiCommand != 0) {
    Serial.print("Command: ");                // Print command number
    Serial.println(midiCommand);
    if (midiCommand == 1) {                   // Save
      savePatch();
      blink(onboardLedPin, 3);
    }
    if (midiCommand == 2) {                   // Bank
      changeBank();
      loadPatch(activeBank);
      sendPatch();
    }
    if (midiCommand == 3) {                   // Randomise
      randomisePatch();
      sendPatch();
      blink(onboardLedPin, 3);
    }
    if (midiCommand == 4) {                   // Reset patch
      resetPatch();
      sendPatch();
      blink(onboardLedPin, 5);
    }
    midiCommand = 0;                          // Clear
  }
  //
  MidiUSB.flush();                            // Flush MIDI USB
}
//
//
//  FUNCIONS //
// Function - blink
void blink(int pin, int n) {
  // Blinks n times on pin (max brightness)
    for (int ii = 0 ; ii<n ; ii++) {
    digitalWrite(pin, LOW);                   // LED off
    delay(20);                                // Wait 20 ms    
    digitalWrite(pin, HIGH);                  // LED on
    delay(50);                                // Wait 50 ms
  }
  digitalWrite(pin, LOW);                     // LED off
}
//
// Function - signaliseBank
void signaliseBank(int pin, int bankNumber) {
  /* Signalizes current bank using on-board LED (on/slow blink/medium blink/fast blink).
  Can also be used to urge input by passing 4 as bankNumber.
  Inputs:
    pin - on-board LED pin
    bankNumber - bank number to indicate (0-3)
  Global variables required:
    ledStatus - current on-board LED status (true for on, false for off)
    ledUpdateTime - "relative time" since on-board LED status was updated
  */
  // Function constants and variables:
  byte brightness = 64;                       // PWM value to use for blinks (determines brightness)
  int fastBlinkDuration = 250;                // Duration of fast blink (half of full cycle) in ms
  int mediumBlinkDuration = 500;              // Duration of medium blink (half of full cycle) in ms
  int slowBlinkDuration = 1000;               // Duration of slow blink (half of full cycle) in ms
  int urgeBlinkDuration = 100;                // Duration of urge-input (half of full cycle) in ms
  long timeNow = millis();                    // Get current "relative time" (ms since Arduino was turned on)
  // Change ledStatus according to bank number to signalize
  if (bankNumber==0){                         // Bank 0 (LED constantly on)
    ledStatus = 1;                            
  }
  else if (bankNumber==1){                    // Bank 1 (fast blink)
    if (timeNow - ledUpdateTime > fastBlinkDuration) {
      ledStatus = !ledStatus;                 // If enough time has passed flip the led status
      ledUpdateTime = millis();               // Update time
    }
  }
  else if (bankNumber==2){                    // Bank 2 (medium blink)
    if (timeNow - ledUpdateTime > mediumBlinkDuration) {
      ledStatus = !ledStatus;                 // If enough time has passed flip the led status
      ledUpdateTime = millis();               // Update timer
    }
  }
  else if (bankNumber==3){                    // Bank 3 (slow blink)
    if (timeNow - ledUpdateTime > slowBlinkDuration) {
      ledStatus = !ledStatus;                 // If enough time has passed flip the led status
      ledUpdateTime = millis();               // Update timer
    }
  }
  else if (bankNumber==4){                    // Urge input (fast, bright blinks)
    if (timeNow - ledUpdateTime > urgeBlinkDuration) {
      brightness = 128;                       // Override brightness;
      ledStatus = !ledStatus;                 // If enough time has passed flip the led status
      ledUpdateTime = millis();               // Update timer   
    }
  }
  // Turn LED on/off according to assigned status
    if (ledStatus) {
    analogWrite(pin, brightness);             // Turn LED on
  }
    else if (!ledStatus) {
    digitalWrite(pin, 0);                     // Turn LED of
  }
}
//
// Function - loadPatch
void loadPatch(int bank){
 /*
 Loads patch of active bank from EEPROM to SRAM
 Needs currentPatch global variable (array)
 */
  Serial.print("Loading patch from bank ");
  Serial.println(bank);
  for (ii = 0; ii < 29; ii++){
    currentPatch[ii] = EEPROM.read(bank*eepromPerBankOffset + ii);   // Load from EEPROM to RAM
  }
  sendPatch();                  // Send patch as set of MIDI CCs
}
//
//
// Function - sendPatch
void sendPatch(){
  // Sends current patch over MIDI as set of MIDI CCs
  // Current patch should be stored in currentPatch global variable
  for (int ii=0; ii<27; ii++){
    controlChange(midiChannel, ccs[ii], currentPatch[ii]);// Send MIDI CC
    MidiUSB.flush();                                      // Flush MIDI USB
    Serial.println(""); 
    Serial.println("Sending CC:");
    Serial.println(ccs[ii]); 
    Serial.println(ccsNames[ii]);
    Serial.println("Value:");
    Serial.println(currentPatch[ii]);      
  }
}
//
// Function - CCs2currentPatch
void CCs2currentPatch(midiEventPacket_t me){
/* Saves MIDI CCs to current patch in SRAM
  Inputs:
    me - MIDIusb midiEventPacket
  Global variables required:
    midiChannel - MIDI channel to listen
    ccs - array of relevant CC numbers stored in currentPatch array
    ccsNames - names corresponding to CC numbers in currentPatch
    currentPatch - current patch stored in SRAM
*/
  if (me.header == 0xB) {                   // Check if CC
    byte chnl = me.byte1 << 4;              // Get channel from MIDI message by two bit-shifts
    chnl = chnl >> 4;
    if (chnl == midiChannel) {              // Check if CC comes from right MIDI channel
      int ii = findInCcs(me.byte2);         // Find index (in ccs list) of CC number
      // Print via serial what CC was received
      Serial.print("Received MIDI CC ");
      Serial.print(me.byte2);
      Serial.print(" (");
      Serial.print(ccsNames[ii]);
      Serial.print(")");
      Serial.print("; value: ");
      Serial.print(me.byte3);
      Serial.print(" on MIDI channel ");
      Serial.println(chnl);
      currentPatch[ii] = me.byte3;           // Write to currentPatch global variable
    }
  }
}
//
// Function - findInCCs
int findInCcs(byte ccn){
/* Looks for given CC number (ccn) in array of cc numbers (ccs global variable) */
  int idx = -1;
  for (ii = 0; ii < sizeof(ccs)/sizeof(ccs[0]); ii++){
    if (ccn == ccs[ii]){
      idx = ii;
      break;
      }  
  }
  return idx;
  if (idx == -1) Serial.println("Value not found.");
}
//
// Function: readMidiCommands
int readMidiCommands(midiEventPacket_t me) {
  /* Detects commands from NTS1 sent as sequence of MIDI CCs (arp intervals)
  Input:
    me - MIDIusb midiEventPacket 
  Output:
    command - detected command as int (0-none, 1-)
  This function is intended to use with following global variables:
    midiChannel - MIDI channel to listen
    commandBuffer - circular buffer to store recent inputs (arp intervals CC values)
    bufferLife - time (ms) after which cmdBuffer starts to clear
    bufferUpdateTime - time of most recent entry in buffer (excluding clear)
  */
  int command = 0;
  if (me.header == 0xB && me.byte2 == 0x76) { // Check "me" if arp-pattern MIDI CC message
    cmdBuffer.unshift(me.byte3);              // Write 3rd byte of me to buffer
    Serial.print(cmdBuffer[0], HEX);
    Serial.print("-");
    Serial.print(cmdBuffer[1], HEX);
    Serial.print("-");
    Serial.print(cmdBuffer[2], HEX);
    Serial.print("-");
    Serial.println(cmdBuffer[3], HEX);
    bufferUpdateTime = millis();               // Save time most recent value was saved
  }
  long tnow = millis();                        // Get time now
  if (tnow-bufferUpdateTime > bufferLife){     // More time than allowed elapsed
    cmdBuffer.unshift(0xFF);                   // Fill buffer with FFs
    bufferUpdateTime = millis();               // Reset timer
    Serial.print(cmdBuffer[0], HEX);
    Serial.print("-");
    Serial.print(cmdBuffer[1], HEX);
    Serial.print("-");
    Serial.print(cmdBuffer[2], HEX);
    Serial.print("-");
    Serial.println(cmdBuffer[3], HEX);
  }
    // Detect sequences
  if (cmdBuffer[0]==0x15 && cmdBuffer[1]==0x0 && cmdBuffer[2]==0x15) {
    Serial.println("1/2/1/2 sequence detected");
    command = 1;
    for (int i=1;i<4;i++){cmdBuffer.unshift(0xFF);}   // Clear buffer
  }
  if (cmdBuffer[0]==0x54 && cmdBuffer[1]==0x7F && cmdBuffer[2]==0x54) {
    Serial.println("6/5/6/5 sequence detected");
    command = 2;
    for (int i=1;i<4;i++){cmdBuffer.unshift(0xFF);}   // Clear buffer
  }
  if (cmdBuffer[0]==0x7F && cmdBuffer[1]==0x0 && cmdBuffer[2]==0x7F) {
    Serial.println("1/6/1/6 sequence detected");
    command = 3;
    for (int i=1;i<4;i++){cmdBuffer.unshift(0xFF);}   // Clear buffer
  }
  if (cmdBuffer[0]==0x3F && cmdBuffer[1]==0x2A && cmdBuffer[2]==0x15) {
    Serial.println("1/2/3/4 sequence detected");
    command = 4;
    for (int i=1;i<4;i++){cmdBuffer.unshift(0xFF);}   // Clear buffer
  }
return command;
}
//
// Function - savePatch
void savePatch(){
  /* Saves current patch to active bank
  Needs following global variables:
    activeBank 
    currentPatch - array of cc values of current patch
  Needs following global constants:
    eepromPerBankOffset - offset in EEPROM memory per one bank
  */
  Serial.print("Saving patch to bank ");
  Serial.println(activeBank);
  for (ii = 0; ii < 29; ii++){
    int t1 = activeBank*eepromPerBankOffset + ii;   // Determine EEPROM address to save to
    EEPROM.write(t1, currentPatch[ii]);             // Write to EEPROM
  }
}
//
// Function - changeBank
void changeBank() {
  /* Changes bank in range from 0 to 3.
  Requires following global variables:
    activeBank
  */
  activeBank = activeBank + 1;            // Increment
  if (activeBank == 4) {activeBank = 0;}  // Loop after reaching 4
  Serial.print("Active bank: ");  
  Serial.println(activeBank);
}
//
// Function - randomisePatch
void randomisePatch() {
  /* Randomises CC values of current patch without saving to EEPROM.
  Needs following global variables:
    currentPatch - array of cc values of current patch
  */
  Serial.println("Generating random patch");
  for (ii = 0; ii < 29; ii++){
    currentPatch[ii] = random(0, 127);
  }
}
//
// Function - reset patch
void resetPatch() {
  /* Resets current patch (in SRAM) to default values
  Needs following global variables:
    currentPatch - array of cc values of current patch
    intPatch - cc values of initialisation patch
  */
  for (ii = 0; ii < 29; ii++){
    currentPatch[ii] = intPatch[ii];
  }
}
//
// Function - controlChange (MIDIusb)
void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}
