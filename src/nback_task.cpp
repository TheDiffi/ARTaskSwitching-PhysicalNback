#include "nback_task.h"

//==============================================================================
// Constructor & Destructor
//==============================================================================

NBackTask::NBackTask()
    : pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
      nBackLevel(2),
      maxTrials(MAX_TRIALS),
      state(STATE_IDLE),
      currentTrial(0),
      trialStartTime(0),
      stimulusEndTime(0),
      feedbackStartTime(0),
      debugColorIndex(0),
      lastColorChangeTime(0),
      colorSequence(nullptr)
{
    // Set default study ID
    strcpy(study_id, "DEFAULT");

    // Initialize timing parameters (in milliseconds)
    timing.stimulusDuration = 2000;      // How long each stimulus is shown
    timing.interStimulusInterval = 2000; // Time between stimuli
    timing.feedbackDuration = 100;       // Duration of button press feedback
    timing.debugColorDuration = 1000;    // How long each color shows in debug mode

    // Initialize trial state flags
    flags.awaitingResponse = false;
    flags.targetTrial = false;
    flags.feedbackActive = false;
    flags.buttonPressed = false;
    flags.responseIsConfirm = false;
    flags.inInterStimulusInterval = false;

    // Initialize button 1 debouncing variables
    buttonCorrect.lastState = HIGH;
    buttonCorrect.lastDebounceTime = 0;
    buttonCorrect.debounceDelay = 50;

    // Initialize button 2 debouncing variables
    buttonWrong.lastState = HIGH;
    buttonWrong.lastDebounceTime = 0;
    buttonWrong.debounceDelay = 50;

    // Initialize trial data recording
    trialData.reactionTime = 0;
    trialData.stimulusOnsetTime = 0;
    trialData.responseTime = 0;
    trialData.stimulusEndTime = 0;

    // Reset performance metrics
    resetMetrics();

    // Initialize the NeoPixel colors array
    colors[RED] = pixels.Color(255, 0, 0);      // Red
    colors[GREEN] = pixels.Color(0, 255, 0);    // Green
    colors[BLUE] = pixels.Color(0, 0, 255);     // Blue
    colors[YELLOW] = pixels.Color(255, 255, 0); // Yellow
}

NBackTask::~NBackTask()
{
    // Free dynamically allocated memory
    if (colorSequence != nullptr)
    {
        delete[] colorSequence;
        colorSequence = nullptr;
    }
}

//==============================================================================
// Main Interface Functions
//==============================================================================

void NBackTask::setup()
{
    // Initialize NeoPixel
    pixels.begin();
    pixels.clear();
    pixels.show();

    // Initialize button with internal pull-up resistor
    pinMode(BUTTON_CORRECT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_WRONG_PIN, INPUT_PULLUP);

    // Initialize serial communication
    Serial.begin(9600);

    // Print welcome message and command list
    Serial.println(F("N-Back Task"));
    Serial.println(F("Commands:"));
    Serial.println(F("- 'debug' to enter debug mode and test hardware"));
    Serial.println(F("- 'exit-debug' to exit debug mode"));
    Serial.println(F("- 'start' to begin task"));
    Serial.println(F("- 'pause' to pause/resume task"));
    Serial.println(F("- 'exit' to cancel the current task and discard data"));
    Serial.println(F("- 'get_data' to retrieve collected data"));
    Serial.println(F("- 'config stimDur,interStimInt,nBackLvl,trials,studyId,sessionNum' to configure all parameters"));
    Serial.println(F("ready"));

    // Allocate memory for color sequence
    colorSequence = new int[maxTrials];

    // Generate initial random sequence
    generateSequence();
}

void NBackTask::loop()
{
    // Process any serial commands
    processSerialCommands();

    // Handle tasks based on current state
    switch (state)
    {
    case STATE_DEBUG:
        // In debug mode, cycle through colors and respond to button presses
        runDebugMode();
        break;

    case STATE_PAUSED:
        // When paused, do nothing until resumed
        break;

    case STATE_RUNNING:
        // Handle ongoing task

        // Check if visual feedback needs to be ended
        handleVisualFeedback(false);

        // If not in feedback, manage trial progression
        if (!flags.feedbackActive)
        {
            manageTrials();
        }

        // Always check for button press when running
        handleButtonPress();
        break;

    case STATE_IDLE:
    case STATE_DATA_READY:
        // Nothing to do in idle or data ready states
        break;
    }
}

//==============================================================================
// Command Processing
//==============================================================================

void NBackTask::processSerialCommands()
{
    if (Serial.available() > 0)
    {
        // Read and trim the incoming command
        String command = Serial.readStringUntil('\n');
        command.trim();

        // Handle different commands
        if (command == "start")
        {
            // Exit debug mode if active
            if (state == STATE_DEBUG)
            {
                Serial.println(F("exiting debug mode"));
                pixels.clear();
                pixels.show();
            }
            startTask();
        }
        else if (command == "pause")
        {
            // Toggle pause state
            if (state == STATE_RUNNING || state == STATE_PAUSED)
            {
                pauseTask(state != STATE_PAUSED);
            }
        }
        else if (command == "debug")
        {
            enterDebugMode();
        }
        else if (command == "exit-debug")
        {
            // Exit debug mode and return to IDLE state
            if (state == STATE_DEBUG)
            {
                Serial.println(F("exiting debug mode"));
                pixels.clear();
                pixels.show();
                state = STATE_IDLE;
                Serial.println(F("ready"));
            }
        }
        else if (command == "exit")
        {
            // Cancel the current study and discard collected data
            if (state == STATE_RUNNING || state == STATE_PAUSED)
            {
                state = STATE_IDLE;
                pixels.clear();
                pixels.show();
                dataCollector.reset(); // Discard collected data
                Serial.println(F("exiting"));
                Serial.println(F("ready"));
            }
            else if (state == STATE_DATA_READY)
            {
                state = STATE_IDLE;
                dataCollector.reset(); // Discard collected data
                Serial.println(F("exiting"));
                Serial.println(F("ready"));
            }
        }
        else if (command == "get_data")
        {
            // Send collected data over serial if available
            if (state == STATE_DATA_READY)
            {
                sendData();
            }
            else
            {
                Serial.println(F("No data available. Run task first."));
            }
        }
        else if (command.startsWith("config "))
        {
            processConfigCommand(command);
        }
    }
}

void NBackTask::processConfigCommand(const String &command)
{
    // Parse configuration parameters
    String configStr = command.substring(7);

    // Variables to store parsed values
    uint16_t params[4] = {0}; // For numeric parameters: stimulus duration, ISI, n-back level, trials
    char studyId[10] = {0};   // For study ID
    uint16_t sessionNum = 0;  // For session number

    // Parse comma-separated values using a more streamlined approach
    int paramIndex = 0;
    int startPos = 0;
    int commaPos = -1;

    // Extract each parameter one by one
    while (paramIndex < 6 && (commaPos = configStr.indexOf(',', startPos)) != -1)
    {
        if (paramIndex < 4)
        {
            // First 4 parameters are numeric
            params[paramIndex] = configStr.substring(startPos, commaPos).toInt();
        }
        else if (paramIndex == 4)
        {
            // 5th parameter is the study ID
            configStr.substring(startPos, commaPos).toCharArray(studyId, sizeof(studyId));
        }
        startPos = commaPos + 1;
        paramIndex++;
    }

    // Last parameter (no comma after it)
    if (paramIndex == 5 && startPos < configStr.length())
    {
        // Last parameter is session number
        sessionNum = configStr.substring(startPos).toInt();
        paramIndex++;
    }

    // Apply configuration if all 6 parameters were found
    if (paramIndex == 6)
    {
        if (configure(params[0], params[1], params[2], params[3], studyId, sessionNum))
        {
            Serial.println(F("Configuration applied successfully"));
        }
        else
        {
            Serial.println(F("Failed to apply configuration - invalid parameters"));
        }
    }
    else
    {
        Serial.println(F("Invalid config format. Use: config stimDuration,interStimulusInterval,nBackLevel,trialsNumber,study_id,session_number"));
    }
}

void NBackTask::sendData()
{
    Serial.print(F("Sending data for "));
    Serial.print(dataCollector.getTrialCount());
    Serial.println(F(" recorded trials..."));

    dataCollector.sendDataOverSerial();

    // Return to idle state
    state = STATE_IDLE;
    Serial.println(F("data-completed"));
}

//==============================================================================
// State Management
//==============================================================================

void NBackTask::startTask()
{
    // Reset performance metrics for the new task
    resetMetrics();

    // Reset trial state
    currentTrial = 0;
    flags.awaitingResponse = false;
    flags.targetTrial = false;
    flags.feedbackActive = false;
    flags.inInterStimulusInterval = false;

    // Generate a new sequence for this task
    generateSequence();

    // Reset data collector for a new session
    dataCollector.reset();

    // Start the task
    state = STATE_RUNNING;

    // Report task settings
    Serial.println(F("Task started"));
    Serial.print(F("N-back level: "));
    Serial.println(nBackLevel);
    Serial.print(F("Study ID: "));
    Serial.println(study_id);

    // Start first trial
    startNextTrial();
}

// TODO: make this a fixed sequence for each task to make study results comparable
void NBackTask::generateSequence()
{
    // Seed the random number generator
    randomSeed(analogRead(A0));

    // Aim for approximately 25% targets
    int targetCount = maxTrials / 4;

    // First, fill with random colors
    for (int i = 0; i < maxTrials; i++)
    {
        colorSequence[i] = random(COLOR_COUNT);
    }

    // Then ensure we have target trials (n-back matches) after position n
    int targetsPlaced = 0;
    while (targetsPlaced < targetCount)
    {
        // Choose a random position after the n-back position
        int pos = random(nBackLevel, maxTrials);

        // Make this position match the n-back position (creating a target)
        colorSequence[pos] = colorSequence[pos - nBackLevel];
        targetsPlaced++;
    }

    // Print the sequence with target indicators for debugging
    Serial.println(F("Sequence generated:"));
    for (int i = 0; i < maxTrials; i++)
    {
        Serial.print(colorSequence[i]);

        // Mark target positions with an asterisk
        if (i >= nBackLevel && colorSequence[i] == colorSequence[i - nBackLevel])
        {
            Serial.print(F("*"));
        }
        Serial.print(F(" "));
    }
    Serial.println();
}

void NBackTask::pauseTask(bool pause)
{
    // Update state based on pause flag
    state = pause ? STATE_PAUSED : STATE_RUNNING;
    Serial.println(pause ? F("Task paused") : F("Task resumed"));
}

void NBackTask::enterDebugMode()
{
    // Enter debug mode
    state = STATE_DEBUG;
    debugColorIndex = 0;
    lastColorChangeTime = millis();
    flags.feedbackActive = false;

    Serial.println(F("*** DEBUG MODE ***"));
    Serial.println(F("Testing NeoPixel and button. NeoPixel will cycle through colors."));
    Serial.println(F("Press the button to test it."));
    Serial.println(F("Send 'exit-debug' to return to IDLE state or 'start' to begin task."));

    // Show first color
    setNeoPixelColor(debugColorIndex);
}

void NBackTask::endTask()
{
    state = STATE_DATA_READY;
    pixels.clear();
    pixels.show();

    reportResults();

    Serial.println(F("task-completed"));
}

bool NBackTask::configure(uint16_t stimDuration, uint16_t interStimulusInt, uint8_t nBackLvl,
                          uint8_t numTrials, const char *studyId, uint16_t sessionNum)
{
    // Validate parameters (basic sanity checks)
    if (stimDuration < 100 || interStimulusInt < 100 || nBackLvl < 1 ||
        numTrials < 5 || numTrials > 50 || strlen(studyId) == 0)
    {
        return false;
    }

    // Only allow configuration when not running a task
    if (state == STATE_RUNNING || state == STATE_PAUSED)
    {
        return false;
    }

    // Apply timing and level parameters
    timing.stimulusDuration = stimDuration;
    timing.interStimulusInterval = interStimulusInt;
    nBackLevel = nBackLvl;

    // Handle trial count change (requires memory reallocation)
    if (numTrials != maxTrials)
    {
        // Free old sequence if it exists
        if (colorSequence != nullptr)
        {
            delete[] colorSequence;
        }

        // Set new trial count
        maxTrials = numTrials;

        // Allocate new sequence array
        colorSequence = new int[maxTrials];
        if (colorSequence == nullptr)
        {
            // Memory allocation failed
            return false;
        }
    }

    // Set study ID (with safety null termination)
    strncpy(study_id, studyId, sizeof(study_id) - 1);
    study_id[sizeof(study_id) - 1] = '\0';

    // Initialize data collector with study information and session number
    dataCollector.begin(study_id, sessionNum);

    // Generate new sequence with updated parameters
    generateSequence();

    // Print confirmation of new settings
    Serial.println(F("Configuration updated:"));
    Serial.print(F("Stimulus Duration: "));
    Serial.print(timing.stimulusDuration);
    Serial.println(F("ms"));
    Serial.print(F("Inter-Stimulus Interval: "));
    Serial.print(timing.interStimulusInterval);
    Serial.println(F("ms"));
    Serial.print(F("N-back Level: "));
    Serial.println(nBackLevel);
    Serial.print(F("Number of Trials: "));
    Serial.println(maxTrials);
    Serial.print(F("Study ID: "));
    Serial.println(study_id);
    Serial.print(F("Session Number: "));
    Serial.println(sessionNum);

    return true;
}

//==============================================================================
// Trial Management
//==============================================================================

void NBackTask::manageTrials()
{
    // Only manage trials when awaiting response or in inter-stimulus interval
    if (!flags.awaitingResponse && !flags.inInterStimulusInterval)
    {
        return;
    }

    unsigned long currentTime = millis();

    // If in response window and stimulus duration has passed
    if (flags.awaitingResponse && currentTime - trialStartTime > timing.stimulusDuration)
    {
        // Turn off the pixel
        pixels.clear();
        pixels.show();
        flags.awaitingResponse = false;

        // Record when the stimulus ended
        stimulusEndTime = currentTime;

        // Evaluate the trial outcome at the end
        evaluateTrialOutcome();

        // Enter inter-stimulus interval state
        flags.inInterStimulusInterval = true;
    }

    // If in inter-stimulus interval and enough time has passed
    if (flags.inInterStimulusInterval &&
        currentTime - stimulusEndTime > timing.interStimulusInterval)
    {
        // Exit inter-stimulus interval state
        flags.inInterStimulusInterval = false;

        // Move to next trial if not at the end
        if (currentTrial < maxTrials - 1)
        {
            currentTrial++;
            startNextTrial();
        }
        else
        {
            // End of task
            endTask();
        }
    }
}

void NBackTask::evaluateTrialOutcome()
{
    // Record the stimulus end time relative to session start
    trialData.stimulusEndTime = millis() - dataCollector.getSessionStartTime();

    // Determine trial outcome:
    // 0. No response = Missed target (false negative)
    // 1. Target trial + response is confirm = Correct response
    // 2. Target trial + response is not confirm = Missed target (false negative)
    // 3. Non-target trial + response is confirm = False alarm (false positive)
    // 4. Non-target trial + response is not confirm = Correct rejection

    bool isCorrect = false;

    // Target trial
    if (!flags.buttonPressed)
    {
        // Missed target (miss = mistake)
        metrics.missedTargets++;
        isCorrect = false;
        Serial.println(F("NO RESPONSE!"));
    }
    else
    {
        // Add reaction time to totals (only for responses)
        metrics.totalReactionTime += trialData.reactionTime;
        metrics.reactionTimeCount++;

        if (flags.targetTrial && currentTrial >= nBackLevel)
        {
            if (flags.responseIsConfirm)
            {
                // Correct response (hit)
                metrics.correctResponses++;
                isCorrect = true;

                Serial.println(F("CORRECT RESPONSE!"));
                Serial.print(F("Reaction time: "));
                Serial.print(trialData.reactionTime);
                Serial.println(F(" ms"));
            }
            else
            {
                // Missed target (false negative)
                metrics.missedTargets++;
                isCorrect = false;
                Serial.println(F("MISSED TARGET!"));
            }
        }
        else if (!flags.targetTrial)
        {
            if (flags.responseIsConfirm)
            {
                // False alarm (false positive)
                metrics.falseAlarms++;
                isCorrect = false;
                Serial.println(F("FALSE ALARM!"));
                Serial.print(F("Reaction time: "));
                Serial.print(trialData.reactionTime);
                Serial.println(F(" ms (not counted in average)"));
            }
            else
            {
                // Correct rejection
                isCorrect = true;
                Serial.println(F("CORRECT REJECTION"));
            }
        }
    }

    // Record the complete trial data in one row
    dataCollector.recordCompletedTrial(
        currentTrial + 1,                                 // 1-based stimulus number
        colorSequence[currentTrial],                      // stimulus color
        flags.targetTrial,                                // is_target
        flags.buttonPressed,                              // response_made
        isCorrect,                                        // is_correct
        trialData.stimulusOnsetTime,                      // stimulus_onset_time
        flags.buttonPressed ? trialData.responseTime : 0, // response_time
        flags.buttonPressed ? trialData.reactionTime : 0, // reaction_time
        trialData.stimulusEndTime                         // stimulus_end_time
    );
}

void NBackTask::startNextTrial()
{
    // Display the current color
    setNeoPixelColor(colorSequence[currentTrial]);

    // Record start time
    trialStartTime = millis();
    trialData.stimulusOnsetTime = trialStartTime - dataCollector.getSessionStartTime();

    // Set trial state
    flags.awaitingResponse = true;
    flags.buttonPressed = false; // Reset button press tracking for new trial

    // Check if this is a target trial (n-back match)
    flags.targetTrial = (currentTrial >= nBackLevel) &&
                        (colorSequence[currentTrial] == colorSequence[currentTrial - nBackLevel]);

    // Display trial information
    Serial.print(F("Trial "));
    Serial.print(currentTrial + 1);
    Serial.print(F(": Color "));
    Serial.print(colorSequence[currentTrial]);
    if (flags.targetTrial)
    {
        Serial.println(F(" (TARGET)"));
    }
    else
    {
        Serial.println();
    }
}

void NBackTask::handleButtonPress()
{
    // Only process button in running state and not during feedback and if not pressed yet
    if (state != STATE_RUNNING || flags.feedbackActive || flags.buttonPressed)
    {
        return;
    }

    // Read button state (LOW when pressed due to INPUT_PULLUP)
    int buttonCorrectReading = digitalRead(BUTTON_CORRECT_PIN);
    int buttonWrongReading = digitalRead(BUTTON_WRONG_PIN);

    // Handle debounced button press for correct button
    if ((millis() - buttonCorrect.lastDebounceTime) > buttonCorrect.debounceDelay)
    {
        // Button press detected (LOW due to pull-up resistor)
        if (buttonCorrectReading == LOW && buttonCorrect.lastState == HIGH && flags.awaitingResponse)
        {
            // Only record reaction time for the first button press in this trial
            // Calculate and store timing data
            trialData.reactionTime = millis() - trialStartTime;
            trialData.responseTime = millis() - dataCollector.getSessionStartTime();

            // Mark that the "correct" button was pressed for this trial
            flags.buttonPressed = true;
            flags.responseIsConfirm = true;

            Serial.println(F("Confirm Button pressed"));

            // Provide visual feedback for button press
            handleVisualFeedback(true);
        }
    }

    // Handle debounced button press for wrong button
    if ((millis() - buttonWrong.lastDebounceTime) > buttonWrong.debounceDelay)
    {
        // Button press detected (LOW due to pull-up resistor)
        if (buttonWrongReading == LOW && buttonWrong.lastState == HIGH && flags.awaitingResponse)
        {
            // Only record reaction time for the first button press in this trial
            // Calculate and store timing data
            trialData.reactionTime = millis() - trialStartTime;
            trialData.responseTime = millis() - dataCollector.getSessionStartTime();

            // Mark that the "wrong" button was pressed for this trial
            flags.buttonPressed = true;
            flags.responseIsConfirm = false;

            Serial.println(F("Wrong button pressed"));

            // Provide visual feedback for button press
            handleVisualFeedback(true);
        }
    }

    // Debounce logic - track state changes
    if (buttonCorrectReading != buttonCorrect.lastState)
    {
        buttonCorrect.lastDebounceTime = millis();
    }
    if (buttonWrongReading != buttonWrong.lastState)
    {
        buttonWrong.lastDebounceTime = millis();
    }

    // Update button state
    buttonCorrect.lastState = buttonCorrectReading;
    buttonWrong.lastState = buttonWrongReading;
}

//==============================================================================
// Visual Feedback
//==============================================================================

void NBackTask::handleVisualFeedback(boolean startFeedback)
{
    if (startFeedback)
    {
        // Start/restart visual feedback with white flash
        for (int i = 0; i < NUM_PIXELS; i++)
        {
            pixels.setPixelColor(i, pixels.Color(255, 255, 255));
        }
        pixels.show();
        flags.feedbackActive = true;
        feedbackStartTime = millis();
        return;
    }

    // If not starting feedback, check if we need to end it
    if (flags.feedbackActive && (millis() - feedbackStartTime > timing.feedbackDuration))
    {
        // End the feedback flash
        flags.feedbackActive = false;

        // Restore appropriate display based on current state
        if (state == STATE_DEBUG)
        {
            // In debug mode, return to the current color
            setNeoPixelColor(debugColorIndex);
        }
        else if (state == STATE_RUNNING && flags.awaitingResponse)
        {
            // In task mode during stimulus, show the current color
            setNeoPixelColor(colorSequence[currentTrial]);
        }
        else
        {
            // Otherwise, turn off the pixel
            pixels.clear();
            pixels.show();
        }
    }
}

void NBackTask::setNeoPixelColor(int colorIndex)
{
    // Set the NeoPixel to the specified color
    if (colorIndex >= 0 && colorIndex < COLOR_COUNT)
    {
        for (int i = 0; i < NUM_PIXELS; i++)
        {
            pixels.setPixelColor(i, colors[colorIndex]);
        }
        pixels.show();
    }
    else if (colorIndex == WHITE)
    {
        // Special case for white (used for feedback)
        for (int i = 0; i < NUM_PIXELS; i++)
        {
            pixels.setPixelColor(i, pixels.Color(255, 255, 255));
        }
        pixels.show();
    }
}

//==============================================================================
// Debug Mode
//==============================================================================

void NBackTask::runDebugMode()
{
    // First handle any ongoing visual feedback
    handleVisualFeedback(false);

    // If feedback is active, don't do other debug operations
    if (flags.feedbackActive)
    {
        return;
    }

    unsigned long currentTime = millis();

    // Cycle through colors automatically at set intervals
    if (currentTime - lastColorChangeTime > timing.debugColorDuration)
    {
        // Move to next color in cycle
        debugColorIndex = (debugColorIndex + 1) % COLOR_COUNT;
        setNeoPixelColor(debugColorIndex);

        // Report current color
        Serial.print(F("Debug: Showing color "));
        Serial.print(debugColorIndex);

        // Show color name for readability
        const char *colorNames[] = {"RED", "GREEN", "BLUE", "YELLOW"};
        if (debugColorIndex < COLOR_COUNT)
        {
            Serial.print(F(" ("));
            Serial.print(colorNames[debugColorIndex]);
            Serial.println(F(")"));
        }
        else
        {
            Serial.println();
        }

        lastColorChangeTime = currentTime;
    }

    // Check for button presses in debug mode
    int readingCorrect = digitalRead(BUTTON_CORRECT_PIN);

    if ((millis() - buttonCorrect.lastDebounceTime) > buttonCorrect.debounceDelay)
    {
        // Button press detected
        if (readingCorrect == LOW && buttonCorrect.lastState == HIGH)
        {
            Serial.println(F("Debug: CONFIRM BUTTON PRESSED!"));

            // Provide visual feedback for button press
            handleVisualFeedback(true);
        }
    }

    int readingWrong = digitalRead(BUTTON_WRONG_PIN);

    if ((millis() - buttonWrong.lastDebounceTime) > buttonWrong.debounceDelay)
    {
        // Button press detected
        if (readingWrong == LOW && buttonWrong.lastState == HIGH)
        {
            Serial.println(F("Debug: WRONG BUTTON PRESSED!"));

            // Provide visual feedback for button press
            handleVisualFeedback(true);
        }
    }

    // Debounce logic - track state changes
    if (readingCorrect != buttonCorrect.lastState)
    {
        buttonCorrect.lastDebounceTime = millis();
    }
    if (readingWrong != buttonWrong.lastState)
    {
        buttonWrong.lastDebounceTime = millis();
    }

    // Update button state
    buttonCorrect.lastState = readingCorrect;
    buttonWrong.lastState = readingWrong;
}

//==============================================================================
// Utility Functions
//==============================================================================

void NBackTask::resetMetrics()
{
    metrics.correctResponses = 0;  // Hits
    metrics.falseAlarms = 0;       // False positives
    metrics.missedTargets = 0;     // False negatives
    metrics.totalReactionTime = 0; // Sum of reaction times
    metrics.reactionTimeCount = 0; // Count of measured reaction times
}

void NBackTask::reportResults()
{
    int totalTargets = metrics.correctResponses + metrics.missedTargets;
    float hitRate = (totalTargets > 0) ? (float)metrics.correctResponses / totalTargets * 100.0 : 0;
    float averageRT = (metrics.reactionTimeCount > 0) ? (float)metrics.totalReactionTime / metrics.reactionTimeCount : 0;

    // Format timestamp for session duration
    char timestampBuffer[16];
    uint32_t sessionDuration = millis() - dataCollector.getSessionStartTime();
    dataCollector.formatTimestamp(sessionDuration, timestampBuffer, sizeof(timestampBuffer));

    // Print performance summary
    Serial.println(F("\n=== TASK COMPLETE ==="));
    Serial.print(F("N-Back Level: "));
    Serial.println(nBackLevel);
    Serial.print(F("Total Trials: "));
    Serial.println(maxTrials);
    Serial.print(F("Total Targets: "));
    Serial.println(totalTargets);
    Serial.print(F("Correct Responses: "));
    Serial.println(metrics.correctResponses);
    Serial.print(F("False Alarms: "));
    Serial.println(metrics.falseAlarms);
    Serial.print(F("Missed Targets: "));
    Serial.println(metrics.missedTargets);
    Serial.print(F("Hit Rate: "));
    Serial.print(hitRate);
    Serial.println(F("%"));
    Serial.print(F("Average Reaction Time (responses only): "));
    Serial.print(averageRT);
    Serial.println(F(" ms"));
    Serial.print(F("Session Duration: "));
    Serial.println(timestampBuffer);
    Serial.println(F("======================"));
}