#include <MIDI.h>
#include <SPI.h>

#define GATE  2
#define DAC1  8 
#define TONE  6

MIDI_CREATE_DEFAULT_INSTANCE();

void setup()
{
 
 pinMode(GATE, OUTPUT);
 pinMode(DAC1, OUTPUT);
 pinMode(TONE, OUTPUT);
 digitalWrite(GATE,LOW);
 digitalWrite(DAC1,HIGH);
 tone(TONE, 440);

 SPI.begin();

 MIDI.begin(MIDI_CHANNEL_OMNI);

}

bool notes[88] = {0}; 
int8_t noteOrder[20] = {0}, orderIndx = {0};
unsigned long trigTimer = 0;
void loop()
{
  int type, noteMsg, velocity, channel, d1, d2;
  
  if (MIDI.read()) {                    
    byte type = MIDI.getType();
    switch (type) {
      case midi::NoteOn: 
      case midi::NoteOff:
        noteMsg = MIDI.getData1() - 21; // A0 = 21, Top Note = 108
        channel = MIDI.getChannel();
        
        if ((noteMsg < 0) || (noteMsg > 87)) break; // Only 88 notes of keyboard are supported

        if (type == midi::NoteOn) velocity = MIDI.getData2();
        else velocity  = 0;  

        if (velocity == 0)  {
          notes[noteMsg] = false;
        }
        else {
          notes[noteMsg] = true;
        }

        if (notes[noteMsg]) 
        {  
         orderIndx = (orderIndx+1) % 20;
         noteOrder[orderIndx] = noteMsg;                 
        }
        commandLastNote();         
        break;
        
      default:
        d1 = MIDI.getData1();
        d2 = MIDI.getData2();
    }
  }
}

void commandLastNote()
{

  int8_t noteIndx;
  for (int i=0; i<20; i++) {
    noteIndx = noteOrder[ mod(orderIndx-i, 20) ];
    if (notes[noteIndx]) {
      commandNote(noteIndx);
      return;
    }
  }
  digitalWrite(GATE,LOW);  // All notes are off
}

// Rescale 88 notes to 4096 mV:
//    noteMsg = 0 -> 0 mV 
//    noteMsg = 87 -> 4096 mV
// DAC output will be (4095/87) = 47.069 mV per note, and 564.9655 mV per octive
// Note that DAC output will need to be amplified by 1.77X for the standard 1V/octave 

#define NOTE_SF 47.069f // This value can be tuned if CV output isn't exactly 1V/octave


void commandNote(int noteMsg) {
  digitalWrite(GATE,HIGH);
  
  unsigned int mV = (unsigned int) ((float) noteMsg * NOTE_SF + 0.5); 
  setVoltage(DAC1, 0, 1, mV);  // DAC1, channel 0, gain = 2X
  
  float exponent = (((float)noteMsg - 69.0f) / 12.0f);
  unsigned int pitch = (unsigned int)(pow(2.0f, exponent) * 440.0f);
  tone(TONE, pitch);
  setVoltage(DAC1, 1, 1, (amplitude(pitch) / 2));
  
}

// setVoltage -- Set DAC voltage output
// dacpin: chip select pin for DAC.  Note and velocity on DAC1, pitch bend and CC on DAC2
// channel: 0 (A) or 1 (B).  Note and pitch bend on 0, velocity and CC on 2.
// gain: 0 = 1X, 1 = 2X.  
// mV: integer 0 to 4095.  If gain is 1X, mV is in units of half mV (i.e., 0 to 2048 mV).
// If gain is 2X, mV is in units of mV

unsigned int amplitude(unsigned int frequency)
{
   float perHz = (4095.0f / 1290.0f); //difference in Hz between highest and lowest notes
   return frequency * perHz;
}

void setVoltage(int dacpin, bool channel, bool gain, unsigned int mV)
{
  unsigned int command = channel ? 0x9000 : 0x1000;

  command |= gain ? 0x0000 : 0x2000;
  command |= (mV & 0x0FFF);
  
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(dacpin,LOW);
  SPI.transfer(command>>8);
  SPI.transfer(command&0xFF);
  digitalWrite(dacpin,HIGH);
  SPI.endTransaction();
}

int mod(int a, int b)
{
    int r = a % b;
    return r < 0 ? r + b : r;
}
