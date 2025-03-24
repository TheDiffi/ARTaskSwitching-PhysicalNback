#include <Arduino.h>
#include "nback_task.h"
#include "data_collector.h"

// Create an instance of the NBackTask class
NBackTask nBackTask;

void setup()
{
  // Initialize the task
  nBackTask.setup();
  // nBackTask.configure(2000, 2000, 2, 10, "TEST", 1);
  //  nBackTask.startTask();
}

void loop()
{
  // Run the task loop
  nBackTask.loop();
}