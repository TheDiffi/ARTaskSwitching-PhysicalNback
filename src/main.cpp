#include <Arduino.h>
#include "nback_task.h"
#include "data_collector.h"

// Create an instance of the NBackTask class
NBackTask nBackTask;

void setup()
{
  // Initialize the task
  nBackTask.setup();
}

void loop()
{
  // Run the task loop
  nBackTask.loop();
}