#include <arpa/inet.h>
#include <fstream>
#include <glib.h>
#include <gst/gst.h>
#include <map>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#if GPIO
#include <wiringPi.h>
#endif

#include "artnetsender.h"
#include "config.h"
#include "player.h"

int main(int argc, char *argv[]) {

  if (argc != 2) {
    printf("artnet-playback [config.json]\n");
    return 1;
  }

#if GPIO
  printf("GPIO Support: Enabled\n");
  wiringPiSetupGpio();
#else
  printf("GPIO Support: Disabled\n");
#endif

  /* Initialize GStreamer */
  gst_init(&argc, &argv);

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

  Player player((Configuration *)&config, (ArtnetSender *)&artnetSender);

  return player.run();
}
