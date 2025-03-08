#include "nback_task.h"

NBackTask::NBackTask()
    : pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
      nBackLevel(2),
      stimulusDuration(1500),
      interStimulusInterval(500),
      maxTrials(MAX_TRIALS),
      numColors(4),
      currentTrial(0),
      awaitingResponse(false),
      targetTrial(false),
      paused(false),
      taskRunning(false),
      correctResponses(0),
      falseAlarms(0),
      missedTargets(0),
      totalReactionTime(0),
      reactionTimeCount(0),
      lastButtonState(HIGH),
      lastDebounceTime(0),
      debounceDelay(50),
      debugMode(false),
      lastColorChangeTime(0),
      debugColorIndex(0)
{
    // Initialize the colors array
    colors[0] = pixels.Color(255, 0, 0);   // Red
    colors[1] = pixels.Color(0, 255, 0);   // Green
    colors[2] = pixels.Color(0, 0, 255);   // Blue
    colors[3] = pixels.Color(255, 255, 0); // Yellow
}

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
    Serial.println("N-Back Task");
    Serial.println("Commands:");
    Serial.println("- 'debug' to enter debug mode and test hardware");
    Serial.println("- 'start' to begin task");
    Serial.println("- 'pause' to pause/resume task");
    Serial.println("- 'level X' to set N-back level");

    // Allocate memory for color sequence
    colorSequence = new int[maxTrials];

    // Generate random sequence
    generateSequence();
}

void NBackTask::loop()
{
    // Process any serial commands
    processSerialCommands();

    // Handle debug mode
    if (debugMode)
    {
        runDebugMode();
        return;
    }

    // If task is paused, do nothing else
    if (paused)
    {
        return;
    }

    // If task is running, manage the trials
    if (taskRunning)
    {
        manageTrials();
    }

    // Always handle button press
    evaluateButtonState();
}

void NBackTask::processSerialCommands()
{
    if (Serial.available() > 0)
    {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "start")
        {
            if (debugMode)
            {
                debugMode = false;
                pixels.clear();
                pixels.show();
                Serial.println("Exiting debug mode");
            }
            startTask();
        }
        else if (command == "pause")
        {
            paused = !paused;
            Serial.println(paused ? "Task paused" : "Task resumed");
        }
        else if (command.startsWith("level "))
        {
            int newLevel = command.substring(6).toInt();
            if (newLevel > 0)
            {
                nBackLevel = newLevel;
                Serial.print("N-back level set to: ");
                Serial.println(nBackLevel);
                generateSequence(); // Regenerate sequence with new targets
            }
        }
        else if (command == "debug")
        {
            // Enter debug mode
            debugMode = true;
            taskRunning = false;
            paused = false;
            debugColorIndex = 0;
            lastColorChangeTime = millis();
            Serial.println("*** DEBUG MODE ***");
            Serial.println("Testing NeoPixel and button. NeoPixel will cycle through colors.");
            Serial.println("Press the button to test it. Send 'start' to exit debug mode.");
            // Show first color
            pixels.setPixelColor(0, colors[debugColorIndex]);
            pixels.show();
        }
    }
}

void NBackTask::startTask()
{
    // Reset all metrics
    currentTrial = 0;
    correctResponses = 0;
    falseAlarms = 0;
    missedTargets = 0;
    totalReactionTime = 0;
    reactionTimeCount = 0;

    // Generate a new sequence
    generateSequence();

    // Start the task
    taskRunning = true;
    paused = false;

    Serial.println("Task started");
    Serial.print("N-back level: ");
    Serial.println(nBackLevel);

    // Start first trial
    startNextTrial();
}

void NBackTask::generateSequence()
{
    // Generate random sequence with controlled number of targets
    randomSeed(analogRead(A0)); // Use unconnected analog pin for true randomness

    int targetCount = maxTrials / 4; // Aim for 25% targets

    // First fill with random colors
    for (int i = 0; i < maxTrials; i++)
    {
        colorSequence[i] = random(numColors);
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
    Serial.println("Sequence generated:");
    for (int i = 0; i < maxTrials; i++)
    {
        Serial.print(colorSequence[i]);
        if (i >= nBackLevel && colorSequence[i] == colorSequence[i - nBackLevel])
        {
            Serial.print("*"); // Mark targets
        }
        Serial.print(" ");
    }
    Serial.println();
}

void NBackTask::manageTrials()
{
    unsigned long currentTime = millis();

    // If we're awaiting a response and the stimulus duration has passed
    if (awaitingResponse && (currentTime - trialStartTime > stimulusDuration))
    {
        // Turn off the pixel
        pixels.clear();
        pixels.show();
        awaitingResponse = false;

        // If it was a target trial and user didn't respond, count as missed
        if (targetTrial && currentTrial > nBackLevel)
        {
            missedTargets++;
            Serial.println("MISSED TARGET!");
        }

        // Wait for the inter-stimulus interval
        delay(interStimulusInterval);

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

void NBackTask::startNextTrial()
{
    // Display the current color
    pixels.setPixelColor(0, colors[colorSequence[currentTrial]]);
    pixels.show();

    // Record start time
    trialStartTime = millis();
    awaitingResponse = true;

    // Check if this is a target trial (n-back match)
    targetTrial = (currentTrial >= nBackLevel) &&
                  (colorSequence[currentTrial] == colorSequence[currentTrial - nBackLevel]);

    // Debug info
    Serial.print("Trial ");
    Serial.print(currentTrial + 1);
    Serial.print(": Color ");
    Serial.print(colorSequence[currentTrial]);
    if (targetTrial)
    {
        Serial.println(" (TARGET)");
    }
    else
    {
        Serial.println();
    }
}

void NBackTask::evaluateButtonState()
{
    // Skip if in debug mode - debug mode handles its own button detection
    if (debugMode)
    {
        return;
    }

    // Read button state
    int reading = digitalRead(BUTTON_PIN);

    // If sufficient time has passed since the last bounce
    if ((millis() - lastDebounceTime) > debounceDelay)
    {
        // Button press detected (LOW due to pull-up resistor)
        if (reading == LOW && lastButtonState == HIGH)
        {
            // Only handle task-related button presses here
            if (taskRunning)
            {
                // Task is running and we're in the response window
                handleTaskButtonPress();
            }
        }
    }

    // Debounce logic
    if (reading != lastButtonState)
    {
        lastDebounceTime = millis();
    }

    // Update button state
    lastButtonState = reading;
}

void NBackTask::handleTaskButtonPress()
{
    // Calculate reaction time
    reactionTime = millis() - trialStartTime;

    // Check if this is a correct response (target trial)
    if (targetTrial && currentTrial >= nBackLevel)
    {
        correctResponses++;
        totalReactionTime += reactionTime;
        reactionTimeCount++;

        Serial.print("CORRECT! Reaction time: ");
        Serial.print(reactionTime);
        Serial.println(" ms");
    }
    else
    {
        // False alarm
        falseAlarms++;
        Serial.println("FALSE ALARM!");
    }

    // Turn off the pixel immediately after response
    pixels.clear();
    pixels.show();
    awaitingResponse = false;

    // Wait for the inter-stimulus interval
    delay(interStimulusInterval);

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

void NBackTask::runDebugMode()
{
    unsigned long currentTime = millis();

    // Cycle through colors automatically
    if (currentTime - lastColorChangeTime > debugColorDuration)
    {
        debugColorIndex = (debugColorIndex + 1) % numColors;
        pixels.setPixelColor(0, colors[debugColorIndex]);
        pixels.show();

        Serial.print("Debug: Showing color ");
        Serial.print(debugColorIndex);
        switch (debugColorIndex)
        {
        case 0:
            Serial.println(" (RED)");
            break;
        case 1:
            Serial.println(" (GREEN)");
            break;
        case 2:
            Serial.println(" (BLUE)");
            break;
        case 3:
            Serial.println(" (YELLOW)");
            break;
        default:
            Serial.println();
            break;
        }

        lastColorChangeTime = currentTime;
    }

    // Read button state directly in debug mode to ensure responsive detection
    int reading = digitalRead(BUTTON_PIN);

    // If sufficient time has passed since the last bounce
    if ((millis() - lastDebounceTime) > debounceDelay)
    {
        // Button press detected (LOW due to pull-up resistor)
        if (reading == LOW && lastButtonState == HIGH)
        {
            // Button pressed in debug mode
            Serial.println("Debug: BUTTON PRESSED!");

            // Visual feedback - briefly flash white
            pixels.setPixelColor(0, pixels.Color(255, 255, 255));
            pixels.show();
            delay(200);

            // Return to current color
            pixels.setPixelColor(0, colors[debugColorIndex]);
            pixels.show();
        }
    }

    // Debounce logic
    if (reading != lastButtonState)
    {
        lastDebounceTime = millis();
    }

    // Update button state
    lastButtonState = reading;
}

void NBackTask::endTask()
{
    taskRunning = false;
    pixels.clear();
    pixels.show();

    // Calculate performance metrics
    int totalTargets = correctResponses + missedTargets;
    float hitRate = (totalTargets > 0) ? (float)correctResponses / totalTargets * 100.0 : 0;
    float averageRT = (reactionTimeCount > 0) ? (float)totalReactionTime / reactionTimeCount : 0;

    // Report results
    Serial.println("\n=== TASK COMPLETE ===");
    Serial.print("N-Back Level: ");
    Serial.println(nBackLevel);
    Serial.print("Total Trials: ");
    Serial.println(maxTrials);
    Serial.print("Total Targets: ");
    Serial.println(totalTargets);
    Serial.print("Correct Responses: ");
    Serial.println(correctResponses);
    Serial.print("False Alarms: ");
    Serial.println(falseAlarms);
    Serial.print("Missed Targets: ");
    Serial.println(missedTargets);
    Serial.print("Hit Rate: ");
    Serial.print(hitRate);
    Serial.println("%");
    Serial.print("Average Reaction Time: ");
    Serial.print(averageRT);
    Serial.println(" ms");
    Serial.println("======================");
    Serial.println("Send 'start' to begin a new session or 'debug' to test hardware");
}