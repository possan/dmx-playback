#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class TimedEvent {
private:
  int counter;

public:
  int frame;

  std::string description;
  std::string type;

  int gpioPin;
  int gpioLevel;

  int limitTimes;

  int fired;

  TimedEvent();
  ~TimedEvent();

  bool parse(nlohmann::json &node);

  void beforeFrame(uint16_t frame);
  void rewind();
};

class ArtnetUniverse {
public:
  int universe;

  std::string type;

  int x;
  int y;
  int length;

  ArtnetUniverse();
  ~ArtnetUniverse();

  bool parse(nlohmann::json &node);
};

class ArtnetConnection;

class ArtnetTarget {
public: // private
  ArtnetConnection *connection;

public:
  int index;
  std::string address;
  int port;
  std::vector<ArtnetUniverse> universes;

  ArtnetTarget();
  ~ArtnetTarget();

  bool parse(nlohmann::json &node);
};

class Configuration {
public:
  std::string video;

  std::vector<ArtnetTarget> targets;
  std::vector<TimedEvent> events;

  Configuration();
  ~Configuration();

  bool parse(nlohmann::json &node);
};
