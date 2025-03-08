#ifndef NBACK_TASK_H
#define NBACK_TASK_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "data_collector.h"

//==============================================================================
// Hardware Configuration
//==============================================================================

// Pin configurations
#define NEOPIXEL_PIN 6 // Pin connected to the NeoPixel
#define BUTTON_PIN 2   // Pin connected to the button
#define NUM_PIXELS 1   // Number of NeoPixels in the strip

//==============================================================================
// Task Parameters
//==============================================================================

// Task constants - default values, can be changed via config command
#define MAX_TRIALS 30 // Default, can be increased up to 50

//==============================================================================
// Color Definitions
//==============================================================================

// Color indices used for the task
enum ColorIndex
{
    RED = 0,
    GREEN = 1,
    BLUE = 2,
    YELLOW = 3,
    COLOR_COUNT = 4, // Total number of colors used
    WHITE = 99       // Special case for visual feedback
};

//==============================================================================
// Task State Definitions
//==============================================================================

// State machine for the task
enum TaskState
{
    STATE_IDLE,      // Task not running, waiting for commands
    STATE_RUNNING,   // Task is actively running
    STATE_PAUSED,    // Task is temporarily paused
    STATE_DEBUG,     // Hardware debug mode active
    STATE_DATA_READY // Task complete, data ready to send
};

//==============================================================================
// Task-related Structures
//==============================================================================

// Trial state flags (using bit fields to save memory)
struct TrialFlags
{
    bool awaitingResponse : 1; // Whether currently in response window
    bool targetTrial : 1;      // Whether current trial is a target
    bool feedbackActive : 1;   // Whether visual feedback is active
    bool buttonPressed : 1;    // Whether button was pressed during this trial
};

// Trial performance data
struct TrialData
{
    unsigned long reactionTime;      // Time between stimulus onset and response
    unsigned long stimulusOnsetTime; // When stimulus appeared (relative to session start)
    unsigned long responseTime;      // When response occurred (relative to session start)
    unsigned long stimulusEndTime;   // When stimulus disappeared (relative to session start)
};

//==============================================================================
// Main N-Back Task Class
//==============================================================================

class NBackTask
{
public:
    // Constructor and destructor
    NBackTask();
    ~NBackTask();

    // Main Arduino interface functions
    void setup();
    void loop();

    // Configuration function (public to allow direct configuration)
    bool configure(uint16_t stimDuration, uint16_t interStimulusInt, uint8_t nBackLvl,
                   uint8_t numTrials, const char *studyId, uint16_t sessionNum);

private:
    //--------------------------------------------------------------------------
    // Timing Parameters
    //--------------------------------------------------------------------------
    struct
    {
        uint16_t stimulusDuration;      // How long each stimulus is shown (ms)
        uint16_t interStimulusInterval; // Time between stimuli (ms)
        uint16_t feedbackDuration;      // Duration of visual feedback (ms)
        uint16_t debugColorDuration;    // Time for each color in debug mode (ms)
    } timing;

    //--------------------------------------------------------------------------
    // Task Parameters
    //--------------------------------------------------------------------------
    int nBackLevel; // N-back level (e.g., 2 for 2-back)
    int maxTrials;  // Total number of trials in a session

    //--------------------------------------------------------------------------
    // Task State
    //--------------------------------------------------------------------------
    TaskState state;                 // Current state of the task
    int currentTrial;                // Current trial number (0-based)
    unsigned long trialStartTime;    // When current trial started (ms)
    unsigned long feedbackStartTime; // When visual feedback started (ms)
    TrialFlags flags;                // Trial state flags
    TrialData trialData;             // Data for the current trial

    //--------------------------------------------------------------------------
    // Button Handling
    //--------------------------------------------------------------------------
    struct
    {
        int lastState;                  // Previous button state (HIGH/LOW)
        unsigned long lastDebounceTime; // Last time button state changed (ms)
        unsigned long debounceDelay;    // Debounce timeout (ms)
    } button;

    //--------------------------------------------------------------------------
    // Debug Mode Variables
    //--------------------------------------------------------------------------
    int debugColorIndex;               // Current color index in debug cycle
    unsigned long lastColorChangeTime; // Last time debug color changed (ms)

    //--------------------------------------------------------------------------
    // Performance Metrics
    //--------------------------------------------------------------------------
    struct
    {
        int correctResponses;            // Number of correct button presses (hits)
        int falseAlarms;                 // Number of incorrect button presses (false positives)
        int missedTargets;               // Number of missed targets (false negatives)
        unsigned long totalReactionTime; // Sum of all correct reaction times (ms)
        int reactionTimeCount;           // Count of measured reaction times
    } metrics;

    //--------------------------------------------------------------------------
    // Hardware and Data
    //--------------------------------------------------------------------------
    Adafruit_NeoPixel pixels;     // NeoPixel control object
    uint32_t colors[COLOR_COUNT]; // Array of NeoPixel color values
    int *colorSequence;           // Dynamically allocated array for color sequence

    // Data collection
    DataCollector dataCollector; // Data collector for research data
    char study_id[10];           // Current study identifier

    //--------------------------------------------------------------------------
    // Command Processing Methods
    //--------------------------------------------------------------------------
    void processSerialCommands();
    void processConfigCommand(const String &command);
    void sendData();

    //--------------------------------------------------------------------------
    // State Management Methods
    //--------------------------------------------------------------------------
    void startTask();
    void generateSequence();
    void pauseTask(bool pause);
    void enterDebugMode();
    void endTask();

    //--------------------------------------------------------------------------
    // Trial Management Methods
    //--------------------------------------------------------------------------
    void manageTrials();
    void startNextTrial();
    void handleButtonPress();
    void evaluateTrialOutcome();

    //--------------------------------------------------------------------------
    // Visual Feedback Methods
    //--------------------------------------------------------------------------
    void handleVisualFeedback(boolean startFeedback);
    void setNeoPixelColor(int colorIndex);

    //--------------------------------------------------------------------------
    // Debug Mode Methods
    //--------------------------------------------------------------------------
    void runDebugMode();

    //--------------------------------------------------------------------------
    // Utility Methods
    //--------------------------------------------------------------------------
    void resetMetrics();
    void reportResults();
};

#endif // NBACK_TASK_H