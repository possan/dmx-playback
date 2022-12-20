#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string>

#include "artnetserver.h"
#include "recorder.h"

int main(int argc, char **argv) {
    printf("ArtnetRecorder - built " __DATE__ "\n\n");

    if (argc != 3) {
        printf("Syntax: artnet-recorder [artnet port] [output folder]\n");
        printf("\n");
        printf("Example: artnet-recorder 6454 output/frame\n");
        printf("  will write frames as output/frame0000.png\n");
        printf("  then convert it to a video: ffmpeg -framerate 40 -i output/frame\%04d.png -f mov -c:v qtrle -pix_fmt rgb24 output.mov\n");
        printf("\n");
        return 1;
    }

    std::string outputFolder = std::string(argv[2]);
    uint16_t port = (uint16_t)atoi(argv[1]);

    Recorder recorder(outputFolder);
    ArtNetServer server(port, &recorder);

    server.run();

    return 0;
}
