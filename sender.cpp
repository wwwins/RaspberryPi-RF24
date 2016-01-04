#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <RF24/RF24.h>
#include <unistd.h>

using namespace std;
//
// Hardware configuration
//

/****************** Raspberry Pi ***********************/

// Radio CE Pin, CSN Pin, SPI Speed
// See
// http://www.airspayce.com/mikem/bcm2835/group__constants.html#ga63c029bd6500167152db4e57736d0939
// and the related enumerations for pin information.

// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 4Mhz
// RF24 radio(RPI_V2_GPIO_P1_22, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ);

// NEW: Setup for RPi B+
RF24 radio(RPI_BPLUS_GPIO_J8_15, RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ);

// Setup for GPIO 15 CE and CE0 CSN with SPI Speed @ 8Mhz
// RF24 radio(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ);

/*** RPi Alternate ***/
// Note: Specify SPI BUS 0 or 1 instead of CS pin number.
// See http://tmrh20.github.io/RF24/RPi.html for more information on usage

// RPi Alternate, with MRAA
// RF24 radio(15,0);

// RPi Alternate, with SPIDEV - Note: Edit RF24/arch/BBB/spi.cpp and  set
// 'this->device = "/dev/spidev0.0";;' or as listed in /dev
// RF24 radio(22,0);

/****************** Linux (BBB,x86,etc) ***********************/

// See http://tmrh20.github.io/RF24/pages.html for more information on usage
// See http://iotdk.intel.com/docs/master/mraa/ for more information on MRAA
// See https://www.kernel.org/doc/Documentation/spi/spidev for more information
// on SPIDEV

// Setup for ARM(Linux) devices like BBB using spidev (default is
// "/dev/spidev1.0" )
// RF24 radio(115,0);

// BBB Alternate, with mraa
// CE pin = (Header P9, Pin 13) = 59 = 13 + 46
// Note: Specify SPI BUS 0 or 1 instead of CS pin number.
// RF24 radio(59,0);

/**************************************************************/

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t addresses[2] = {0xABCDABCD71LL, 0x544d52687CLL};

//uint8_t data[32];
char data[32];

bool send(char *msg) {
  radio.stopListening();
  cout << "[Start Sending]:" << msg << endl;
  char buf[32];
  strcpy(buf,msg);
  if (radio.write(&buf, 32)) {
    cout << "[Send OK]" << endl;
  }

  radio.startListening();
  unsigned long started_waiting_at = millis();
  bool timeout = false;
  while (!radio.available() && !timeout) {
    if (millis() - started_waiting_at > 1000 ) {
      timeout = true;
    }
  }
  if (timeout) {
    cout << "[Response]:Timeout" << endl;
    return false;
  }
  else {
    radio.read(&data,32);
    cout << "[Response]:" << data << endl;
    return true;
  }

}

int main(int argc, char **argv) {

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <msg>\n", argv[0]);
    exit(1);
  }

  radio.begin(); // Setup and configure rf radio
  radio.setChannel(1);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  //radio.setAutoAck(1); // Ensure autoACK is enabled
  radio.setRetries(
      15, 15); // Optionally, increase the delay between retries & # of retries
  radio.setCRCLength(RF24_CRC_16); // Use 8-bit CRC for performance
  radio.printDetails();

  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1, addresses[1]);

  send(argv[1]);

} // main
