#include "config.h"

#include <nlohmann/json.hpp>

#if GPIO
#include <wiringPi.h>
#endif

TimedEvent::TimedEvent() {
  this->limitTimes = 0;
  this->counter = 0;
  this->fired = 0;
}

TimedEvent::~TimedEvent() {}

bool TimedEvent::parse(nlohmann::json &node) {
  if (node.contains("type")) {
    this->type = node["type"];
  }

  if (node.contains("description")) {
    this->description = node["description"];
  }

  if (node.contains("gpio_pin")) {
    this->gpioPin = node["gpio_pin"];
  }

  if (node.contains("gpio_level")) {
    this->gpioLevel = node["gpio_level"];
  }

  if (node.contains("when")) {
    this->frame = node["when"];
  }

  if (node.contains("limit")) {
    this->limitTimes = node["limit"];
  }

  return true;
}

void TimedEvent::beforeFrame(uint16_t frame) {

  if (this->fired) {
    return;
  }

  if (frame < this->frame) {
    return;
  }

  this->fired = 1;

  printf("in TimedEvent::beforeFrame %d \"%s\" (%s) (fired %d times before)\n",
         frame, this->type.c_str(), this->description.c_str(), this->counter);

  if (this->limitTimes > 0 && this->counter >= this->limitTimes) {
    printf("  [Already fired too many times]\n");
    return;
  }

  if (this->type == "gpio") {
#if GPIO
    printf("  Set GPIO %d to %d\n", this->gpioPin, this->gpioLevel);
    pinMode(this->gpioPin, OUTPUT);
    digitalWrite(this->gpioPin, this->gpioLevel ? HIGH : FALSE);
#else
    printf("  [DISABLED] Set GPIO %d to %d\n", this->gpioPin, this->gpioLevel);
#endif
  }

  this->counter++;
}

void TimedEvent::rewind() { this->fired = 0; }

ArtnetUniverse::ArtnetUniverse() { this->length = 512; }

ArtnetUniverse::~ArtnetUniverse() {}

bool ArtnetUniverse::parse(nlohmann::json &node) {

  if (node.contains("universe")) {
    this->universe = node["universe"];
  }

  if (node.contains("type")) {
    this->type = node["type"];
  }

  if (node.contains("x")) {
    this->x = node["x"];
  }

  if (node.contains("y")) {
    this->y = node["y"];
  }

  if (node.contains("length")) {
    this->length = node["length"];
  }

  return true;
}

ArtnetTarget::ArtnetTarget() {
  this->port = 6454;
  this->connection = NULL;
}

ArtnetTarget::~ArtnetTarget() {}

bool ArtnetTarget::parse(nlohmann::json &node) {
  if (node.contains("port")) {
    this->port = node["port"];
  }

  if (node.contains("address")) {
    this->address = node["address"];
  }

  if (node.contains("universes")) {
    for (nlohmann::json::iterator it = node["universes"].begin();
         it != node["universes"].end(); ++it) {
      ArtnetUniverse o;
      if (o.parse(*it)) {
        this->universes.push_back(o);
      } else {
        printf("Config error: Failed to parse output.\n");
        return false;
      }
    }
  }

  return true;
}

Configuration::Configuration() {}

Configuration::~Configuration() {}

bool Configuration::parse(nlohmann::json &node) {
  if (node.contains("video")) {
    this->video = node["video"];
  }

  if (node.contains("events")) {
    for (nlohmann::json::iterator it = node["events"].begin();
         it != node["events"].end(); ++it) {
      TimedEvent o;
      if (o.parse(*it)) {
        this->events.push_back(o);
      } else {
        printf("Config error: Failed to parse output.\n");
        return false;
      }
    }
  }

  if (node.contains("targets")) {
    for (nlohmann::json::iterator it = node["targets"].begin();
         it != node["targets"].end(); ++it) {
      ArtnetTarget o;
      if (o.parse(*it)) {
        this->targets.push_back(o);
      } else {
        printf("Config error: Failed to parse output.\n");
        return false;
      }
    }
  }

  return true;
}
