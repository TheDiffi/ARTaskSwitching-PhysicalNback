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
      feedbackStartTime(0),
      debugColorIndex(0),
      lastColorChangeTime(0),
      colorSequence(nullptr)
{
    // Initialize timing parameters
    timing.stimulusDuration = 2000;
    timing.interStimulusInterval = 2000;
    timing.feedbackDuration = 100;
    timing.debugColorDuration = 1000;

    // Initialize trial flags
    flags.awaitingResponse = false;
    flags.targetTrial = false;
    flags.feedbackActive = false;
    flags.buttonPressed = false;

    // Initialize button
    button.lastState = HIGH;
    button.lastDebounceTime = 0;
    button.debounceDelay = 50;

    // Reset metrics
    resetMetrics();

    // Initialize the colors array
    colors[RED] = pixels.Color(255, 0, 0);
    colors[GREEN] = pixels.Color(0, 255, 0);
    colors[BLUE] = pixels.Color(0, 0, 255);
    colors[YELLOW] = pixels.Color(255, 255, 0);
}

NBackTask::~NBackTask()
{
    // Free dynamic memory
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

    // Initialize button with internal pull-up
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Initialize serial communication
    Serial.begin(9600);
    Serial.println(F("N-Back Task"));
    Serial.println(F("Commands:"));
    Serial.println(F("- 'debug' to enter debug mode and test hardware"));
    Serial.println(F("- 'start' to begin task"));
    Serial.println(F("- 'pause' to pause/resume task"));
    Serial.println(F("- 'level X' to set N-back level"));

    // Allocate memory for color sequence
    colorSequence = new int[maxTrials];

    // Generate random sequence
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
        runDebugMode();
        break;

    case STATE_PAUSED:
        // Do nothing when paused
        break;

    case STATE_RUNNING:
        // Check if we need to end visual feedback
        handleVisualFeedback(false);

        // If not in feedback, manage trials
        if (!flags.feedbackActive)
        {
            manageTrials();
        }

        // Always check for button press
        handleButtonPress();
        break;

    case STATE_IDLE:
        // Nothing to do in idle state
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
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "start")
        {
            if (state == STATE_DEBUG)
            {
                Serial.println(F("Exiting debug mode"));
                pixels.clear();
                pixels.show();
            }
            startTask();
        }
        else if (command == "pause")
        {
            if (state == STATE_RUNNING || state == STATE_PAUSED)
            {
                pauseTask(state != STATE_PAUSED);
            }
        }
        else if (command.startsWith("level "))
        {
            int newLevel = command.substring(6).toInt();
            if (newLevel > 0)
            {
                nBackLevel = newLevel;
                Serial.print(F("N-back level set to: "));
                Serial.println(nBackLevel);
                generateSequence();
            }
        }
        else if (command == "debug")
        {
            enterDebugMode();
        }
    }
}

//==============================================================================
// State Management
//==============================================================================

void NBackTask::startTask()
{
    // Reset performance metrics
    resetMetrics();

    // Reset trial state
    currentTrial = 0;
    flags.awaitingResponse = false;
    flags.targetTrial = false;
    flags.feedbackActive = false;

    // Generate a new sequence
    generateSequence();

    // Start the task
    state = STATE_RUNNING;

    Serial.println(F("Task started"));
    Serial.print(F("N-back level: "));
    Serial.println(nBackLevel);

    // Start first trial
    startNextTrial();
}

void NBackTask::generateSequence()
{
    // Generate random sequence with controlled number of targets
    randomSeed(analogRead(A0));

    int targetCount = maxTrials / 4; // Aim for 25% targets

    // First fill with random colors
    for (int i = 0; i < maxTrials; i++)
    {
        colorSequence[i] = random(COLOR_COUNT);
    }

    // Then ensure we have target trials after position n
    int targetsPlaced = 0;
    while (targetsPlaced < targetCount)
    {
        int pos = random(nBackLevel, maxTrials);
        // Make this position match the n-back position
        colorSequence[pos] = colorSequence[pos - nBackLevel];
        targetsPlaced++;
    }

    // Debug: print sequence
    Serial.println(F("Sequence generated:"));
    for (int i = 0; i < maxTrials; i++)
    {
        Serial.print(colorSequence[i]);
        if (i >= nBackLevel && colorSequence[i] == colorSequence[i - nBackLevel])
        {
            Serial.print(F("*")); // Mark targets
        }
        Serial.print(F(" "));
    }
    Serial.println();
}

void NBackTask::pauseTask(bool pause)
{
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
    Serial.println(F("Press the button to test it. Send 'start' to exit debug mode."));

    // Show first color
    setNeoPixelColor(debugColorIndex);
}

void NBackTask::endTask()
{
    state = STATE_IDLE;
    pixels.clear();
    pixels.show();

    reportResults();

    Serial.println(F("Send 'start' to begin a new session or 'debug' to test hardware"));
}

//==============================================================================
// Trial Management
//==============================================================================

void NBackTask::manageTrials()
{
    // Only manage trials when awaiting response
    if (!flags.awaitingResponse)
    {
        return;
    }

    unsigned long currentTime = millis();

    // If stimulus duration has passed
    if (currentTime - trialStartTime > timing.stimulusDuration)
    {
        // Turn off the pixel
        pixels.clear();
        pixels.show();
        flags.awaitingResponse = false;

        // Evaluate the trial outcome at the end
        evaluateTrialOutcome();

        // Wait for the inter-stimulus interval
        delay(timing.interStimulusInterval);

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
    // Four possible outcomes:
    // 1. Target trial + button pressed = Correct response
    // 2. Target trial + no button press = Missed target (false negative)
    // 3. Non-target trial + button pressed = False alarm (false positive)
    // 4. Non-target trial + no button press = Correct rejection

    if (flags.targetTrial && currentTrial >= nBackLevel)
    {
        // Target trial
        if (flags.buttonPressed)
        {
            // Correct response
            metrics.correctResponses++;

            // Add reaction time to totals (only for correct responses)
            metrics.totalReactionTime += trialData.reactionTime;
            metrics.reactionTimeCount++;

            Serial.println(F("CORRECT RESPONSE!"));
            Serial.print(F("Reaction time: "));
            Serial.print(trialData.reactionTime);
            Serial.println(F(" ms"));
        }
        else
        {
            // Missed target (false negative)
            metrics.missedTargets++;
            Serial.println(F("MISSED TARGET!"));
        }
    }
    else if (flags.buttonPressed)
    {
        // False alarm (false positive)
        metrics.falseAlarms++;
        Serial.println(F("FALSE ALARM!"));
        Serial.print(F("Reaction time: "));
        Serial.print(trialData.reactionTime);
        Serial.println(F(" ms (not counted in average)"));
    }
    else
    {
        // Correct rejection (no need to count these, but could add if needed)
        Serial.println(F("CORRECT REJECTION"));
    }
}

void NBackTask::startNextTrial()
{
    // Display the current color
    setNeoPixelColor(colorSequence[currentTrial]);

    // Record start time
    trialStartTime = millis();
    flags.awaitingResponse = true;
    flags.buttonPressed = false; // Reset button press tracking for new trial

    // Check if this is a target trial (n-back match)
    flags.targetTrial = (currentTrial >= nBackLevel) &&
                        (colorSequence[currentTrial] == colorSequence[currentTrial - nBackLevel]);

    // Debug info
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
    // Only process button in running state and not during feedback
    if (state != STATE_RUNNING || flags.feedbackActive)
    {
        return;
    }

    // Read button state
    int reading = digitalRead(BUTTON_PIN);

    // If sufficient time has passed since the last bounce
    if ((millis() - button.lastDebounceTime) > button.debounceDelay)
    {
        // Button press detected (LOW due to pull-up resistor)
        if (reading == LOW && button.lastState == HIGH && flags.awaitingResponse)
        {
            // Only record reaction time for the first button press
            if (!flags.buttonPressed)
            {
                // Calculate and store reaction time for this trial
                trialData.reactionTime = millis() - trialStartTime;

                // Mark that the button was pressed for this trial
                flags.buttonPressed = true;

                Serial.println(F("Button pressed"));

                // Always provide visual feedback for button presses
                handleVisualFeedback(true);
            }
        }
    }

    // Debounce logic
    if (reading != button.lastState)
    {
        button.lastDebounceTime = millis();
    }

    // Update button state
    button.lastState = reading;
}

//==============================================================================
// Visual Feedback
//==============================================================================

void NBackTask::handleVisualFeedback(boolean startFeedback)
{
    if (startFeedback)
    {
        // Start/restart visual feedback with white flash
        pixels.setPixelColor(0, pixels.Color(255, 255, 255));
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

        if (state == STATE_DEBUG)
        {
            // In debug mode, return to the current color
            setNeoPixelColor(debugColorIndex);
        }
        else if (state == STATE_RUNNING)
        {
            // In task mode, return to the current color if still in stimulus duration
            // or turn off the pixel if the stimulus duration has passed
            if (millis() - trialStartTime < timing.stimulusDuration)
            {
                setNeoPixelColor(colorSequence[currentTrial]);
            }
            else
            {
                pixels.clear();
                pixels.show();
            }
        }
    }
}

void NBackTask::setNeoPixelColor(int colorIndex)
{
    if (colorIndex >= 0 && colorIndex < COLOR_COUNT)
    {
        pixels.setPixelColor(0, colors[colorIndex]);
        pixels.show();
    }
    else if (colorIndex == WHITE)
    {
        pixels.setPixelColor(0, pixels.Color(255, 255, 255));
        pixels.show();
    }
}

//==============================================================================
// Debug Mode
//==============================================================================

void NBackTask::runDebugMode()
{
    // Handle visual feedback if active
    handleVisualFeedback(false);

    // If feedback is active, don't do other debug operations
    if (flags.feedbackActive)
    {
        return;
    }

    unsigned long currentTime = millis();

    // Cycle through colors automatically
    if (currentTime - lastColorChangeTime > timing.debugColorDuration)
    {
        debugColorIndex = (debugColorIndex + 1) % COLOR_COUNT;
        setNeoPixelColor(debugColorIndex);

        Serial.print(F("Debug: Showing color "));
        Serial.print(debugColorIndex);

        // Translate color index to name
        switch (debugColorIndex)
        {
        case RED:
            Serial.println(F(" (RED)"));
            break;
        case GREEN:
            Serial.println(F(" (GREEN)"));
            break;
        case BLUE:
            Serial.println(F(" (BLUE)"));
            break;
        case YELLOW:
            Serial.println(F(" (YELLOW)"));
            break;
        default:
            Serial.println();
            break;
        }

        lastColorChangeTime = currentTime;
    }

    // Read button state
    int reading = digitalRead(BUTTON_PIN);

    // If sufficient time has passed since the last bounce
    if ((millis() - button.lastDebounceTime) > button.debounceDelay)
    {
        // Button press detected (LOW due to pull-up resistor)
        if (reading == LOW && button.lastState == HIGH)
        {
            Serial.println(F("Debug: BUTTON PRESSED!"));

            // Start visual feedback (non-blocking)
            handleVisualFeedback(true);
        }
    }

    // Debounce logic
    if (reading != button.lastState)
    {
        button.lastDebounceTime = millis();
    }

    // Update button state
    button.lastState = reading;
}

//==============================================================================
// Utility Functions
//==============================================================================

void NBackTask::resetMetrics()
{
    metrics.correctResponses = 0;
    metrics.falseAlarms = 0;
    metrics.missedTargets = 0;
    metrics.totalReactionTime = 0;
    metrics.reactionTimeCount = 0;
}

void NBackTask::reportResults()
{
    // Calculate performance metrics
    int totalTargets = metrics.correctResponses + metrics.missedTargets;
    float hitRate = (totalTargets > 0) ? (float)metrics.correctResponses / totalTargets * 100.0 : 0;

    // Calculate average reaction time only from correct responses
    float averageRT = (metrics.reactionTimeCount > 0) ? (float)metrics.totalReactionTime / metrics.reactionTimeCount : 0;

    // Report results
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
    Serial.print(F("Average Reaction Time (correct responses only): "));
    Serial.print(averageRT);
    Serial.println(F(" ms"));
    Serial.println(F("======================"));
}