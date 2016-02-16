#include "RF24Mesh/RF24Mesh.h"
#include <RF24/RF24.h>
#include <RF24Network/RF24Network.h>

void send(uint16_t node);

RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);
RF24Network network(radio);
RF24Mesh mesh(radio,network);

uint16_t to_node = 0;
unsigned long last_send;
unsigned long packets_sent;

struct payload_t {
  unsigned long ms;
  unsigned long counter;
};

int main(int argc, char** argv) {

  // Set the nodeID to 0 for the master node
  mesh.setNodeID(0);
  // Connect to the mesh
  printf("start\n");
  mesh.begin();
  radio.printDetails();
  last_send = millis();

  while(1) {

    // Call network.update as usual to keep the network updated
    mesh.update();

    // In addition, keep the 'DHCP service' running on the master node so addresses will
    // be assigned to the sensor nodes
    mesh.DHCP();


    // Check for incoming data from the sensors
    while(network.available()) {
      // printf("rcv\n");
      RF24NetworkHeader header;
      network.peek(header);

      uint32_t dat=0;
      switch(header.type) {
        // Display the incoming millis() values from the sensor nodes
        case 'M': network.read(header,&dat,sizeof(dat));
                  printf("Rcv %u from 0%o to 0%o\n",dat,header.from_node,header.to_node);
                  to_node = header.from_node;
                  send(to_node);
                  break;
        default:  network.read(header,0,0);
                  printf("Rcv bad type %d from 0%o to 0%o\n",header.type,header.from_node,header.to_node);
                  break;
      }
    }

    if(millis() - last_send >= 5000) {
      last_send = millis();
      //send(to_node);
    }
    delay(2);
  }
  return 0;
}

void send(uint16_t node) {
  payload_t payload = { millis(), packets_sent++ };
  RF24NetworkHeader header(node);
  bool ok = network.write(header,&payload,sizeof(payload));
  if (ok) {
    printf("Send Ok: back to 0%o.\n", to_node);
  }
  else {
    printf("Send Fail: back to 0%o.\n", to_node);
  }
}
