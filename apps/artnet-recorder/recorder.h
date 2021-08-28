#pragma once

#include <vector>
#include <map>
#include <set>
#include <string>

class Recorder {
    public:
        std::set<uint16_t> seenUniverses;
        std::string outputFolder;
        uint32_t lastFrameNumber;
        uint8_t *frameData;

        void handleDmxDataCommand(uint16_t universe, uint8_t *data, uint16_t datalength);
        void handleSyncCommand();

        void flushFrame();
        void beginFrame(uint16_t number);

    public:
        Recorder(std::string outputFolder);
        ~Recorder();

};
