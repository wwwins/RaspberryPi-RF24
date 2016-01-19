#!/usr/bin/env python

from __future__ import print_function
import time
import sys
from RF24 import *

radio = RF24(RPI_BPLUS_GPIO_J8_15, RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ)

count_of_write = 0
count_of_read = 0

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
  global count_of_write
  radio.stopListening()
  count_of_write = count_of_write+1
  radio.write(msg)
  print('Send Response:',msg)
  radio.startListening()

def main():
  global count_of_read
  while 1:
    while radio.available():
      count_of_read = count_of_read+1
      receive_payload = radio.read(32)
      print('Received:',receive_payload.decode('utf-8'))
      sendCallback(receive_payload)
      time.sleep(0.1)

setup()

try:
  if __name__ == "__main__":
    main()

finally:
  print("\n=================================================")
  print("Receive:",count_of_read,"Response:",count_of_write)
  print("=================================================")
