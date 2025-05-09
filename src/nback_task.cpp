#include "nback_task.h"

//==============================================================================
// Constructor & Destructor
//==============================================================================

NBackTask::NBackTask()
    : pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
      nBackLevel(1),
      maxTrials(MAX_TRIALS),
      state(STATE_IDLE),
      currentTrial(0),
      trialStartTime(0),
      stimulusEndTime(0),
      feedbackStartTime(0),
      debugColorIndex(0),
      lastColorChangeTime(0),
      inputMode(INPUT_MODE),
      colorSequence(nullptr),
      study_id("DEFAULT")
{
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
    buttonCorrect.debounceDelay = 20;

    // Initialize button 2 debouncing variables
    buttonWrong.lastState = HIGH;
    buttonWrong.lastDebounceTime = 0;
    buttonWrong.debounceDelay = 20;

    // Initialize capacitive touch variables
    touchCorrect.value = 0;
    touchCorrect.threshold = TOUCH_THRESHOLD_CORRECT;
    touchCorrect.lastState = false;
    touchCorrect.lastDebounceTime = 0;

    touchWrong.value = 0;
    touchWrong.threshold = TOUCH_THRESHOLD_WRONG;
    touchWrong.lastState = false;
    touchWrong.lastDebounceTime = 0;

    // Initialize trial data recording
    trialData.reactionTime = 0;
    trialData.stimulusOnsetTime = 0;
    trialData.responseTime = 0;
    trialData.stimulusEndTime = 0;

    // Reset performance metrics
    resetMetrics();

    // Initialize the NeoPixel colors array
    colors[RED] = pixels.Color(255, 0, 0);       // Red
    colors[GREEN] = pixels.Color(0, 255, 0);     // Green
    colors[BLUE] = pixels.Color(0, 0, 255);      // Blue
    colors[YELLOW] = pixels.Color(255, 255, 0);  // Yellow
    colors[PURPLE] = pixels.Color(255, 0, 255);  // Purple
    colors[WHITE] = pixels.Color(255, 255, 255); // White
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
    // Initialize LED strip
    pixels.begin();
    pixels.setBrightness(255);

    // Initial power-on test
    setNeoPixelColor(PURPLE);
    delay(1000);
    pixels.clear();
    pixels.show();

    // Initialize input system based on current mode
    initializeInput();

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
    Serial.println(F("- 'input_mode 0|1' to set input mode (0=button, 1=touch)"));
    Serial.println(F("ready"));

    // Allocate memory for color sequence
    colorSequence = new int[maxTrials];

    // Generate initial random sequence
    generateSequence();
}

void NBackTask::loop()
{

    // Handle tasks based on current state
    switch (state)
    {
    case STATE_DEBUG:
        // In debug mode, cycle through colors and respond to button presses
        runDebugMode();
        break;

    case STATE_PAUSED:
        // When paused, do nothing until resumed
        renderPixels();
        break;

    case STATE_RUNNING:
        // Handle ongoing task
        renderPixels();

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

    case STATE_INPUT_MODE:
        // Handle input forwarding mode
        handleInputModeLoop();
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

bool NBackTask::processSerialCommands(const String &command)
{
    // Check for empty command
    if (command.length() == 0)
    {
        return false;
    }

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
        return true;
    }
    else if (command == "pause")
    {
        // Toggle pause state
        if (state == STATE_RUNNING || state == STATE_PAUSED)
        {
            pauseTask(state != STATE_PAUSED);
        }
        return true;
    }
    else if (command == "debug")
    {
        Serial.println(F("enter debug mode"));

        enterDebugMode();
        return true;
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
        return true;
    }
    else if (command == "exit")
    {
        // Cancel the current study or exit input mode
        if (state == STATE_RUNNING || state == STATE_PAUSED)
        {
            state = STATE_IDLE;
            pixels.clear();
            pixels.show();
            Serial.println(F("exiting"));
            Serial.println(F("ready"));
        }
        else if (state == STATE_DATA_READY)
        {
            state = STATE_IDLE;
            Serial.println(F("exiting"));
            Serial.println(F("ready"));
        }
        else if (state == STATE_INPUT_MODE)
        {
            exitInputMode();
        }
        return true;
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
        return true;
    }
    else if (command.startsWith("config "))
    {
        processConfigCommand(command);
        return true;
    }
    else if (command == "input_mode")
    {
        enterInputMode();
        return true;
    }
    else if (command == "sync")
    {
        // Send time sync message to master device
        sendTimeSyncToMaster();
        return true;
    }

    return false; // Command not recognized
}

void NBackTask::processConfigCommand(const String &command)
{
    // `config ${config.stimDuration},${config.interStimulusInterval},${config.nBackLevel},${config.trialsNumber},${config.studyId},${config.sessionNumber},%${sequenceStr}%`;

    // Parse configuration parameters
    String configStr = command.substring(7);

    // Variables to store parsed values
    uint16_t params[4] = {0}; // For numeric parameters: stimulus duration, ISI, n-back level, trials
    String studyId;           // For study ID
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
            studyId = configStr.substring(startPos, commaPos);
        }
        else if (paramIndex == 5)
        {
            // 6th parameter is the session number
            sessionNum = configStr.substring(startPos, commaPos).toInt();
        }
        startPos = commaPos + 1;
        paramIndex++;
    }

    // Check if a sequence was provided (format: %color1,color2,...%)
    bool hasCustomSequence = false;
    String sequenceStr = "";

    // Look for the sequence format %sequence% after the basic config parameters
    int seqStartPos = configStr.indexOf('%', startPos);
    if (seqStartPos != -1)
    {
        int seqEndPos = configStr.indexOf('%', seqStartPos + 1);
        if (seqEndPos != -1)
        {
            // Extract the sequence string without the % symbols
            sequenceStr = configStr.substring(seqStartPos + 1, seqEndPos);
            hasCustomSequence = true;
        }
    }

    // Apply configuration if all 6 parameters were found
    if (paramIndex == 6)
    {
        if (configure(params[0], params[1], params[2], params[3], studyId, sessionNum, false))
        {
            Serial.println(F("Configuration applied successfully"));

            // Apply custom sequence if provided
            if (hasCustomSequence && colorSequence != nullptr)
            {
                parseAndSetColorSequence(sequenceStr);
            }
        }
        else
        {
            Serial.println(F("Failed to apply configuration - invalid parameters"));
        }
    }
    else
    {
        Serial.println(F("Invalid config format. Use: config stimDuration,interStimulusInterval,nBackLevel,trialsNumber,study_id,session_number[,%color1,color2,...%]"));
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

void NBackTask::sendTimeSyncToMaster()
{
    // Send time sync message to master device
    unsigned long current = millis();
    Serial.println("sync " + String(current));
}

//==============================================================================
// State Management
//==============================================================================

void NBackTask::startTask()
{
    sendTimeSyncToMaster();

    // Reset performance metrics for the new task
    resetMetrics();

    // Reset trial state
    currentTrial = 0;
    flags.awaitingResponse = false;
    flags.targetTrial = false;
    flags.feedbackActive = false;
    flags.inInterStimulusInterval = false;

    // Reset data collector for a new session
    dataCollector.reset();

    // Send real-time start event with configuration data
    char configData[100];
    snprintf(configData, sizeof(configData),
             "n-back_level:%d,stim_duration:%d,inter_stim_interval:%d,trials:%d",
             nBackLevel, timing.stimulusDuration, timing.interStimulusInterval, maxTrials);
    dataCollector.sendTimestampedEvent("start", configData);

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

void NBackTask::pauseTask(bool pause)
{
    // Update state based on pause flag
    state = pause ? STATE_PAUSED : STATE_RUNNING;
    Serial.println(pause ? F("Task paused") : F("Task resumed"));

    // Send real-time event for pause/resume
    dataCollector.sendTimestampedEvent(pause ? "pause" : "resume");
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
                          uint8_t numTrials, const String &studyId, uint16_t sessionNum, bool genSequence)
{
    // Validate parameters (basic sanity checks)
    if (stimDuration < 100 || interStimulusInt < 100 || nBackLvl < 1 ||
        numTrials < 5 || numTrials > 50 || studyId.length() == 0)
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

    // Set study ID
    study_id = studyId;

    // Initialize data collector with study information and session number
    dataCollector.begin(study_id, sessionNum);

    // Generate new sequence with updated parameters
    if (genSequence)
    {
        // Generate a new random sequence of colors
        generateSequence();
    }

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

    // If in response window, check if a button was pressed or if time is up
    if (flags.awaitingResponse)
    {
        // End the trial if a button was pressed or time is up
        if (flags.buttonPressed /* || (currentTime - trialStartTime > timing.stimulusDuration) */)
        {
            // End the trial
            flags.awaitingResponse = false;

            // Record when the stimulus ended
            stimulusEndTime = currentTime;

            // Send `trial-complete` message to serial
            Serial.println(F("trial-complete"));

            // Evaluate the trial outcome at the end
            evaluateTrialOutcome();

            // Enter inter-stimulus interval state
            flags.inInterStimulusInterval = true;
        }
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
                Serial.println(trialData.reactionTime);
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
        flags.responseIsConfirm,                          // response_made - true = confirm, false = wrong
        isCorrect,                                        // is_correct
        trialData.stimulusOnsetTime,                      // stimulus_onset_time
        flags.buttonPressed ? trialData.responseTime : 0, // response_time
        flags.buttonPressed ? trialData.reactionTime : 0, // reaction_time
        trialData.stimulusEndTime                         // stimulus_end_time
    );

    // Send real-time data for trial completion
    dataCollector.sendRealTimeEvent(
        "trial_complete",
        currentTrial + 1,                                      // 1-based stimulus number
        colorSequence[currentTrial],                           // stimulus color
        flags.targetTrial,                                     // is_target
        flags.buttonPressed ? flags.responseIsConfirm : false, // response_made
        isCorrect,                                             // is_correct
        trialData.stimulusOnsetTime,                           // stimulus_onset_time
        flags.buttonPressed ? trialData.responseTime : 0,      // response_time
        flags.buttonPressed ? trialData.reactionTime : 0,      // reaction_time
        trialData.stimulusEndTime                              // stimulus_end_time
    );

    Serial.println(F("-----------"));
}

void NBackTask::startNextTrial()
{
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
    // Only process input in running state and not during feedback and if not pressed yet
    if (state != STATE_RUNNING || flags.feedbackActive || flags.buttonPressed)
    {
        return;
    }

    // Check for correct button press using abstraction
    if (isCorrectPressed() && flags.awaitingResponse)
    {
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

    // Check for wrong button press using abstraction
    if (isWrongPressed() && flags.awaitingResponse)
    {
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

//==============================================================================
// Visual Feedback
//==============================================================================

void NBackTask::renderPixels()
{
    if (flags.feedbackActive)
    {
        setNeoPixelColor(WHITE);
        return;
    }

    if (state == STATE_DEBUG)
    {
        setNeoPixelColor(debugColorIndex);
        return;
    }

    if (state == STATE_IDLE || state == STATE_PAUSED || state == STATE_DATA_READY)
    {
        pixels.clear();
        pixels.show();
        return;
    }

    if (flags.inInterStimulusInterval)
    {
        pixels.clear();
        pixels.show();
        return;
    }

    if (flags.awaitingResponse)
    {
        // Show the current color during response window
        setNeoPixelColor(colorSequence[currentTrial]);
    }
    else
    {
        // Show nothing during stimulus presentation
        pixels.clear();
        pixels.show();
    }
}

void NBackTask::handleVisualFeedback(boolean startFeedback)
{
    if (startFeedback)
    {
        // Start/restart visual feedback with white flash
        flags.feedbackActive = true;
        feedbackStartTime = millis();
        return;
    }

    // If not starting feedback, check if we need to end it
    if (flags.feedbackActive && (millis() - feedbackStartTime > timing.feedbackDuration))
    {
        // End the feedback flash
        flags.feedbackActive = false;
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
}

//==============================================================================
// Input Handling System
//==============================================================================

void NBackTask::initializeInput()
{
    if (inputMode == BUTTON_INPUT)
    {
        // Initialize buttons with internal pull-up resistor
        pinMode(BUTTON_CORRECT_PIN, INPUT_PULLUP);
        pinMode(BUTTON_WRONG_PIN, INPUT_PULLUP);
    }
    else
    {
        // For capacitive touch, no pin mode is needed as touchRead() handles it
        // Reset touch states
        touchCorrect.lastState = false;
        touchWrong.lastState = false;
    }
}

bool NBackTask::readCorrectInput()
{
    if (inputMode == BUTTON_INPUT)
    {
        // For physical button, LOW means pressed (due to INPUT_PULLUP)
        return digitalRead(BUTTON_CORRECT_PIN) == LOW;
    }
    else
    {
        // For capacitive touch, check against threshold
        touchCorrect.value = touchRead(TOUCH_CORRECT_PIN);
        return touchCorrect.value < touchCorrect.threshold;
    }
}

bool NBackTask::readWrongInput()
{
    if (inputMode == BUTTON_INPUT)
    {
        // For physical button, LOW means pressed (due to INPUT_PULLUP)
        return digitalRead(BUTTON_WRONG_PIN) == LOW;
    }
    else
    {
        // For capacitive touch, check against threshold
        touchWrong.value = touchRead(TOUCH_WRONG_PIN);
        return touchWrong.value < touchWrong.threshold;
    }
}

bool NBackTask::isCorrectPressed()
{
    // Get the current input state with debouncing
    unsigned long currentTime = millis();
    bool currentState = readCorrectInput();

    // If input mode is traditional button
    if (inputMode == BUTTON_INPUT)
    {
        // Only consider a button press if enough time has passed since the last state change
        if ((currentTime - buttonCorrect.lastDebounceTime) > buttonCorrect.debounceDelay)
        {
            // Check if state changed from not pressed to pressed
            if (currentState == true && buttonCorrect.lastState == false)
            {
                buttonCorrect.lastDebounceTime = currentTime;
                buttonCorrect.lastState = currentState;
                return true;
            }
        }
        if (currentState != buttonCorrect.lastState)
        {
            buttonCorrect.lastDebounceTime = currentTime;
            buttonCorrect.lastState = currentState;
        }
    }
    // If input mode is capacitive touch
    else
    {
        // Only consider a touch if enough time has passed since the last state change
        if ((currentTime - touchCorrect.lastDebounceTime) > buttonCorrect.debounceDelay)
        {
            // Check if state changed from not touched to touched
            if (currentState == true && touchCorrect.lastState == false)
            {
                touchCorrect.lastDebounceTime = currentTime;
                touchCorrect.lastState = currentState;
                return true;
            }
        }
        if (currentState != touchCorrect.lastState)
        {
            touchCorrect.lastDebounceTime = currentTime;
            touchCorrect.lastState = currentState;
        }
    }

    return false;
}

bool NBackTask::isWrongPressed()
{
    // Get the current input state with debouncing
    unsigned long currentTime = millis();
    bool currentState = readWrongInput();

    // If input mode is traditional button
    if (inputMode == BUTTON_INPUT)
    {
        // Only consider a button press if enough time has passed since the last state change
        if ((currentTime - buttonWrong.lastDebounceTime) > buttonWrong.debounceDelay)
        {
            // Check if state changed from not pressed to pressed
            if (currentState == true && buttonWrong.lastState == false)
            {
                buttonWrong.lastDebounceTime = currentTime;
                buttonWrong.lastState = currentState;
                return true;
            }
        }
        if (currentState != buttonWrong.lastState)
        {
            buttonWrong.lastDebounceTime = currentTime;
            buttonWrong.lastState = currentState;
        }
    }
    // If input mode is capacitive touch
    else
    {
        // Only consider a touch if enough time has passed since the last state change
        if ((currentTime - touchWrong.lastDebounceTime) > buttonWrong.debounceDelay)
        {
            // Check if state changed from not touched to touched
            if (currentState == true && touchWrong.lastState == false)
            {
                touchWrong.lastDebounceTime = currentTime;
                touchWrong.lastState = currentState;
                return true;
            }
        }
        if (currentState != touchWrong.lastState)
        {
            touchWrong.lastDebounceTime = currentTime;
            touchWrong.lastState = currentState;
        }
    }

    return false;
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
        colorSequence[i] = random(COLORS_USED);
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

//==============================================================================
// Debug Mode
//==============================================================================

void NBackTask::runDebugMode()
{
    handleVisualFeedback(false);
    renderPixels();

    unsigned long currentTime = millis();

    if (inputMode == CAPACITIVE_INPUT && currentTime % 1000 == 0)
    {
        int touchValue = touchRead(TOUCH_CORRECT_PIN);
        int touchValue2 = touchRead(TOUCH_WRONG_PIN);
        Serial.print(F("Touch value: "));
        Serial.print(touchValue);
        Serial.print(F(" Touch value 2: "));
        Serial.println(touchValue2);
    }

    // Cycle through colors automatically at set intervals
    if (currentTime - lastColorChangeTime > timing.debugColorDuration)
    {
        // Move to next color in cycle
        debugColorIndex = (debugColorIndex + 1) % COLOR_COUNT;

        // Report current color
        Serial.print(F("Debug: Showing color "));
        Serial.print(debugColorIndex);

        // Show color name for readability
        const char *colorNames[] = {"RED", "GREEN", "BLUE", "YELLOW", "PURPLE", "WHITE"};
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

    // Check correct button/touch input
    if (isCorrectPressed())
    {
        Serial.println(F("Debug: CONFIRM BUTTON PRESSED!"));
        // Provide visual feedback for button press
        handleVisualFeedback(true);
    }

    // Check wrong button/touch input
    if (isWrongPressed())
    {
        Serial.println(F("Debug: WRONG BUTTON PRESSED!"));
        // Provide visual feedback for button press
        handleVisualFeedback(true);
    }
}

//==============================================================================
// Color Parsing and Sequence Setting
//==============================================================================

int NBackTask::parseColorName(const String &colorName)
{
    // Convert color name to lowercase for case-insensitive comparison
    String lowerColor = colorName;
    lowerColor.toLowerCase();

    // Map color names to their enum values
    if (lowerColor == "red")
        return RED;
    if (lowerColor == "green")
        return GREEN;
    if (lowerColor == "blue")
        return BLUE;
    if (lowerColor == "yellow")
        return YELLOW;
    if (lowerColor == "purple")
        return PURPLE;
    if (lowerColor == "white")
        return WHITE;

    // Default to RED if color name is not recognized
    Serial.print(F("Warning: Unknown color name '"));
    Serial.print(colorName);
    Serial.println(F("', defaulting to RED"));
    return RED;
}

void NBackTask::parseAndSetColorSequence(const String &sequenceStr)
{
    int index = 0;
    int startPos = 0;
    int commaPos = -1;

    // Parse each color name separated by commas
    while (index < maxTrials && (commaPos = sequenceStr.indexOf(',', startPos)) != -1)
    {
        String colorName = sequenceStr.substring(startPos, commaPos);

        // Convert to enum value and store in sequence
        colorSequence[index] = parseColorName(colorName);

        // Move to next position
        startPos = commaPos + 1;
        index++;
    }

    // Handle the last color if there is one
    if (index < maxTrials && startPos < sequenceStr.length())
    {
        String colorName = sequenceStr.substring(startPos);
        colorSequence[index] = parseColorName(colorName);
        index++;
    }

    // If the sequence doesn't fill the entire trials, log a warning
    if (index < maxTrials)
    {
        Serial.print(F("!!!Warning: Provided sequence has only "));
        Serial.print(index);
        Serial.print(F(" colors, but "));
        Serial.print(maxTrials);
        Serial.println(F(" trials are configured.!!!"));
    }
    else
    {
        Serial.println(F("Custom color sequence applied successfully"));
    }
}

//==============================================================================
// Input Mode Forwarding Functions
//==============================================================================

void NBackTask::enterInputMode()
{
    // Enter input forwarding mode
    state = STATE_INPUT_MODE;

    // Turn off LEDs when entering input mode
    pixels.clear();
    pixels.show();

    // Reset button states for clean input forwarding
    buttonCorrect.lastState = false;
    buttonWrong.lastState = false;
    touchCorrect.lastState = false;
    touchWrong.lastState = false;

    Serial.println(F("Nback Entering INPUT MODE"));
    Serial.println(F("Send 'exit' to return to IDLE state"));
}

void NBackTask::exitInputMode()
{
    // Only perform exit actions if we're actually in input mode
    if (state != STATE_INPUT_MODE)
    {
        return;
    }

    // Return to idle state
    state = STATE_IDLE;

    // Turn off LEDs
    pixels.clear();
    pixels.show();

    // Reset button states
    buttonCorrect.lastState = false;
    buttonWrong.lastState = false;
    touchCorrect.lastState = false;
    touchWrong.lastState = false;

    // Send clear exit message
    Serial.println(F("INPUT_MODE_EXIT"));
    Serial.println(F("ready"));
}

void NBackTask::handleInputModeLoop()
{
    // Current state of inputs
    bool confirmCurrent = readCorrectInput();
    bool wrongCurrent = readWrongInput();

    // Only send PRESS events
    if (confirmCurrent && !buttonCorrect.lastState)
    {
        sendInputEvent("CONFIRM", true);
    }
    buttonCorrect.lastState = confirmCurrent;

    if (wrongCurrent && !buttonWrong.lastState)
    {
        sendInputEvent("WRONG", true);
    }
    buttonWrong.lastState = wrongCurrent;

    // Check for exit command
    if (Serial.available() > 0)
    {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toLowerCase();

        if (command == "exit")
        {
            exitInputMode();
        }
    }
}

void NBackTask::sendInputEvent(const String &inputType, bool isPressed)
{
    // Use the simplified "button-press:<type>" format for compatibility
    Serial.print(F("button-press:"));
    Serial.println(inputType);

    // Send real-time event data for input forwarding with write> prefix
    dataCollector.sendTimestampedEvent("input_forwarded", inputType);
}
