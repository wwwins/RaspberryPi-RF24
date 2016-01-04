#!/usr/bin/env python

#
# Example using Dynamic Payloads
# 
#  This is an example of how to use payloads of a varying (dynamic) size.
# 

from __future__ import print_function
import time
import sys
from RF24 import *


########### USER CONFIGURATION ###########
# See https://github.com/TMRh20/RF24/blob/master/RPi/pyRF24/readme.md

# CE Pin, CSN Pin, SPI Speed

# Setup for GPIO 22 CE and GPIO 25 CSN with SPI Speed @ 1Mhz
#radio = RF24(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_18, BCM2835_SPI_SPEED_1MHZ)

# Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 4Mhz
#radio = RF24(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ)

#RPi B
# Setup for GPIO 15 CE and CE1 CSN with SPI Speed @ 8Mhz
#radio = RF24(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ)

#RPi B+
# Setup for GPIO 22 CE and CE0 CSN for RPi B+ with SPI Speed @ 8Mhz
radio = RF24(RPI_BPLUS_GPIO_J8_15, RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ)

##########################################

millis = lambda: int(round(time.time() * 1000))

def setup():
  #pipes = [0xF0F0F0F0E1, 0xF0F0F0F0D2]
  pipes = [0xABCDABCD71, 0x544d52687C]
  
  print('<<sender>>')
  radio.begin()
  radio.setChannel(1);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  #radio.enableDynamicPayloads()
  radio.setRetries(15,15)
  radio.printDetails()
  
  radio.openWritingPipe(pipes[0])
  radio.openReadingPipe(1,pipes[1])

def send(msg):
  radio.stopListening()
  print("[Start Sending]:",msg)
  radio.write(msg)
  print("[Send OK]")
  radio.startListening();
  started_waiting_at = millis()
  timeout = False
  while (not radio.available()) and (not timeout):
    if (millis() - started_waiting_at) > 1000:
      timeout = True

  if timeout:
    print("[Response]:Timeout")
  else:
    receive_payload = radio.read(32);
    print("[Response]:",receive_payload.decode('utf-8'))


if (len(sys.argv) != 2):
  print("Usage: sender.py <msg>")
  sys.exit()

setup()
send(sys.argv[1])

