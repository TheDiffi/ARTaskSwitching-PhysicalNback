#ifndef CAPACITIVE_TOUCH_DEBUGGER_H
#define CAPACITIVE_TOUCH_DEBUGGER_H

#include <Arduino.h>

// if touchRead is not defined, define it
#ifndef touchRead
#define touchRead(p) (analogRead(p))
#endif

/**
 * @brief Class for debugging and calibrating capacitive touch sensors
 *
 * This class provides tools to monitor, analyze, and calibrate capacitive touch sensors.
 * It includes features for threshold determination, statistical analysis, and visualization.
 * Integrated serial interface allows for interactive debugging and configuration.
 */
class CapacitiveTouchDebugger
{
public:
    /**
     * @brief Construct a new Capacitive Touch Debugger
     *
     * @param sensorPin1 First pin connected to a capacitive touch sensor
     * @param sensorPin2 Second pin connected to a capacitive touch sensor
     * @param name1 Descriptive name for first sensor (e.g., "Correct")
     * @param name2 Descriptive name for second sensor (e.g., "Wrong")
     * @param initialThreshold1 Initial threshold value for first sensor (optional)
     * @param initialThreshold2 Initial threshold value for second sensor (optional)
     * @param sampleSize Number of samples to collect for analysis (optional)
     */
    CapacitiveTouchDebugger(int sensorPin1, int sensorPin2,
                            const char *name1 = "Touch1",
                            const char *name2 = "Touch2",
                            int initialThreshold1 = 37,
                            int initialThreshold2 = 37,
                            int sampleSize = 100);

    // Keep the old constructor for backward compatibility
    CapacitiveTouchDebugger(int sensorPin, const char *name = "Touch",
                            int initialThreshold = 37, int sampleSize = 100);

    /**
     * @brief Initialize the debugger
     */
    void begin();

    /**
     * @brief Process serial commands sent to the debugger
     *
     * @param command The command to process
     * @return true if the command was handled
     */
    bool processCommand(const String &command);

    /**
     * @brief Take a single reading and update statistics for both sensors
     */
    void update();

    /**
     * @brief Get the last reading from a specific sensor
     *
     * @param sensorIndex The sensor to read (0 for first, 1 for second)
     * @return Current sensor reading
     */
    int getReading(int sensorIndex = 0);

    /**
     * @brief Run continuous monitoring with serial output
     *
     * @param duration Duration to run in milliseconds (0 for indefinite)
     * @param interval Interval between readings in milliseconds
     */
    void monitor(unsigned long duration = 0, unsigned long interval = 100);

    /**
     * @brief Run the serial interface in interactive mode
     *
     * This method processes user input and provides an interactive
     * debugging interface for the capacitive touch sensor.
     *
     * @param timeout Timeout in milliseconds after which to return (0 for no timeout)
     * @return true if exited due to user command, false if timed out
     */
    bool runInteractiveMode(unsigned long timeout = 0);

    /**
     * @brief Calibrate a capacitive touch sensor
     *
     * This method automatically determines an appropriate threshold for the specified sensor.
     * It takes multiple readings while the sensor is untouched and calculates a threshold
     * based on statistical analysis of the readings.
     *
     * @param sensorIndex The sensor to calibrate (0 for first, 1 for second)
     * @note The method will print the calibration results to the serial monitor.
     */
    void calibrate(int sensorIndex);

    /**
     * @brief Calibrate all active capacitive touch sensors
     *
     * This method runs the calibration procedure for all active touch sensors in sequence.
     * It guides the user through the process of calibrating each sensor one by one.
     */
    void calibrateAll();

    /**
     * @brief Print help information about available commands
     */
    void printHelp();

private:
    // Sensor pins and properties
    int sensorPins[2];
    int thresholds[2];
    const char *sensorNames[2];
    int sampleSize;
    int activeSensor; // Current sensor being worked with (0 or 1)

    // Statistics for both sensors
    int minReadings[2];
    int maxReadings[2];
    long sumReadings[2];
    int numReadings[2];
    int lastReadings[2];

    // Touch state tracking for both sensors
    bool touchStates[2];
    unsigned long lastTouchTimes[2];
    unsigned long debouncePeriod;

    // Serial interface
    bool interactiveModeActive;

    // Helper methods
    void printReading(int sensorIndex, int reading);
};

#endif // CAPACITIVE_TOUCH_DEBUGGER_H