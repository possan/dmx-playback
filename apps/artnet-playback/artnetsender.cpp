#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <vector>
#include <map>
#include <string>
#include <fstream>

#include <sys/types.h>
#include <sys/socket.h>
#ifdef __has_include // C++17, supported as extension to C++11 in clang, GCC 5+, vs2015
#  if __has_include(<endian.h>)
#    include <endian.h> // gnu libc normally provides, linux
#  elif __has_include(<machine/endian.h>)
#    include <machine/endian.h> //open bsd, macos
#  endif
#endif
#include <netinet/in.h>
#include <arpa/inet.h>

#include "artnetsender.h"
#include "config.h"

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

struct ArtnetSyncHeader {
    char identifier[8];
    unsigned short opcode;
    unsigned short protver;
    unsigned char aux1;
    unsigned char aux2;
};

bool udpSend(sockaddr_in servaddr, const uint8_t *packet, uint16_t length ){
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        printf("failed to open socket\n");
        return false;
    }

    int broadcast=1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast);

    int ret = sendto(fd, packet, length, 0, (sockaddr*)&servaddr, sizeof(servaddr));
    close(fd);
    return ret == length;
}

uint16_t htoartnet_le(uint16_t host) {
  return host;
}

uint16_t htoartnet_be(uint16_t host) {
  return __builtin_bswap16(host);
}

ArtnetConnection::ArtnetConnection(ArtnetTarget *targetConfiguration) {
  this->targetConfiguration = targetConfiguration;

  bzero(&this->servaddr,sizeof(servaddr));
  this->servaddr.sin_family = AF_INET;
  this->servaddr.sin_addr.s_addr = INADDR_ANY;
  this->servaddr.sin_port = htons(this->targetConfiguration->port);
  inet_aton(this->targetConfiguration->address.c_str(), &this->servaddr.sin_addr);

  printf("ArtnetSender: Preparing connecting to \"%s\" port %d...\n", this->targetConfiguration->address.c_str(), this->targetConfiguration->port);
}

ArtnetConnection::~ArtnetConnection() {}

void ArtnetConnection::sendUniverse(uint8_t sequence, uint8_t universe, uint16_t length, uint8_t *channels) {
  uint8_t temppacket[1024];

  ArtnetDmxHeader *dmxpacket = (ArtnetDmxHeader *)&temppacket;
  memset(dmxpacket, 0, sizeof(ArtnetDmxHeader) + 512);
  memcpy(dmxpacket->identifier, "Art-Net\0", 8);
  dmxpacket->protver = 14;
  dmxpacket->opcode = 0x5000;
  dmxpacket->sequence = sequence;
  dmxpacket->universe = universe;
  dmxpacket->datalength = htoartnet_be(length);
  memcpy((uint8_t *)&temppacket + sizeof(ArtnetDmxHeader), channels, length);

  udpSend(this->servaddr, (const uint8_t *)dmxpacket, sizeof(ArtnetDmxHeader) + length);
}

void ArtnetConnection::sendSync() {

  uint8_t temppacket[1024];

  ArtnetSyncHeader *syncpacket = (ArtnetSyncHeader *)&temppacket;
  memset(syncpacket, 0, sizeof(ArtnetSyncHeader));
  memcpy(syncpacket->identifier, "Art-Net\0", 8);
  syncpacket->protver = 14;
  syncpacket->opcode = 0x5200;

  udpSend(this->servaddr, (const uint8_t *)&temppacket, sizeof(ArtnetSyncHeader));
}

void ArtnetConnection::handleVideoFrame(uint16_t frame, uint8_t *pixels, uint16_t width, uint16_t height)
{
    for (int u = 0; u < this->targetConfiguration->universes.size(); u++)
    {
        ArtnetUniverse *uni = &this->targetConfiguration->universes[u];
        if (uni->type == "dmx512row")
        {
            sendUniverse(frame, uni->universe, uni->length, pixels + 3 * (uni->y * 256 + uni->x));
        }
    }
}

ArtnetSender::ArtnetSender(Configuration *configuration) {
  this->configuration = configuration;
}

ArtnetSender::~ArtnetSender() {}

void ArtnetSender::handleVideoFrame(uint16_t frame, uint8_t *pixels, uint16_t width, uint16_t height) {
    // Send dmx frames
    for (int t = 0; t < this->configuration->targets.size(); t++)
    {
        ArtnetTarget *target = &this->configuration->targets[t];

        if (target->connection == NULL) {
            target->connection = new ArtnetConnection(target);
        }

        if (target->connection != NULL) {
            target->connection->handleVideoFrame(frame, pixels, width, height);
        }
    }

    // Send sync messages
    for (int t = 0; t < this->configuration->targets.size(); t++)
    {
        ArtnetTarget *target = &this->configuration->targets[t];

        if (target->connection != NULL) {
            target->connection->sendSync();
        }
    }
}
