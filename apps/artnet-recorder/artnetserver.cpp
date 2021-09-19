#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "artnetserver.h"
#include "recorder.h"

#define MAXPACKET 10000

#define FIRST_LOCAL_UNIVERSE 0
#define LAST_LOCAL_UNIVERSE 255

struct ArtnetDmxHeader {
    char identifier[8];
    unsigned short opcode;
    unsigned short protver;
    unsigned char sequence;
    unsigned char physical;
    unsigned char universe;
    unsigned char universe2;
    unsigned short datalength;
};

void ArtNetServer::touchUniverse(uint32_t universe) {
    uint32_t T = (uint32_t)time(NULL);

    bool found = false;
    for(int u=0; u<this->universeStats.size(); u++) {
        UniverseStat *st = &this->universeStats[u];
        if (st->universe == universe) {
            st->lastActivity = T;
            st->numUpdates ++;
            found = true;
        }
    }
    if (!found) {
        UniverseStat newst;
        newst.universe = universe;
        newst.lastActivity = T;
        newst.numUpdates = 1;
        this->universeStats.push_back(newst);
    }
}

void ArtNetServer::parsePacket(uint8_t *bytes, uint32_t length) {
    this->totalBytes += length;
    this->totalPackets ++;

    if (length < 100) {
        this->totalInvalidPackets ++;
        return;
    }

    struct ArtnetDmxHeader *h1 = (struct ArtnetDmxHeader *)bytes;

    if (memcmp(h1->identifier, "Art-Net", 7) != 0) {
        this->totalInvalidPackets ++;
        return;
    }

    h1->opcode = htons(h1->opcode);
    h1->protver = htons(h1->protver);
    h1->datalength = htons(h1->datalength);

    if (h1->opcode == 0x52) {
        // artnet sync
        // printf("Received unknown ArtNet opcode (%02X)\n", h1->opcode);
        this->recorder->handleSyncCommand();
        return;
    }

    if (h1->opcode != 0x50) {
        // not dmx512 data
        printf("Received unknown ArtNet opcode (%02X)\n", h1->opcode);
        this->totalIgnoredPackets ++;
        return;
    }

    int32_t universe = h1->universe; //  - (int32_t) this->config->firstUniverse;
    touchUniverse(universe);

     printf("got artnet: identifier \"%s\", opcode=%x, protover=%x, universe=%d (local %d), datalength=%d\n",
         h1->identifier,
         h1->opcode,
         h1->protver,
         h1->universe,
         universe,
         h1->datalength);

    if (universe < FIRST_LOCAL_UNIVERSE || universe > LAST_LOCAL_UNIVERSE) {
        // wrong universe, pal!
        this->totalIgnoredPackets ++;
        return;
    }

    int numBytes = 512;

    uint8_t *inptr = bytes + sizeof(struct ArtnetDmxHeader);

    this->recorder->handleDmxDataCommand(universe, inptr, numBytes);
}

ArtNetServer::ArtNetServer(uint16_t port, Recorder *recorder) {
    this->port = port;
    this->recorder = recorder;
}

ArtNetServer::~ArtNetServer() {
}

const char *ageString(int age) {
    if (age < 2) {
        return "act!";
    }
    if (age < 10) {
        return "slow";
    }
    return "idle";
}

void ArtNetServer::printStatistics() {
    uint32_t T = (uint32_t)time(NULL);

    printf("ArtNet Statistics:\n");
    printf("  %ld Packets received (%ld ignored, %ld invalid).\n", this->totalPackets, this->totalIgnoredPackets, this->totalInvalidPackets);
    printf("  %ld Bytes received.\n", this->totalBytes);
    printf("  Universes received:\n");

    bool found = false;
    for(int u=0; u<this->universeStats.size(); u++) {
        UniverseStat *st = &this->universeStats[u];
        int age = T-st->lastActivity;
        if (age > 100) age = 100;
        if (u % 4 == 0 && u > 0) {
            printf("\n");
        }
        printf("  \tu%03d %s #%-6d", st->universe, ageString(age), st->numUpdates);
    }
    printf("\n\n");
}

void ArtNetServer::run() {
    int sockfd;
    fd_set readfds;
    uint8_t buffer[MAXPACKET];
    struct sockaddr_in servaddr, cliaddr;
    struct timeval tv;

    this->totalPackets = 0;
    this->totalBytes = 0;
    this->totalIgnoredPackets = 0;
    this->totalInvalidPackets = 0;

    printf("Starting server on port %d\n", this->port);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Failed to create socket.\n");
        return;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(this->port);

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0) {
        printf("Failed to set setsockopt(SO_REUSEPORT)\n");
        return;
    }

    if (bind(sockfd, (const struct sockaddr *)&servaddr, (socklen_t) sizeof(servaddr)) < 0 ) {
        printf("Failed to bind socket.\n");
        return;
    }

    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    printf("\n");

    uint32_t nextdebugprint = 0;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        uint32_t rv = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if(rv == 1) {
            socklen_t slen = sizeof(cliaddr);
            memset(&cliaddr, 0, sizeof(cliaddr));
            uint32_t n = recvfrom(sockfd, (char *)buffer, MAXPACKET, 0, (struct sockaddr *) &cliaddr, &slen);
            this->parsePacket(buffer, n);
        }

        uint32_t T = (uint32_t)time(NULL);
        if (T > nextdebugprint) {
            nextdebugprint = T + 5;
            this->printStatistics();
        }
    }
}
