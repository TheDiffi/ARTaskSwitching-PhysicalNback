#ifndef NBACK_TASK_H
#define NBACK_TASK_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "data_collector.h"

//==============================================================================
// Hardware Configuration
//==============================================================================

// Pin configurations
#if defined(ESP32)
#define NEOPIXEL_PIN 32 // Pin connected to the NeoPixel for ESP32
#define INPUT_MODE CAPACITIVE_INPUT
#elif defined(ESP8266)
#define touchRead(p) (analogRead(p))
#define NEOPIXEL_PIN 4 // Pin connected to the NeoPixel for ESP8266 (D1 Mini)
#define INPUT_MODE BUTTON_INPUT
#else
#define NEOPIXEL_PIN 4 // Default pin if board cannot be determined
#define INPUT_MODE BUTTON_INPUT
#endif

#define BUTTON_CORRECT_PIN 16 // Pin connected to the button
#define BUTTON_WRONG_PIN 12   // Pin connected to the button

// #define NUM_PIXELS 1   // Number of NeoPixels in the strip
#define NUM_PIXELS 8 // Number of NeoPixels in the strip

// Capacitive touch configuration
#define TOUCH_CORRECT_PIN 14       // Pin connected to the capacitive touch sensor
#define TOUCH_WRONG_PIN 13         // Pin connected to the capacitive touch sensor
#define TOUCH_THRESHOLD_CORRECT 36 // Touch sensitivity threshold
#define TOUCH_THRESHOLD_WRONG 36   // Touch sensitivity threshold

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
    PURPLE = 4,
    WHITE = 5,       // Special case for visual feedback
    COLOR_COUNT = 6, // Total number of colors used
    COLORS_USED = 5  // Number of colors used in the task
};

//==============================================================================
// Task State Definitions
//==============================================================================

// State machine for the task
enum TaskState
{
    STATE_IDLE,       // Task not running, waiting for commands
    STATE_RUNNING,    // Task is actively running
    STATE_PAUSED,     // Task is temporarily paused
    STATE_DEBUG,      // Hardware debug mode active
    STATE_DATA_READY, // Task complete, data ready to send
    STATE_INPUT_MODE  // Input forwarding mode
};

// Input mode selection
enum InputMode
{
    BUTTON_INPUT,    // Traditional push button input
    CAPACITIVE_INPUT // Capacitive touch input
};

//==============================================================================
// Task-related Structures
//==============================================================================

// Trial state flags (using bit fields to save memory)
struct TrialFlags
{
    bool awaitingResponse : 1;        // Whether currently in response window
    bool targetTrial : 1;             // Whether current trial is a target
    bool feedbackActive : 1;          // Whether visual feedback is active
    bool buttonPressed : 1;           // Whether button was pressed during this trial
    bool responseIsConfirm : 1;       // Which response was made (confirm/incorrect)
    bool inInterStimulusInterval : 1; // Whether we're in the interval between stimuli
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
                   uint8_t numTrials, const String &studyId, uint16_t sessionNum, bool genSequence);

    void startTask();
    bool processSerialCommands(const String &command);

    // Input mode forwarding functions (separated for easy extraction to another class)
    void enterInputMode();
    void exitInputMode();
    void handleInputModeLoop();
    void sendInputEvent(const String &inputType, bool isPressed);

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
    unsigned long stimulusEndTime;   // When stimulus ended (ms)
    unsigned long feedbackStartTime; // When visual feedback started (ms)
    TrialFlags flags;                // Trial state flags
    TrialData trialData;             // Data for the current trial

    //--------------------------------------------------------------------------
    // Input Handling
    //--------------------------------------------------------------------------
    // Input mode
    InputMode inputMode;

    // Button input structures
    struct
    {
        int lastState;                  // Previous button state (HIGH/LOW)
        unsigned long lastDebounceTime; // Last time button state changed (ms)
        unsigned long debounceDelay;    // Debounce timeout (ms)
    } buttonCorrect;

    struct
    {
        int lastState;                  // Previous button state (HIGH/LOW)
        unsigned long lastDebounceTime; // Last time button state changed (ms)
        unsigned long debounceDelay;    // Debounce timeout (ms)
    } buttonWrong;

    // Capacitive touch input
    struct
    {
        int value;                      // Current touch sensor value
        int threshold;                  // Touch detection threshold
        bool lastState;                 // Previous touch state
        unsigned long lastDebounceTime; // Last time touch state changed (ms)
    } touchCorrect, touchWrong;

    // Input abstraction methods
    void initializeInput();
    bool readCorrectInput();
    bool readWrongInput();
    void checkInputs();
    bool isCorrectPressed();
    bool isWrongPressed();

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
    String study_id;             // Current study identifier

    //--------------------------------------------------------------------------
    // Command Processing Methods
    //--------------------------------------------------------------------------
    void processConfigCommand(const String &command);
    void sendData();
    void sendTimeSyncToMaster();

    //--------------------------------------------------------------------------
    // State Management Methods
    //--------------------------------------------------------------------------
    void generateSequence();
    void pauseTask(bool pause);
    void enterDebugMode();
    void endTask();

    //--------------------------------------------------------------------------
    // Trial Management Methods
    //--------------------------------------------------------------------------
    void manageTrials();
    void renderPixels();
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

    //--------------------------------------------------------------------------
    // Color Parsing Methods
    //--------------------------------------------------------------------------
    int parseColorName(const String &colorName);
    void parseAndSetColorSequence(const String &sequenceStr);
};

#endif // NBACK_TASK_H