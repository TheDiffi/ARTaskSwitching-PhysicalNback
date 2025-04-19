#include <Arduino.h>
#include "nback_task.h"
#include "data_collector.h"
#include "capacitive_touch_debugger.h"

// Create an instance of the NBackTask class
NBackTask nBackTask;

// Create a capacitive touch debugger for both sensors
CapacitiveTouchDebugger touchDebugger(TOUCH_CORRECT_PIN, TOUCH_WRONG_PIN, "Correct", "Wrong", TOUCH_THRESHOLD_CORRECT, TOUCH_THRESHOLD_WRONG);

bool debugMode = false;
void handleSerialInput();

void setup()
{
  Serial.begin(9600);

  // Wait for Serial connection
  delay(1000);

  Serial.println(F("N-Back LED Button System"));
  Serial.println(F("Enter 'debug_touch' for capacitive touch debugging"));

  // Initialize the task
  nBackTask.setup();
  nBackTask.configure(2000, 2000, 1, 10, "TEST", 1);

  // Uncomment to run task directly at startup
  /*
  nBackTask.startTask();
  */
}

void loop()
{
  // Check for debug command
  handleSerialInput();

  // Run the task loop if not in debug mode
  if (!debugMode)
  {
    nBackTask.loop();
  }
}

void handleSerialInput()
{
  if (Serial.available() > 0)
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    Serial.print(F("Received command: "));
    Serial.println(command);

    bool commandProcessed = false;

    commandProcessed = touchDebugger.processCommand(command);

    if (!commandProcessed)
    {
      // Process command for NBackTask if not handled by touch debugger
      commandProcessed = nBackTask.processSerialCommands(command);
    }
    if (!commandProcessed)
    {
      Serial.println(F("Command not recognized."));
    }
  }
}