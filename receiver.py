#!/usr/bin/env python

from __future__ import print_function
import time
import sys
from RF24 import *

radio = RF24(RPI_BPLUS_GPIO_J8_15, RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ)

def setup():
  print('<<receiver>>')
  pipes = [0xABCDABCD71, 0x544d52687C]
  radio.begin()
  radio.setChannel(1)
  radio.setPALevel(RF24_PA_MIN)
  radio.setDataRate(RF24_250KBPS)
  radio.setAutoAck(1)
  radio.setRetries(15, 15)
  radio.setCRCLength(RF24_CRC_16)
  radio.printDetails()

  radio.openWritingPipe(pipes[1])
  radio.openReadingPipe(1, pipes[0])
  radio.startListening()

def sendCallback(msg):
  radio.stopListening()
  radio.write(msg)
  print('Send Response:',msg)
  radio.startListening()



setup()

while 1:
  while radio.available():
    receive_payload = radio.read(32)
    print('Received:',receive_payload.decode('utf-8'))
    sendCallback(receive_payload)
    time.sleep(0.1)
