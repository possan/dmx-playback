#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <png.h>

#include "recorder.h"

void Recorder::handleDmxDataCommand(uint16_t universe, uint8_t *data, uint16_t datalength) {
    printf("Got dmx command, universe=%d, length=%d\n", universe, datalength);

    std::set<uint16_t>::iterator it = seenUniverses.find(universe);
    if (it != seenUniverses.end()) {
        printf("Universe seen already, probably new frame...\n");

        flushFrame();
        beginFrame(this->lastFrameNumber + 1);

        printf("Handling previous dmx command, universe=%d, length=%d\n", universe, datalength);
    }

    seenUniverses.insert(universe);
    memcpy(this->frameData + (universe * 256 * 3), data, datalength);
}

 void Recorder::handleSyncCommand() {
    printf("Got sync command.\n");
    flushFrame();
    beginFrame(this->lastFrameNumber + 1);
}

void Recorder::flushFrame() {
    if (seenUniverses.size() == 0) {
        printf("Nothing to write for frame %d.\n", this->lastFrameNumber);
        return;
    }

    char filename[512];
    sprintf(filename, "%s%04d.png", this->outputFolder.c_str(), lastFrameNumber);
    printf("Writing frame #%d to: %s\n", this->lastFrameNumber, filename);

    printf("Seen universes this frame: ");
    std::set<uint16_t>::iterator it;
    for (it=seenUniverses.begin(); it!=seenUniverses.end(); ++it) {
        printf("%d ", *it);
    }
    printf("\n");

    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file %s for writing\n", filename);
        goto finalise;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fprintf(stderr, "Could not allocate write struct\n");
        goto finalise;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fprintf(stderr, "Could not allocate info struct\n");
        goto finalise;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during png creation\n");
        goto finalise;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, 256, 256, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    for (int row=0 ; row<256 ; row++) {
        png_write_row(png_ptr, this->frameData + (256*3*row));
    }
    png_write_end(png_ptr, NULL);

    finalise:
    if (fp != NULL) fclose(fp);
    if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

    printf("\n");
}

void Recorder::beginFrame(uint16_t number) {
    this->lastFrameNumber = number;
    seenUniverses.clear();
}

Recorder::Recorder(std::string outputFolder) {
    this->frameData = (uint8_t *)malloc(256*256*3);
    memset(this->frameData, 0, 256*256*3);
    this->outputFolder = outputFolder;
    this->beginFrame(0);
}

Recorder::~Recorder() {
    this->flushFrame();
    free(this->frameData);
}
