#ifndef NBACK_TASK_H
#define NBACK_TASK_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// Define pins
#define NEOPIXEL_PIN 6
#define BUTTON_PIN 2

// Define NeoPixel parameters
#define NUM_PIXELS 1

// Task configuration
#define MAX_TRIALS 30

class NBackTask
{
public:
    // Constructor
    NBackTask();

    // Main functions
    void setup();
    void loop();
    void processSerialCommands();

    // Task control functions
    void startTask();
    void generateSequence();
    void manageTrials();
    void startNextTrial();
    void evaluateButtonState();
    void handleTaskButtonPress();
    void runDebugMode();
    void endTask();

private:
    // NeoPixel object
    Adafruit_NeoPixel pixels;

    // N-Back Task Parameters
    int nBackLevel;            // N-back level (default is 2)
    int stimulusDuration;      // Stimulus display time in ms
    int interStimulusInterval; // Time between stimuli in ms
    int maxTrials;             // Total number of trials in a session

    // Colors (RGB values)
    uint32_t colors[4];
    int numColors;

    // Task variables
    int *colorSequence;           // Array to store color sequence
    int currentTrial;             // Current trial number
    unsigned long trialStartTime; // When current trial started
    unsigned long reactionTime;   // Time taken to react
    boolean awaitingResponse;     // If we're in response window
    boolean targetTrial;          // If current trial is a target
    boolean paused;               // Pause state
    boolean taskRunning;          // Is task currently running

    // Performance metrics
    int correctResponses;            // Number of correct button presses
    int falseAlarms;                 // Number of incorrect button presses
    int missedTargets;               // Number of missed targets
    unsigned long totalReactionTime; // Sum of all reaction times
    int reactionTimeCount;           // Count of measured reaction times

    // Button debouncing
    int lastButtonState; // Default is HIGH due to pull-up
    unsigned long lastDebounceTime;
    unsigned long debounceDelay;

    // Debug mode variables
    boolean debugMode;
    unsigned long lastColorChangeTime;
    int debugColorIndex;
    const int debugColorDuration = 10000; // 1 second per color in debug mode
};

#endif // NBACK_TASK_H