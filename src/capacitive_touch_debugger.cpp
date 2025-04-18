#include "capacitive_touch_debugger.h"

// New constructor for two sensors
CapacitiveTouchDebugger::CapacitiveTouchDebugger(int sensorPin1, int sensorPin2,
                                                 const char *name1, const char *name2,
                                                 int initialThreshold1, int initialThreshold2,
                                                 int sampleSize)
    : sampleSize(sampleSize),
      activeSensor(0),
      debouncePeriod(50),
      interactiveModeActive(false)
{
    // Initialize sensor pins and properties
    sensorPins[0] = sensorPin1;
    sensorPins[1] = sensorPin2;
    sensorNames[0] = name1;
    sensorNames[1] = name2;
    thresholds[0] = initialThreshold1;
    thresholds[1] = initialThreshold2;

    // Initialize statistics for both sensors
    for (int i = 0; i < 2; i++)
    {
        minReadings[i] = 9999;
        maxReadings[i] = 0;
        sumReadings[i] = 0;
        numReadings[i] = 0;
        lastReadings[i] = 0;
        touchStates[i] = false;
        lastTouchTimes[i] = 0;
    }
}

// Old constructor for backward compatibility
CapacitiveTouchDebugger::CapacitiveTouchDebugger(int sensorPin, const char *name, int initialThreshold, int sampleSize)
    : sampleSize(sampleSize),
      activeSensor(0),
      debouncePeriod(50),
      interactiveModeActive(false)
{
    // Initialize only the first sensor
    sensorPins[0] = sensorPin;
    sensorPins[1] = -1; // Invalid pin to indicate not used
    sensorNames[0] = name;
    sensorNames[1] = "Unused";
    thresholds[0] = initialThreshold;
    thresholds[1] = 0;

    // Initialize statistics for both sensors
    for (int i = 0; i < 2; i++)
    {
        minReadings[i] = 9999;
        maxReadings[i] = 0;
        sumReadings[i] = 0;
        numReadings[i] = 0;
        lastReadings[i] = 0;
        touchStates[i] = false;
        lastTouchTimes[i] = 0;
    }
}

void CapacitiveTouchDebugger::begin()
{
    Serial.println(F("=== Capacitive Touch Debugger ==="));

    // Print information for each configured sensor
    for (int i = 0; i < 2; i++)
    {
        if (sensorPins[i] >= 0)
        { // Only show valid sensors
            Serial.print(F("Sensor "));
            Serial.print(i + 1);
            Serial.print(F(": "));
            Serial.println(sensorNames[i]);
            Serial.print(F("Pin: "));
            Serial.print(sensorPins[i]);
            Serial.print(F(", Initial threshold: "));
            Serial.println(thresholds[i]);
        }
    }

    // Print help
    printHelp();

    unsigned long startTime = millis();
    interactiveModeActive = true;

    Serial.println(F("\n=== Capacitive Touch Debug Interface ==="));

    // Print info for all valid sensors
    for (int i = 0; i < 2; i++)
    {
        if (sensorPins[i] >= 0)
        {
            Serial.print(F("Sensor "));
            Serial.print(i + 1);
            Serial.print(F(": "));
            Serial.println(sensorNames[i]);
        }
    }

    Serial.print(F("Active sensor: "));
    Serial.println(sensorNames[activeSensor]);
    Serial.println(F("Type 'help' for available commands or 'exit' to quit."));
    printPrompt();
}

bool CapacitiveTouchDebugger::processCommand(const String &command)
{
    // Check for empty command
    if (command.length() == 0)
    {
        return false;
    }

    // Handle different commands
    if (command == "help" || command == "?")
    {
        printHelp();
        return true;
    }
    else if (command == "monitor" || command == "m")
    {
        monitor(100000, 100); // Monitor for 30 seconds
        printPrompt();
        return true;
    }
    else if (command == "exit" || command == "q")
    {
        Serial.println(F("Exiting debug mode."));
        interactiveModeActive = false;
        return true;
    }
    else if (command == "calibrate" || command == "c")
    {
        // Calibrate the active sensor
        calibrate(activeSensor);
        printPrompt();
        return true;
    }
    else if (command == "calibrateall" || command == "ca")
    {
        // Calibrate all sensors
        calibrateAll();
        printPrompt();
        return true;
    }
    else if (command.startsWith("calibrate "))
    {
        // Calibrate a specific sensor
        int sensor = command.substring(10).toInt() - 1; // Convert to 0-based
        if (sensor >= 0 && sensor < 2 && sensorPins[sensor] >= 0)
        {
            calibrate(sensor);
        }
        else
        {
            Serial.println(F("Invalid sensor number"));
        }
        printPrompt();
        return true;
    }
    else if (command.startsWith("set "))
    {
        // Check if the command has a sensor number and threshold
        int spacePos = command.indexOf(' ', 4);
        if (spacePos > 0)
        {
            // Format is "set <sensor> <threshold>"
            int sensor = command.substring(4, spacePos).toInt() - 1; // Convert to 0-based
            int newThreshold = command.substring(spacePos + 1).toInt();

            if (sensor >= 0 && sensor < 2 && sensorPins[sensor] >= 0)
            {
                thresholds[sensor] = newThreshold;
                Serial.print(F("Threshold for sensor "));
                Serial.print(sensorNames[sensor]);
                Serial.print(F(" set to: "));
                Serial.println(newThreshold);
            }
            else
            {
                Serial.println(F("Invalid sensor number"));
            }
        }
        else
        {
            // Format is "set <threshold>" (apply to active sensor)
            int newThreshold = command.substring(4).toInt();
            thresholds[activeSensor] = newThreshold;
            Serial.print(F("Threshold for sensor "));
            Serial.print(sensorNames[activeSensor]);
            Serial.print(F(" set to: "));
            Serial.println(newThreshold);
        }
        printPrompt();
        return true;
    }
    else if (command.startsWith("sensor "))
    {
        // Change active sensor
        int sensor = command.substring(7).toInt() - 1; // Convert to 0-based
        if (sensor >= 0 && sensor < 2 && sensorPins[sensor] >= 0)
        {
            activeSensor = sensor;
            Serial.print(F("Active sensor changed to: "));
            Serial.println(sensorNames[activeSensor]);
        }
        else
        {
            Serial.println(F("Invalid sensor number"));
        }
        printPrompt();
        return true;
    }
    else if (command == "read" || command == "r")
    {
        // Take a single reading from all sensors and display
        update();

        Serial.println(F("\n=== Current Sensor Readings ==="));
        for (int i = 0; i < 2; i++)
        {
            if (sensorPins[i] >= 0) // Only show valid sensors
            {
                Serial.print(sensorNames[i]);
                Serial.print(F(": "));
                printReading(i, lastReadings[i]);
                Serial.println();

                // Show touch status
                Serial.print(F("Status: "));
                if (lastReadings[i] < thresholds[i])
                {
                    Serial.println(F("TOUCH"));
                }
                else
                {
                    Serial.println(F("NO TOUCH"));
                }
                Serial.println();
            }
        }
        printPrompt();
        return true;
    }
    else if (command == "stats" || command == "s")
    {
        // Show statistics for all sensors
        Serial.println(F("\n=== Sensor Statistics ==="));
        for (int i = 0; i < 2; i++)
        {
            if (sensorPins[i] >= 0 && numReadings[i] > 0) // Only show valid sensors with readings
            {
                Serial.print(sensorNames[i]);
                Serial.println(F(":"));

                Serial.print(F("  Samples: "));
                Serial.println(numReadings[i]);

                Serial.print(F("  Min: "));
                Serial.println(minReadings[i]);

                Serial.print(F("  Max: "));
                Serial.println(maxReadings[i]);

                Serial.print(F("  Avg: "));
                Serial.println((float)sumReadings[i] / numReadings[i]);

                Serial.print(F("  Range: "));
                Serial.println(maxReadings[i] - minReadings[i]);

                Serial.print(F("  Current threshold: "));
                Serial.println(thresholds[i]);
                Serial.println();
            }
        }
        printPrompt();
        return true;
    }
    else if (command == "reset")
    {
        // Reset statistics for all sensors
        for (int i = 0; i < 2; i++)
        {
            minReadings[i] = 9999;
            maxReadings[i] = 0;
            sumReadings[i] = 0;
            numReadings[i] = 0;
        }
        Serial.println(F("Statistics reset."));
        printPrompt();
        return true;
    }

    // Command not recognized
    Serial.print(F("Unknown command: "));
    Serial.println(command);
    Serial.println(F("Type 'help' for available commands."));
    printPrompt();

    return false;
}

void CapacitiveTouchDebugger::printPrompt()
{
    Serial.print(sensorNames[activeSensor]);
    Serial.print(F(" > "));
}

void CapacitiveTouchDebugger::update()
{
    // Read values from both sensors
    for (int i = 0; i < 2; i++)
    {
        if (sensorPins[i] >= 0)
        { // Only read valid sensors
            lastReadings[i] = touchRead(sensorPins[i]);

            // Update statistics
            if (lastReadings[i] < minReadings[i])
                minReadings[i] = lastReadings[i];
            if (lastReadings[i] > maxReadings[i])
                maxReadings[i] = lastReadings[i];
            sumReadings[i] += lastReadings[i];
            numReadings[i]++;
        }
    }
}

int CapacitiveTouchDebugger::getReading(int sensorIndex)
{
    if (sensorIndex < 0 || sensorIndex >= 2 || sensorPins[sensorIndex] < 0)
    {
        return -1; // Invalid sensor
    }
    return lastReadings[sensorIndex];
}

void CapacitiveTouchDebugger::monitor(unsigned long duration, unsigned long interval)
{
    unsigned long startTime = millis();
    unsigned long lastUpdateTime = 0;

    Serial.println(F("=== Touch Sensor Monitoring ==="));
    for (int i = 0; i < 2; i++)
    {
        if (sensorPins[i] >= 0)
        { // Only show valid sensors
            Serial.print(F("Sensor "));
            Serial.print(i + 1);
            Serial.print(F(": "));
            Serial.print(sensorNames[i]);
            Serial.print(F(", Pin: "));
            Serial.print(sensorPins[i]);
            Serial.print(F(", Threshold: "));
            Serial.println(thresholds[i]);
        }
    }
    Serial.println(F("Press any key to stop..."));

    // Run until duration is reached (if not 0) or until serial input is received
    while (((duration == 0) || (millis() - startTime < duration)) && !Serial.available())
    {
        // Only update at the specified interval
        if (millis() - lastUpdateTime >= interval)
        {
            update();

            // Print readings for all valid sensors
            for (int i = 0; i < 2; i++)
            {
                if (sensorPins[i] >= 0)
                {
                    Serial.print(sensorNames[i]);
                    Serial.print(F(": "));
                    printReading(i, lastReadings[i]);
                }
            }
            Serial.println(); // Add a blank line between readings

            lastUpdateTime = millis();
        }

        // Small delay to prevent overwhelming the serial output
        delay(10);
    }

    // Clear any received characters
    while (Serial.available())
    {
        Serial.read();
    }

    Serial.println(F("\n=== Monitoring Ended ==="));
}

void CapacitiveTouchDebugger::printReading(int sensorIndex, int reading)
{
    Serial.print(F("Reading: "));
    Serial.print(reading);
    Serial.print(F(" | "));

    // Indicate if this is considered a touch
    if (reading < thresholds[sensorIndex])
    {
        Serial.print(F("TOUCH "));
    }
    else
    {
        Serial.print(F("      "));
    }

    // Print visual bar
    // Create a visual bar representing the reading
    const int barWidth = 30;
    int normalizedReading;

    // If we have min and max readings, use them to scale the bar
    if (minReadings[sensorIndex] < maxReadings[sensorIndex])
    {
        normalizedReading = map(reading, minReadings[sensorIndex], maxReadings[sensorIndex], 0, barWidth);
    }
    else
    {
        // Default scaling
        normalizedReading = map(reading, 0, 100, 0, barWidth);
    }

    // Protect against out-of-range values
    normalizedReading = constrain(normalizedReading, 0, barWidth);

    // Create threshold indicator position
    int thresholdPos = 0;
    if (minReadings[sensorIndex] < maxReadings[sensorIndex])
    {
        thresholdPos = map(thresholds[sensorIndex], minReadings[sensorIndex], maxReadings[sensorIndex], 0, barWidth);
        thresholdPos = constrain(thresholdPos, 0, barWidth);
    }
}

bool CapacitiveTouchDebugger::runInteractiveMode(unsigned long timeout)
{
    unsigned long startTime = millis();
    interactiveModeActive = true;

    while (interactiveModeActive)
    {
        // Check for timeout if specified
        if (timeout > 0 && (millis() - startTime > timeout))
        {
            Serial.println(F("\nDebug session timed out."));
            interactiveModeActive = false;
            return false;
        }

        // Check for serial input
        if (Serial.available() > 0)
        {
            String command = Serial.readStringUntil('\n');
            command.trim();
            processCommand(command);
        }

        // Small delay to prevent CPU hogging
        delay(10);
    }

    return true;
}

void CapacitiveTouchDebugger::printHelp()
{
    Serial.println(F("\n--- Available Commands ---"));
    Serial.println(F("monitor, m     : Monitor all sensor values in real-time"));
    Serial.println(F("read, r        : Take a single reading from all sensors"));
    Serial.println(F("sensor X       : Set active sensor (1 or 2)"));
    Serial.println(F("calibrate, c   : Run calibration procedure for active sensor"));
    Serial.println(F("calibrate X    : Run calibration procedure for sensor X"));
    Serial.println(F("calibrateAll   : Run calibration procedure for all sensors in sequence"));
    Serial.println(F("reset          : Reset all statistics"));
    Serial.println(F("set X Y        : Set threshold for sensor X to Y (e.g., 'set 1 40')"));
    Serial.println(F("set Y          : Set threshold for active sensor to Y (e.g., 'set 40')"));
    Serial.println(F("stats, s       : Show statistics from collected readings"));
    Serial.println(F("help, ?        : Show this help message"));
    Serial.println(F("exit, q        : Exit debug mode"));
    Serial.println(F("------------------------"));
}

void CapacitiveTouchDebugger::calibrate(int sensorIndex)
{
    if (sensorIndex < 0 || sensorIndex >= 2 || sensorPins[sensorIndex] < 0)
    {
        Serial.println(F("Invalid sensor for calibration"));
        return;
    }

    // Increase number of samples for more reliable measurements
    const int numSamples = 50;    // Increased from 30
    const int sampleDelay = 40;   // Slightly increased delay between samples
    const int readyCountdown = 5; // Countdown time in seconds

    int untouchedReadings[numSamples];
    int touchedReadings[numSamples];

    Serial.print(F("\n=== Calibrating "));
    Serial.print(sensorNames[sensorIndex]);
    Serial.println(F(" ==="));
    Serial.println(F("Step 1: Please do NOT touch the sensor..."));

    // Initial delay to ensure user isn't touching the sensor
    Serial.println(F("Getting ready in:"));
    for (int i = readyCountdown; i > 0; i--)
    {
        Serial.print(i);
        Serial.println(F(" seconds..."));
        delay(1000);
    }

    // Take readings with no touch
    Serial.println(F("Taking untouched baseline readings for 5 seconds..."));
    Serial.println(F("Do NOT touch the sensor during this time!"));
    for (int i = 0; i < numSamples; i++)
    {
        untouchedReadings[i] = touchRead(sensorPins[sensorIndex]);
        Serial.print(F("."));
        if ((i + 1) % 10 == 0)
        {
            Serial.print(F(" "));
            Serial.print(100 * (i + 1) / numSamples);
            Serial.println(F("%"));
        }
        delay(sampleDelay);
    }

    // Calculate untouched statistics
    long untouchedSum = 0;
    for (int i = 0; i < numSamples; i++)
    {
        untouchedSum += untouchedReadings[i];
    }

    float untouchedAvg = (float)untouchedSum / numSamples;

    // Calculate untouched standard deviation
    float untouchedSumSquaredDiff = 0;
    for (int i = 0; i < numSamples; i++)
    {
        float diff = untouchedReadings[i] - untouchedAvg;
        untouchedSumSquaredDiff += diff * diff;
    }

    float untouchedStdDev = sqrt(untouchedSumSquaredDiff / numSamples);

    // Step 2: Ask user to touch the sensor
    Serial.println(F("\n\nStep 2: Please TOUCH and HOLD the sensor..."));

    // Give user more time to touch the sensor - countdown timer
    Serial.println(F("Getting ready in:"));
    for (int i = readyCountdown; i > 0; i--)
    {
        Serial.print(i);
        Serial.println(F(" seconds..."));
        delay(1000);
    }
    Serial.println(F("Taking touched readings for 5 seconds..."));
    Serial.println(F("Keep touching the sensor during this time!"));

    // Take readings with touch
    for (int i = 0; i < numSamples; i++)
    {
        touchedReadings[i] = touchRead(sensorPins[sensorIndex]);
        Serial.print(F("."));
        if ((i + 1) % 10 == 0)
        {
            Serial.print(F(" "));
            Serial.print(100 * (i + 1) / numSamples);
            Serial.println(F("%"));
        }
        delay(sampleDelay);
    }

    // Calculate touched statistics
    long touchedSum = 0;
    for (int i = 0; i < numSamples; i++)
    {
        touchedSum += touchedReadings[i];
    }

    float touchedAvg = (float)touchedSum / numSamples;

    // Calculate touched standard deviation
    float touchedSumSquaredDiff = 0;
    for (int i = 0; i < numSamples; i++)
    {
        float diff = touchedReadings[i] - touchedAvg;
        touchedSumSquaredDiff += diff * diff;
    }

    float touchedStdDev = sqrt(touchedSumSquaredDiff / numSamples);

    // Calculate midpoint between touched and untouched
    // We add a small buffer to account for noise, ensuring more reliable detection
    int newThreshold;

    // Check if touched readings are indeed lower than untouched
    if (touchedAvg < untouchedAvg)
    {
        // Find a point between the two averages, accounting for standard deviations
        float midpoint = (untouchedAvg + touchedAvg) / 2;

        // Adjust midpoint based on standard deviations to minimize false positives/negatives
        // If untouched values vary more, move threshold closer to touched average
        if (untouchedStdDev > touchedStdDev)
        {
            // Move threshold 10% closer to touched average
            midpoint = midpoint - (untouchedAvg - touchedAvg) * 0.1;
        }

        newThreshold = round(midpoint);
    }
    else
    {
        // This indicates an issue with the sensor or the readings
        Serial.println(F("\n\nWARNING: Touched values not lower than untouched values!"));
        Serial.println(F("Using default calculation method instead."));

        // Fall back to the 85% method if touched readings aren't lower
        newThreshold = round(untouchedAvg * 0.85);
    }

    // Update threshold
    thresholds[sensorIndex] = newThreshold;

    // Display the results
    Serial.println(F("\n\nCalibration Results:"));
    Serial.println(F("Untouched readings:"));
    Serial.print(F("  Average: "));
    Serial.println(untouchedAvg);
    Serial.print(F("  Std Dev: "));
    Serial.println(untouchedStdDev);

    Serial.println(F("Touched readings:"));
    Serial.print(F("  Average: "));
    Serial.println(touchedAvg);
    Serial.print(F("  Std Dev: "));
    Serial.println(touchedStdDev);

    Serial.print(F("New threshold set to: "));
    Serial.println(newThreshold);

    // Let user test the new threshold
    Serial.println(F("\nTesting new threshold for 5 seconds..."));
    Serial.println(F("Touch and release the sensor to test."));

    unsigned long testStart = millis();
    unsigned long lastDisplayTime = 0;
    while (millis() - testStart < 5000) // Test for 5 seconds
    {
        int reading = touchRead(sensorPins[sensorIndex]);

        if (millis() - lastDisplayTime >= 100) // Update display every 100ms
        {
            Serial.print(F("\rReading: "));
            Serial.print(reading);
            Serial.print(F(" | Threshold: "));
            Serial.print(newThreshold);
            Serial.print(F(" | Status: "));
            if (reading < thresholds[sensorIndex])
            {
                Serial.print(F("TOUCH   "));
            }
            else
            {
                Serial.print(F("NO TOUCH"));
            }
            lastDisplayTime = millis();
        }

        delay(10);
    }

    Serial.println(F("\n\nCalibration complete."));
}

void CapacitiveTouchDebugger::calibrateAll()
{
    Serial.println(F("\n=== Calibrating All Capacitive Touch Sensors ==="));

    // Count how many active sensors we have
    int activeSensorCount = 0;
    for (int i = 0; i < 2; i++)
    {
        if (sensorPins[i] >= 0)
        {
            activeSensorCount++;
        }
    }

    if (activeSensorCount == 0)
    {
        Serial.println(F("No active sensors found!"));
        return;
    }

    Serial.println(F("This will calibrate all active touch sensors in sequence."));
    Serial.println(F("Follow the prompts for each sensor."));
    Serial.println();

    // Calibrate each active sensor
    int calibratedCount = 0;
    for (int i = 0; i < 2; i++)
    {
        if (sensorPins[i] >= 0)
        {
            calibratedCount++;

            Serial.print(F("Calibrating sensor "));
            Serial.print(calibratedCount);
            Serial.print(F(" of "));
            Serial.print(activeSensorCount);
            Serial.print(F(": "));
            Serial.println(sensorNames[i]);

            // If this isn't the first sensor, add a pause
            if (calibratedCount > 1)
            {
                Serial.println(F("\nPreparing for next sensor..."));
                delay(2000);
            }

            // Run calibration for this sensor
            calibrate(i);
        }
    }

    Serial.println(F("\n=== All Sensors Calibrated ==="));
    Serial.println(F("Summary of calibration results:"));

    for (int i = 0; i < 2; i++)
    {
        if (sensorPins[i] >= 0)
        {
            Serial.print(sensorNames[i]);
            Serial.print(F(": Threshold = "));
            Serial.println(thresholds[i]);
        }
    }
}