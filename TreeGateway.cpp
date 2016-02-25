
/*
* TreeGateway Service
* This is a generic tool for master nodes running RF24Mesh that will display address
* assignments, and information regarding incoming data, regardless of the specific
* configuration details.
*
* Requirements: NCurses
* Install NCurses: apt-get install libncurses5-dev
* Setup:
* 1: make
* 2: sudo ./TreeGateway
*
* Usage:
* nc localhost 8888
* TreeGateway Daemon v1.0
* INFO
* 4 04 1
* 2 044 1
* 1 03 0
*
* NOTE: DEBUG MUST BE DISABLED IN RF24Mesh_config.h
*
* Once configured and running, the interface will display the header information, data rate,
* and address assignments for all connected nodes.*
* The master node will also continuously ping each of the child nodes, one per second, while indicating
* the results.
*
*/

#include <stdio.h>
#include <cstring>
#include <iostream>
#include <ncurses.h>
#include <vector>

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

//#define DEBUG_SOCKET
#define PORT "8888"
#define MAX_BUFFER_SIZE 255

using namespace std;

RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);
RF24Network network(radio);
RF24Mesh mesh(radio,network);

void printNodes(uint8_t boldID);
bool pingNode(uint8_t listNo);

void *get_in_addr(struct sockaddr *sa);

// for appending string
int bytes_added( int result_of_sprintf );

uint8_t nodeCounter;
uint16_t failID = 0;

// ping node status values
uint8_t connected[MAX_BUFFER_SIZE];
struct payload_beacon {
  int8_t nodeID;
  uint8_t major;
  uint8_t minor;
  float distance;
};
// find vector container nodeID
int8_t findNodeID(vector<payload_beacon>& vectorContainer,int8_t nodeID);

int main()
{
  /////////////////////////////////////////////////////
  // a simple server
  char version[] = "TreeGateway Daemon v1.0 \r\n";
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
  if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
    #if DEBUG_SOCKET
    fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
    #endif
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

  // init ping status values
  memset(connected,0,MAX_BUFFER_SIZE);
  // init vector container
  vector<payload_beacon> vectorContainer(32);
  for (size_t n = 0; n < vectorContainer.size(); n++) {
    vectorContainer[n].nodeID = -1;
  }
  char buf_nodeID[32];
  char buf_major[32];
  char buf_minor[32];
  char buf_distance[32];
  char *pChar[] = {buf_nodeID,buf_major,buf_minor,buf_distance};

  // title for output
  uint8_t yCoord = 19;

  printf("Establishing mesh...\n");
  mesh.setNodeID(0);
  mesh.begin();
  radio.printDetails();

  initscr();      /* Start curses mode       */
  start_color();
  curs_set(0);
  //keypad(stdscr, TRUE); //Enable user interaction
  init_pair(1, COLOR_GREEN, COLOR_BLACK);
  init_pair(2, COLOR_RED, COLOR_BLACK);
  attron(COLOR_PAIR(1));
  printw("TreeGateway by isobar - 2016\n");
  printw("Base on RF24Mesh Master Node Monitoring Interface by TMRh20 - 2014 2015\n");
  mvprintw(18,0,"[Output]\n");
  attroff(COLOR_PAIR(1));
  refresh();      /* Print it on to the real screen */

  uint32_t kbTimer = 0,kbCount = 0, pingTimer=millis();
  //std::map<char,uint16_t>::iterator it = mesh.addrMap.begin();
  unsigned long totalPayloads = 0;

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
            #if DEBUG_SOCKET
            mvprintw(yCoord++,0,"selectserver: new connection from %s on socket %d\n",inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr),remoteIP, INET6_ADDRSTRLEN),newfd);
            #endif
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
              #if DEBUG_SOCKET
              mvprintw(yCoord++,0,"selectserver: socket %d hung up\n", i);
              #endif
            } else {
              perror("recv");
            }
            close(i); // bye!
            FD_CLR(i, &master); // remove from master set
          } else {
            // send back
            buf[nbytes] = '\0';
            #if DEBUG_SOCKET
            mvprintw(yCoord++,0,"Received: %s\n",buf);
            #endif
            char output[MAX_BUFFER_SIZE] = "Echo:";
            // if (strcmp(buf,"INFO\n")==0) {
            if (strncmp(buf,"INFO",4)==0) {
              int length = 0;
              int at;
              for(uint8_t j=0; j<mesh.addrListTop; j++) {
                at = findNodeID(vectorContainer, mesh.addrList[j].nodeID);
                // format: nodeID address connected distance
                length += bytes_added(sprintf(output+length, "%d 0%o %d %f\n",mesh.addrList[j].nodeID,mesh.addrList[j].address,connected[j],at < 0 ? 0.0 : vectorContainer[at].distance));
              }
              at = findNodeID(vectorContainer,0);
              length += bytes_added(sprintf(output+length, "0 00 1 %f\n",(at < 0 ? 0.0 :vectorContainer[at].distance)));
            }
            // DIST nodeID Major Minor Distance
            else if (strncmp(buf,"DIST",4)==0) {
              uint8_t idx = 0;
              const char delim[] = " ";
              char *token;
              token = strtok(buf, delim);
              // parsing char array
              while( token != NULL ) {
                if (idx>0) strcpy(pChar[idx-1],token);
                idx++;
                token = strtok(NULL, delim);
              }
              payload_beacon obj;
              bool updated = false;
              obj.nodeID = (uint8_t)atoi(pChar[0]);
              obj.major = (uint8_t)atoi(pChar[1]);
              obj.minor = (uint8_t)atoi(pChar[2]);
              obj.distance = (float)atof(pChar[3]);
              if (obj.nodeID > -1) {
                for (size_t n = 0; n < vectorContainer.size(); n++) {
                  if (vectorContainer[n].nodeID==obj.nodeID) {
                    vectorContainer[n].major = obj.major;
                    vectorContainer[n].minor = obj.minor;
                    vectorContainer[n].distance = obj.distance;
                    updated = true;
                  }
                }
                if (!updated) {
                  vectorContainer.push_back(obj);
                }
              }
            }
            else if (strncmp(buf,"DUMP",4)==0) {
              // print all
              for (size_t n = 0; n < vectorContainer.size(); n++) {
                if (vectorContainer[n].nodeID > -1) {
                  mvprintw(yCoord++,0,"nodeID:%d, major:%d, minor:%d, distanc:%f\n",vectorContainer[n].nodeID,vectorContainer[n].major,vectorContainer[n].minor,vectorContainer[n].distance);
                }
              }
            }
            else {
              strcat(output,buf);
            }

            if (send(i,output, strlen(output), 0) == -1) {
              perror("send");
            }

          }
        } // END handle data from client
      } // END got new incoming connection
    } // END looping through file descriptors
    /////////////////////////////////////////////////////

    // Call mesh.update to keep the network updated
    mesh.update();
    // In addition, keep the 'DHCP service' running on the master node so addresses will
    // be assigned to the sensor nodes
    mesh.DHCP();
    // Wait until a sensor node is connected
    if(sizeof(mesh.addrList) < 1){continue; }

    // Check for incoming data from the sensors
    while(network.available()) {
      RF24NetworkHeader header;
      network.peek(header);

      uint8_t boldID = 0;

      // Print the total number of received payloads
      mvprintw(10,0," Total: %lu\n",totalPayloads++);

      kbCount++;

      attron(A_BOLD | COLOR_PAIR(1));
      mvprintw(3,0,"[Last Payload Info]\n");
      attroff(A_BOLD | COLOR_PAIR(1));

      bool updated = false;
      payload_beacon payload;
      switch(header.type) {
        // Display the incoming millis() values from the sensor nodes
        case 'B': network.read(header,&payload,sizeof(payload));
                  if (payload.nodeID > -1) {
                    for (size_t n = 0; n < vectorContainer.size(); n++) {
                      if (vectorContainer[n].nodeID==payload.nodeID) {
                        vectorContainer[n].major = payload.major;
                        vectorContainer[n].minor = payload.minor;
                        vectorContainer[n].distance = payload.distance;
                        updated = true;
                      }
                    }
                    if (!updated) {
                      vectorContainer.push_back(payload);
                    }
                  }
                  break;
        // Read the network payload
        default:  network.read(header,0,0);
                  mvprintw(4,0," HeaderID: %u  \n Type: %d  \n From: 0%o  \n ",header.id,header.type,header.from_node);
                  break;
      }

      //refresh();
      //for (std::map<char,uint16_t>::iterator _it=mesh.addrMap.begin(); _it!=mesh.addrMap.end(); _it++){
      for(uint8_t i=0; i<mesh.addrListTop; i++){
        if(header.from_node == mesh.addrList[i].address){
          boldID = mesh.addrList[i].nodeID;
        }
      }
      printNodes(boldID);

    }
    //refresh();

    if(millis()-kbTimer > 1000 && kbCount > 0) {
      kbTimer = millis();
      attron(A_BOLD | COLOR_PAIR(1));
      mvprintw(8,0,"[Data Rate (In)]");
      attroff(A_BOLD | COLOR_PAIR(1));
      mvprintw(9,0," Kbps: %.2f",(kbCount * 32 * 8)/1000.00);
      kbCount = 0;

    }

    // Ping each connected node, one per second
    if(millis()-pingTimer>1003 && sizeof(mesh.addrList) > 0) {
      pingTimer=millis();
      if(  nodeCounter == mesh.addrListTop){ // if(mesh.addrMap.size() > 1){ it=mesh.addrMap.begin(); } continue;}
        nodeCounter = 0;
      }
      connected[nodeCounter] = pingNode(nodeCounter);
      nodeCounter++;
    }

    /*uint32_t nOK,nFails;
    network.failures(&nFails,&nOK);
    attron(A_BOLD | COLOR_PAIR(1));
      mvprintw(2,24,"[Transmit Results] ");
      attroff(A_BOLD | COLOR_PAIR(1));
    mvprintw(3,25," #OK: %u   ",nOK);
    mvprintw(4,25," #Fail: %u   ",nFails);*/


    refresh();
    delay(2);
  }//while 1

  endwin();      /* End curses mode      */
  return 0;
}


void printNodes(uint8_t boldID){

  ////////////////////////////////////////
  char buf[32];
  char buf_address[32];
  char substr[32];
  char output[256];
  ////////////////////////////////////////

  uint8_t yCoord = 3;
  attron(A_BOLD | COLOR_PAIR(1));
  mvprintw(yCoord,27,"[Address Assignments]\n");
  mvprintw(yCoord++,60,"[Routing Table]\n");
  attroff(A_BOLD | COLOR_PAIR(1));
  //for (std::map<char,uint16_t>::iterator it=mesh.addrMap.begin(); it!=mesh.addrMap.end(); ++it){
  for( uint8_t i=0; i<mesh.addrListTop; i++) {
    memset(buf, 0, 32);
    memset(buf_address, 0, 32);
    memset(substr, 0, 32);
    memset(output, 0, 256);

    //if( failID == it->first){
    if( failID == mesh.addrList[i].nodeID) {
      attron(COLOR_PAIR(2));
    } else if( boldID == mesh.addrList[i].nodeID ) {
      attron(A_BOLD | COLOR_PAIR(1));
    }

    ////////////////////////////////////////
    // convert octal to char.
    sprintf(buf_address,"0%o",mesh.addrList[i].address);
    for (uint j=0;j<strlen(buf_address);j++) {
      if (j==0) {

      }
      else if (j==1) {

      }
      else {
        strncpy(substr,buf_address+strlen(buf_address)-j+1,j);
        // convert char to octal.
        // uint16_t node_address = strtol(substr, NULL, 8);
        // format: address(id)
        sprintf(buf,"%s(%d)",substr,mesh.getNodeID(strtol(substr, NULL, 8)));
        strcat(output,buf);
        strcat(output,"-");
      }
    }
    // format: address(id)
    sprintf(buf,"%s(%d)",buf_address,mesh.addrList[i].nodeID);
    // remove first char
    strcat(output,buf+1);
    ////////////////////////////////////////

    mvprintw(yCoord,28,"ID: %d  Network: 0%o  ",mesh.addrList[i].nodeID,mesh.addrList[i].address);
    mvprintw(yCoord++,61,"0(0)-%s",output);
    attroff(A_BOLD | COLOR_PAIR(1));
    attroff(COLOR_PAIR(2));
  }
  mvprintw(yCoord++,28,"                   ");
  mvprintw(yCoord++,28,"                   ");
  mvprintw(yCoord++,28,"                   ");
}

bool pingNode(uint8_t listNo){

  uint8_t yCoord = 12;
  attron(A_BOLD | COLOR_PAIR(1));
  mvprintw(yCoord++,0,"[Ping Test]\n");
  attroff(A_BOLD | COLOR_PAIR(1));

  RF24NetworkHeader headers(mesh.addrList[listNo].address,NETWORK_PING);
  uint32_t pingtime=millis();
  bool ok = false;
  if(headers.to_node){
    ok = network.write(headers,0,0);
    if(ok && failID == mesh.addrList[listNo].nodeID){ failID = 0; }
    if(!ok){ failID = mesh.addrList[listNo].nodeID; }
  }
  pingtime = millis()-pingtime;
  mvprintw(yCoord++,0," ID:%d    ",mesh.addrList[listNo].nodeID);
  mvprintw(yCoord++,0," Net:0%o    ",mesh.addrList[listNo].address);
  mvprintw(yCoord++,0," Time:%ums       ",pingtime);

  if(ok || !headers.to_node){
    mvprintw(yCoord,0," OK  ");
    return true;
  } else {
    attron(COLOR_PAIR(2));
    //attron(A_BOLD);
    mvprintw(yCoord,0," FAIL");
    attron(A_BOLD);
    return false;
  }
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int bytes_added( int result_of_sprintf ) {
    return (result_of_sprintf > 0) ? result_of_sprintf : 0;
}

int8_t findNodeID(vector<payload_beacon>& vectorContainer,int8_t nodeID) {
  int at = -1;
  for (size_t n = 0; n < vectorContainer.size(); n++) {
    if (vectorContainer[n].nodeID==nodeID) {
      at = n;
    }
  }
  return at;
}
