#pragma once

#include "config.h"
#include "artnetsender.h"

class Player {
private:
    Configuration *configuration;
    ArtnetSender *sender;

public:
    Player(Configuration *configuration, ArtnetSender *sender);
    ~Player();

    void handleVideoFrame(uint16_t frame, uint8_t *pixels, uint16_t width, uint16_t height);

    int run();
};
