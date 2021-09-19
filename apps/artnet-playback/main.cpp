#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <glib.h>
#include <gst/gst.h>
#if GPIO
#include <wiringPi.h>
#endif

#include "config.h"
#include "player.h"
#include "artnetsender.h"


int main(int argc, char *argv[]) {

  if (argc != 2) {
    printf("artnet-playback [config.json]\n");
    return 1;
  }

#if GPIO
  wiringPiSetup();
#endif

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  char *configFile = argv[1];
  printf("Loading config file: %s\n", configFile);

  std::ifstream configstream(configFile);
  nlohmann::json configjson;

  Configuration config;
  if (configstream >> configjson) {
      if (!config.parse(configjson)) {
          printf("Failed to parse config json\n\n");
          return 1;
      }
  } else {
      printf("Failed to read config json\n\n");
      return 1;
  }

  ArtnetSender artnetSender(&config);

  Player player((Configuration *)&config, (ArtnetSender*)&artnetSender);

  return player.run();
}
