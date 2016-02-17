 /**
  * RF24 Mesh node by wwwins 2016/02/04
  * Base on https://github.com/TMRh20/RF24Mesh/blob/master/examples_RPi/RF24Mesh_Example.cpp
  *
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "RF24Mesh/RF24Mesh.h"
#include <RF24/RF24.h>
#include <RF24Network/RF24Network.h>

// Set the nodeID to 0 for the master node
#define NODE_ID 1

#if NODE_ID > 0
#define HOST "localhost"
#else
#define HOST NULL
#endif

#define PORT "8888"
#define MAX_BUFFER_SIZE 255

void *get_in_addr(struct sockaddr *sa);

RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);
RF24Network network(radio);
RF24Mesh mesh(radio,network);

uint32_t displayTimer=0;

struct payload_t {
  unsigned long ms;
  unsigned long counter;
};

int main(int argc, char** argv) {

  /////////////////////////////////////////////////////
  // a simple server
  char version[] = "TreeNode Daemon v1.0 \r\n";
  fd_set master;    // master file descriptor list
  fd_set read_fds;  // temp file descriptor list for select()
  int fdmax;        // maximum file descriptor number

  int listener;     // listening socket descriptor
  int newfd;        // newly accept()ed socket descriptor
  struct sockaddr_storage remoteaddr; // client address
  socklen_t addrlen;

  char buf[MAX_BUFFER_SIZE+1];    // buffer for client data
  int nbytes;

  char remoteIP[INET6_ADDRSTRLEN];

  int yes=1;        // for setsockopt() SO_REUSEADDR, below
  int i, rv;
  //int j;

  struct addrinfo hints, *ai, *p;
  struct timeval tv;
  tv.tv_sec = 2;
  tv.tv_usec = 500000;

  FD_ZERO(&master);    // clear the master and temp sets
  FD_ZERO(&read_fds);

  // get us a socket and bind it
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((rv = getaddrinfo(HOST, PORT, &hints, &ai)) != 0) {
    fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
    exit(1);
  }

  for(p = ai; p != NULL; p = p->ai_next) {
    listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (listener < 0) {
      continue;
    }

    // lose the pesky "address already in use" error message
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
      close(listener);
      continue;
    }

    break;
  }

  // if we got here, it means we didn't get bound
  if (p == NULL) {
    fprintf(stderr, "selectserver: failed to bind\n");
    exit(2);
  }

  freeaddrinfo(ai); // all done with this

  // listen
  if (listen(listener, 10) == -1) {
      perror("listen");
      exit(3);
  }

  // add the listener to the master set
  FD_SET(listener, &master);

  // keep track of the biggest file descriptor
  fdmax = listener; // so far, it's this one
  /////////////////////////////////////////////////////

  /////////////////////////////////////////////////////
  // Set the nodeID to 0 for the master node
  mesh.setNodeID(NODE_ID);
  // Connect to the mesh
  printf("start nodeID %d\n",mesh.getNodeID());
  mesh.begin();
  radio.printDetails();
  /////////////////////////////////////////////////////

  while(1) {
    /////////////////////////////////////////////////////
    // simple server main loop
    read_fds = master; // copy it
    if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
      perror("select");
      exit(4);
    }

    // run through the existing connections looking for data to read
    for(i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &read_fds)) { // we got one!!
        if (i == listener) {
          // handle new connections
          addrlen = sizeof remoteaddr;
          newfd = accept(listener,(struct sockaddr *)&remoteaddr,&addrlen);

          if (newfd == -1) {
            perror("accept");
          } else {
            FD_SET(newfd, &master); // add to master set
            if (newfd > fdmax) {    // keep track of the max
                fdmax = newfd;
            }
            printf("selectserver: new connection from %s on socket %d\n",inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr),remoteIP, INET6_ADDRSTRLEN),newfd);
            if (send(newfd, version, strlen(version), 0) == -1) {
              perror("send");
            }
          }
        } else {
          // handle data from a client
          if ((nbytes = recv(i, buf, MAX_BUFFER_SIZE, 0)) <= 0) {
            // got error or connection closed by client
            if (nbytes == 0) {
              // connection closed
              printf("selectserver: socket %d hung up\n", i);
            } else {
              perror("recv");
            }
            close(i); // bye!
            FD_CLR(i, &master); // remove from master set
          } else {
            // send back
            buf[nbytes] = '\0';
            printf("Received: %s\n",buf);
            char output[512] = "Echo:";
            strcat(output,buf);
            if (send(i,output, strlen(output), 0) == -1) {
              perror("send");
            }

            // we got some data from a client
            /*
            for(j = 0; j <= fdmax; j++) {
              // send to everyone!
              if (FD_ISSET(j, &master)) {
                // except the listener and ourselves
                if (j != listener && j != i) {
                  if (send(j, buf, nbytes, 0) == -1) {
                    perror("send");
                  }
                }
              }
            }*/
          }
        } // END handle data from client
      } // END got new incoming connection
    } // END looping through file descriptors
    /////////////////////////////////////////////////////

    /////////////////////////////////////////////////////
    // Call mesh.update to keep the network updated
    mesh.update();

    // Send the current millis() to the master node every second
    if(millis() - displayTimer >= 1000) {
      displayTimer = millis();

      if(!mesh.write(&displayTimer,'M',sizeof(displayTimer))) {
        // If a write fails, check connectivity to the mesh network
         if( ! mesh.checkConnection() ){
          // The address could be refreshed per a specified timeframe or only when sequential writes fail, etc.
          printf("Renewing Address\n");
          mesh.renewAddress();
        }
        else {
            printf("Send fail, Test OK\n");
        }
      } else {
        printf("Send OK: %u\n",displayTimer);
      }
    }

    while ( network.available() ) {
      RF24NetworkHeader header;
      payload_t payload;
      network.read(header,&payload,sizeof(payload));
      printf("Received payload # %lu at %lu \n",payload.counter,payload.ms);
    }
    /////////////////////////////////////////////////////

  delay(1);
  }

  return 0;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
