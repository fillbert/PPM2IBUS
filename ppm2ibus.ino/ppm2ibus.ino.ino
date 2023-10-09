/*
 Name:		ppm_to_ibus_serial.ino
 Created:	10/09/2023
 Author:	filbertby
*/

#define IBUS_FRAME_LENGTH 0x20                                  // iBus packet size (2 byte header, 14 channels x 2 bytes, 2 byte checksum)
#define IBUS_COMMAND40    0x40                                  // Command is always 0x40
#define IBUS_MAXCHANNELS  14                                    // iBus has a maximum of 14 channels

#define IBUS_DEFAULT_VALUE (uint16_t)1500

#define PPM_LOW_SEPARATOR (uint16_t)2100                        // Anything higher than this value is considered to be the pulse separator

#define PPM_MAX_CHANNELS      8                                 // Should be no more than 10

volatile uint16_t ppm_channel_data[PPM_MAX_CHANNELS] = { 0 };
volatile uint8_t  ppm_pulse_index   = 0;
volatile uint8_t  ppm_channel_count = PPM_MAX_CHANNELS;
volatile uint8_t  ppm_num_off_channels = 0;

byte serial_buffer[IBUS_FRAME_LENGTH] = { 0 };
int buffer_index = 0;

bool hasBeenUpdated = false;

// the setup function runs once when you press reset or power the board
void setup() {

	PCICR  |= (1 << PCIE0);
	PCMSK0 |= (1 << PCINT0);
  pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(115200);
}

void WriteSerial()
{
  // whlte ready flag
  if (!hasBeenUpdated) 
    return;
  hasBeenUpdated = false;

	uint16_t ibus_checksum = ((uint16_t)0xFFFF);
  buffer_index = 0;

  // Write the IBus buffer length
  serial_buffer[buffer_index++] = (byte)IBUS_FRAME_LENGTH;
  // Write the IBus Command 0x40
  serial_buffer[buffer_index++] = (byte)IBUS_COMMAND40;

  // Write the IBus channel data to the buffer
  for (int i = 0; i < min(ppm_channel_count, IBUS_MAXCHANNELS); i++) {
    noInterrupts();
      serial_buffer[buffer_index++] = (byte)(ppm_channel_data[i] & 0xFF);
      serial_buffer[buffer_index++] = (byte)((ppm_channel_data[i] >> 8) & 0xFF);
    interrupts();
  }

  // Fill the remaining buffer channels with the default value
  if (ppm_channel_count < IBUS_MAXCHANNELS) {
    for (int i = 0; i < IBUS_MAXCHANNELS - ppm_channel_count; i++) {
      serial_buffer[buffer_index++] = (byte)(IBUS_DEFAULT_VALUE & 0xFF);
      serial_buffer[buffer_index++] = (byte)((IBUS_DEFAULT_VALUE >> 8) & 0xFF);
    }
  }

  // Calculate the IBus checksum
  for (int i = 0; i < buffer_index; i++) {
    ibus_checksum -= (uint16_t)serial_buffer[i];
  }

  // Write the IBus checksum to the buffer
  serial_buffer[buffer_index++] = (byte)(ibus_checksum & 0xFF);
  serial_buffer[buffer_index++] = (byte)((ibus_checksum >> 8) & 0xFF);

  // Write the buffer to the Serial pin
  Serial.write(serial_buffer, buffer_index);
}

// the loop function runs over and over again until power down or reset
void loop() {

	// Write the IBus data to the Serial Port
	WriteSerial();

	// Delay before next update
	//delay(10);
}

ISR(PCINT0_vect) {
  static unsigned long lastTimeStamp = 0;
  // get only low to hi edge
	if (PINB & B00000001) {
    // get current tipestamp
    unsigned long currentTimeStamp = micros();
    // not first time
    if (0 != lastTimeStamp) {
        uint16_t width = (uint16_t)(currentTimeStamp - lastTimeStamp);
        if(width > PPM_LOW_SEPARATOR){
          ppm_num_off_channels = ppm_pulse_index;
          ppm_pulse_index = 0;
        }
        else {
          ppm_channel_data[ppm_pulse_index++] = width;
          if(ppm_num_off_channels == ppm_pulse_index)
            hasBeenUpdated = true;
        }
      }
    lastTimeStamp = currentTimeStamp;
	}
}