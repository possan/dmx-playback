#pragma once

#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/types.h>

class Configuration;
class ArtnetTarget;

class ArtnetConnection {
private:
  ArtnetTarget *targetConfiguration;
  sockaddr_in servaddr;

public:
  ArtnetConnection(ArtnetTarget *targetConfiguration);
  ~ArtnetConnection();

  void handleVideoFrame(uint16_t frame, uint8_t *pixels, uint16_t width,
                        uint16_t height);

  void sendUniverse(uint8_t sequence, uint8_t universe, uint16_t length,
                    uint8_t *channels);
  void sendSync();
};

class ArtnetSender {
private:
  Configuration *configuration;
  std::vector<ArtnetConnection> connections;

public:
  ArtnetSender(Configuration *configuration);
  ~ArtnetSender();

  void handleVideoFrame(uint16_t frame, uint8_t *pixels, uint16_t width,
                        uint16_t height);
};
