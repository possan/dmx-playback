#pragma once

#include <vector>

class Recorder;

class UniverseStat {
    public:
        uint32_t universe;
        uint32_t numUpdates;
        uint32_t lastActivity;
};

class ArtNetServer {
    public:
        uint16_t port;
        Recorder *recorder;
        uint32_t totalPackets;
        uint32_t totalInvalidPackets;
        uint32_t totalIgnoredPackets;
        uint32_t totalBytes;
        std::vector<UniverseStat> universeStats;

        void parsePacket(uint8_t *bytes, uint32_t length);

        void touchUniverse(uint32_t universe);
        void printStatistics();

    public:
        ArtNetServer(uint16_t port, Recorder *recorder);
        ~ArtNetServer();

        void run();
};
