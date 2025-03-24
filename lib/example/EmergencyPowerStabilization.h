#ifndef EMERGENCY_POWER_STABILIZATION_H
#define EMERGENCY_POWER_STABILIZATION_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "DataCollector.h"

// Hardware configuration
#define LED_PIN 32   // Pin connected to the NeoPixel strip
                     // #define LED_PIN D2 // Pin connected to the NeoPixel strip
#define BUTTON_PIN 14 // Push button for user input
// #define BUTTON_PIN D1     // Push button for user input

// Capacitive touch configuration
#define TOUCH_PIN 13        // Pin connected to the capacitive touch sensor
#define TOUCH_THRESHOLD 30 // Touch sensitivity threshold

#define NUM_LEDS 45       // Number of LEDs in the strip
#define DEBOUNCE_DELAY 50 // Debounce time for button in ms

#define MAX_TRIALS 20 // Max number of trials in a session

class EmergencyPowerStabilization
{
public:
    EmergencyPowerStabilization();

    // Core functionality
    void setup();
    void loop();
    void startInterrupt();
    void startSession();

    // Input mode control - can be called via command
    void setInputMode(int mode);

private:
    // Game state machine
    enum GameState
    {
        IDLE,                // Waiting for commands
        STARTED,             // Session started but no active game
        INTERRUPT_TRIGGERED, // Game has been triggered, waiting for user to start trials
        IN_PROGRESS,         // Trials are running
        TEST_MODE            // Debug mode
    };
    GameState gameState;

    // Input mode selection
    enum InputMode
    {
        BUTTON_INPUT,    // Traditional push button input
        CAPACITIVE_INPUT // Capacitive touch input
    };
    InputMode inputMode;

    //--------------------------------------------------
    // Command handling
    //--------------------------------------------------
    String inputBuffer;
    bool commandReady;

    void processCommand();
    void processConfigCommand(String params);
    void endSession();
    void handleDebug();

    //--------------------------------------------------
    // Session configuration
    //--------------------------------------------------
    String studyId;
    int sessionNumber;
    DataCollector dataCollector; // Integrated data collector

    //--------------------------------------------------
    // LED and display related
    //--------------------------------------------------
    Adafruit_NeoPixel strip;

    // Color definitions
    uint32_t redColor;
    uint32_t orangeColor;
    uint32_t greenColor;
    uint32_t blueColor;
    uint32_t purpleColor;
    uint32_t blackColor;

    // Rendering methods
    void renderLEDs();
    void drawCursor(int position);
    void renderAlarm();
    void fillStrip(uint32_t color);
    void blinkStrip(uint32_t color);
    uint32_t wheelColor(byte wheelPos);
    void displayDebugPattern(int pattern, bool buttonState, unsigned long currentTime);

    //--------------------------------------------------
    // Cursor management
    //--------------------------------------------------
    int cursorPosition;
    int cursorDirection; // 1: right, -1: left
    unsigned long lastCursorUpdateTime;
    int baseTraversalTimeMs; // Base time in milliseconds to traverse the entire strip
    int traversalTimeMs;     // Current time in milliseconds to traverse the entire strip
    int delayPerStep;        // Delay between cursor steps in milliseconds

    // Game configuration parameters
    float redZoneRatio;    // Percentage of strip for red zones
    float orangeZoneRatio; // Percentage of strip for orange zones
    float greenZoneRatio;  // Percentage of strip for green zone

    void updateCursor();
    int getZone(int position); // 0: red left, 1: orange left, 2: green, 3: orange right, 4: red right
    int getAccuracy(int position);
    void randomizeSpeed(int percentage);

    //--------------------------------------------------
    // Input handling
    //--------------------------------------------------
    // Button input
    bool lastButtonState;
    unsigned long lastDebounceTime;

    // Capacitive touch input
    int touchSensorValue;
    int touchThreshold;
    bool lastTouchState;
    unsigned long lastTouchTime;

    // New input abstraction methods
    void initializeInput();
    void checkInput();
    bool isInputPressed();
    bool isInputJustPressed();
    void handleInputPress(int position);

    // Specific input reading methods
    bool readButtonInput();
    bool readCapacitiveInput();

    //--------------------------------------------------
    // Game logic
    //--------------------------------------------------
    int totalTrials;
    int lastScores[MAX_TRIALS];
    int currentTrial;
    int successCount;

    void startTrials();
    void startNextTrial();
    void displayZoneResult(int zone);
    void endInterrupt();
    void printResults();

    //--------------------------------------------------
    // Debug mode
    //--------------------------------------------------
    int testPattern;
    unsigned long lastPatternChange;
};

#endif // EMERGENCY_POWER_STABILIZATION_H
