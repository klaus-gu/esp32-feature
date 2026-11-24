// driver.cpp —— drivers 层：驱动注册表实现

#include "driver.h"

#include "../core/logger.h"

namespace {

constexpr int MAX_DRIVERS = 8;

drivers::Driver* gDrivers[MAX_DRIVERS];
int gCount = 0;

}  // namespace

namespace drivers {

void add(Driver* driver) {
  if (driver == nullptr) {
    return;
  }
  if (gCount >= MAX_DRIVERS) {
    logger::line("[ERROR] Driver table full, '%s' not registered.", driver->name());
    return;
  }
  gDrivers[gCount++] = driver;
}

int beginAll() {
  int failed = 0;
  for (int i = 0; i < gCount; i++) {
    if (gDrivers[i]->begin()) {
      logger::line("[DRIVER] %s ready.", gDrivers[i]->name());
    } else {
      failed++;
      logger::line("[DRIVER] %s init failed, skipped.", gDrivers[i]->name());
    }
  }
  return failed;
}

void pollAll() {
  for (int i = 0; i < gCount; i++) {
    gDrivers[i]->poll();
  }
}

void printStatusAll() {
  for (int i = 0; i < gCount; i++) {
    logger::line("[DRIVER] %s:", gDrivers[i]->name());
    gDrivers[i]->printStatus();
  }
}

}  // namespace drivers
