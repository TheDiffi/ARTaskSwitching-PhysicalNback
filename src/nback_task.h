#ifndef NBACK_TASK_H
#define NBACK_TASK_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// Pin configurations
#define NEOPIXEL_PIN 6
#define BUTTON_PIN 2
#define NUM_PIXELS 1

// Task constants
#define MAX_TRIALS 30

// Color definitions
enum ColorIndex
{
    RED = 0,
    GREEN = 1,
    BLUE = 2,
    YELLOW = 3,
    COLOR_COUNT = 4,
    WHITE = 99 // Special case for feedback
};

// Task states
enum TaskState
{
    STATE_IDLE,    // Task not running
    STATE_RUNNING, // Task is running
    STATE_PAUSED,  // Task is paused
    STATE_DEBUG    // Debug mode active
};

// Trial flags
struct TrialFlags
{
    bool awaitingResponse : 1; // If we're in response window
    bool targetTrial : 1;      // If current trial is a target
    bool feedbackActive : 1;   // Visual feedback is active
};

class NBackTask
{
public:
    // Constructor
    NBackTask();

    // Destructor (to free dynamic memory)
    ~NBackTask();

    // Main interface functions
    void setup();
    void loop();

private:
    // Timing parameters
    struct
    {
        uint16_t stimulusDuration;      // Stimulus display time in ms
        uint16_t interStimulusInterval; // Time between stimuli in ms
        uint16_t feedbackDuration;      // Duration of visual feedback
        uint16_t debugColorDuration;    // Duration for each color in debug mode
    } timing;

    // Task parameters
    int nBackLevel; // N-back level (default is 2)
    int maxTrials;  // Total number of trials in a session

    // Task state
    TaskState state;                 // Current state of the task
    int currentTrial;                // Current trial number
    unsigned long trialStartTime;    // When current trial started
    unsigned long feedbackStartTime; // When visual feedback started
    TrialFlags flags;                // Trial state flags

    // Button handling
    struct
    {
        int lastState;                  // Previous button state
        unsigned long lastDebounceTime; // Last time button state changed
        unsigned long debounceDelay;    // Debounce timeout
    } button;

    // Debug mode variables
    unsigned long lastColorChangeTime; // Last time debug color changed
    int debugColorIndex;               // Current color in debug cycle

    // Performance metrics
    struct
    {
        int correctResponses;            // Number of correct button presses
        int falseAlarms;                 // Number of incorrect button presses
        int missedTargets;               // Number of missed targets
        unsigned long totalReactionTime; // Sum of all reaction times
        int reactionTimeCount;           // Count of measured reaction times
    } metrics;

    // NeoPixel and sequence
    Adafruit_NeoPixel pixels;     // NeoPixel object
    uint32_t colors[COLOR_COUNT]; // Array of color values
    int *colorSequence;           // Array to store color sequence

    // Command processing
    void processSerialCommands();

    // State management
    void startTask();
    void generateSequence();
    void pauseTask(bool pause);
    void enterDebugMode();
    void endTask();

    // Trial management
    void manageTrials();
    void startNextTrial();
    void handleButtonPress();
    void handleTaskResponse(unsigned long reactionTime);

    // Visual feedback
    void handleVisualFeedback(boolean startFeedback);
    void setNeoPixelColor(int colorIndex);

    // Debug mode
    void runDebugMode();

    // Utility functions
    void resetMetrics();
    void reportResults();
};

#endif // NBACK_TASK_H