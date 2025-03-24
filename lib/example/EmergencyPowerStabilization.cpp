#include "EmergencyPowerStabilization.h"

//==============================================================================
// Constructor
//==============================================================================

EmergencyPowerStabilization::EmergencyPowerStabilization()
    : strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800)
{
    // Game state initialization
    gameState = IDLE;

    // Input mode initialization
    inputMode = CAPACITIVE_INPUT;

    // Cursor initialization
    cursorPosition = 0;
    cursorDirection = 1;
    lastCursorUpdateTime = 0;

    // Game configuration parameters
    redZoneRatio = 0.2;
    orangeZoneRatio = 0.2;
    greenZoneRatio = 0.1;

    // New cursor speed implementation
    // Time in milliseconds to traverse the entire strip once, changed by config
    baseTraversalTimeMs = 1000;
    traversalTimeMs = baseTraversalTimeMs;
    delayPerStep = traversalTimeMs / (NUM_LEDS * 2); // Time per LED step (x2 for back and forth)

    // Button state initialization
    lastButtonState = HIGH; // pullup, so HIGH is not pressed
    lastDebounceTime = 0;

    // Capacitive touch initialization
    touchThreshold = TOUCH_THRESHOLD;
    lastTouchState = false;
    lastTouchTime = 0;

    // Game progress initialization
    totalTrials = MAX_TRIALS;
    currentTrial = 0;
    successCount = 0;

    // Serial command handling initialization
    inputBuffer = "";
    commandReady = false;

    // Session configuration initialization
    studyId = "DEFAULT";
    sessionNumber = 1;

    // Debug mode initialization
    testPattern = 0;
    lastPatternChange = 0;

    // Initialize colors
    redColor = strip.Color(255, 0, 0);      // Red
    orangeColor = strip.Color(255, 165, 0); // Orange
    greenColor = strip.Color(0, 255, 0);    // Green
    blueColor = strip.Color(0, 0, 255);     // Blue
    purpleColor = strip.Color(128, 0, 128); // Purple
    blackColor = strip.Color(0, 0, 0);      // Black (off)

    // Initialize scores
    for (int i = 0; i < MAX_TRIALS; i++)
        lastScores[i] = -1; // -1 means not played yet
}

//==============================================================================
// Core Functionality
//==============================================================================

void EmergencyPowerStabilization::setup()
{
    // Initialize LED strip
    strip.begin();
    strip.setBrightness(50); // Set moderate brightness

    // Initial power-on test
    fillStrip(greenColor);
    delay(1000);
    strip.clear();
    strip.show();

    // Initialize input system
    initializeInput();

    // Display startup information
    Serial.println("Power Stabilizer Controller ready");
    Serial.println("\nEmergency Power Stabilization Task Simulator");
    Serial.println("Available commands:");
    Serial.println("  config <studyId>,<sessionNumber>,<traversalTime>,<trialCount> - Configure the task");
    Serial.println("  start - Start a new session");
    Serial.println("  interrupt - Trigger emergency power stabilization game");
    Serial.println("  debug - Enter debug mode (LED and button testing)");
    Serial.println("  get_data - Retrieve collected data");
    Serial.println("  input_mode <0|1> - Set input mode (0=button, 1=capacitive)");
    Serial.println("  exit - Cancel current task");
}

void EmergencyPowerStabilization::loop()
{
    // Check for serial input
    processCommand();

    // Handle game logic based on state
    switch (gameState)
    {
    case INTERRUPT_TRIGGERED:
        renderAlarm();
        checkInput();
        break;
    case IN_PROGRESS:
        updateCursor();
        renderLEDs();
        checkInput();
        break;

    case TEST_MODE:
        handleDebug();
        break;

    case IDLE:
    case STARTED:
    default:
        // Nothing to do in these states
        break;
    }
}

//==============================================================================
// Input Handling
//==============================================================================

void EmergencyPowerStabilization::initializeInput()
{
    // Initialize based on current input mode
    if (inputMode == BUTTON_INPUT)
    {
        pinMode(BUTTON_PIN, INPUT_PULLUP);
        Serial.println("Button input mode initialized");
    }
    else if (inputMode == CAPACITIVE_INPUT)
    {
        // Set proper threshold based on working example
        Serial.println("Capacitive touch input mode initialized");
        Serial.println("Touch threshold set to: " + String(touchThreshold));
    }
}

void EmergencyPowerStabilization::setInputMode(int mode)
{
    if (mode == 0)
    {
        inputMode = BUTTON_INPUT;
        Serial.println("Switching to button input mode");
    }
    else if (mode == 1)
    {
        inputMode = CAPACITIVE_INPUT;
        Serial.println("Switching to capacitive touch input mode");
    }
    else
    {
        Serial.println("Invalid input mode: " + String(mode) + ". Using button input.");
        inputMode = BUTTON_INPUT;
    }

    // Re-initialize with new input mode
    initializeInput();
}

void EmergencyPowerStabilization::checkInput()
{
    // Check if input is pressed and handle it
    if (isInputJustPressed())
    {
        handleInputPress(cursorPosition);
    }
}

bool EmergencyPowerStabilization::readCapacitiveInput()
{
    // Get raw touch reading
    int reading = touchRead(TOUCH_PIN);

    // Return true if reading is below threshold (logic is inverted from how it was)
    // This matches the working implementation in main.cpp
    return reading < touchThreshold;
}

bool EmergencyPowerStabilization::isInputJustPressed()
{
    bool currentState = false;
    unsigned long currentTime = millis();

    if (inputMode == BUTTON_INPUT)
    {
        // Read button state (active LOW due to pull-up)
        int reading = digitalRead(BUTTON_PIN);

        // Check for button press with debounce
        if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY)
        {
            // Button press detection (LOW means pressed)
            if (reading == LOW && lastButtonState == HIGH)
            {
                lastDebounceTime = currentTime;
                lastButtonState = reading;
                return true;
            }
        }

        // Update button state for next check
        if (reading != lastButtonState)
        {
            lastDebounceTime = currentTime;
        }
        lastButtonState = reading;
    }
    else if (inputMode == CAPACITIVE_INPUT)
    {
        // Read touch sensor and binarize the reading
        int reading = touchRead(TOUCH_PIN);

        // Convert to binary state - below threshold means touched (HIGH)
        if (reading < touchThreshold)
        {
            currentState = true;
        }
        else
        {
            currentState = false;
        }

        // Check for state change with debounce
        if ((currentTime - lastTouchTime) > DEBOUNCE_DELAY)
        {
            // If state has been stable for debounce period and is different from last state
            if (currentState != lastTouchState)
            {
                // Only register a button press when going from not touched to touched
                if (currentState && !lastTouchState)
                {
                    lastTouchTime = currentTime;
                    lastTouchState = currentState;
                    return true;
                }
                lastTouchState = currentState;
            }
        }

        // Update touch state for next check if changed
        if (currentState != lastTouchState)
        {
            lastTouchTime = currentTime;
        }
        lastTouchState = currentState;
    }

    return false;
}

void EmergencyPowerStabilization::handleInputPress(int position)
{
    if (gameState == INTERRUPT_TRIGGERED)
    {
        // Start the game
        startTrials();
        return;
    }

    if (gameState == IN_PROGRESS)
    {
        // Get zone (0-4 from left red to right red)
        int zone = getZone(position);
        int accuracy = getAccuracy(position);

        //  Store result
        lastScores[currentTrial] = position;

        // Record event in data collector
        dataCollector.recordTrialEvent(zone == 2, accuracy, traversalTimeMs, millis());

        // Display result based on zone hit
        displayZoneResult(zone);

        // Update success count if green zone hit
        if (zone == 2)
        {
            successCount++;
        }

        delay(500);
        strip.clear();
        strip.show();
        delay(500);

        // Move to next trial or end game
        currentTrial++;
        if (currentTrial >= totalTrials)
        {
            endInterrupt();
        }
        else
        {
            startNextTrial();
        }
    }
}

//==============================================================================
// Command Processing
//==============================================================================

void EmergencyPowerStabilization::processCommand()
{
    if (Serial.available() > 0)
    {
        // Read and trim the incoming command
        String command = Serial.readStringUntil('\n');
        command.trim();

        Serial.println("Processing command: " + command);

        if (command.startsWith("config"))
        {
            // Extract configuration parameters
            String params = command.substring(7);
            processConfigCommand(params);
        }
        else if (command.equalsIgnoreCase("start"))
        {
            startSession();
        }
        else if (command.equalsIgnoreCase("interrupt"))
        {
            // Check if session is active
            if (gameState != STARTED)
            {
                Serial.println("No active session. Use 'start' command first.");
                return;
            }

            Serial.println("Emergency Power Stabilization Needed!");
            Serial.println("Starting power stabilization sequence...");
            startInterrupt();
        }
        else if (command.startsWith("input_mode"))
        {
            // Extract mode parameter (0 for button, 1 for capacitive)
            int mode = command.substring(10).toInt();
            setInputMode(mode);
        }
        else if (command.equalsIgnoreCase("debug"))
        {
            gameState = TEST_MODE;
            Serial.println("*** DEBUG MODE ***");
            Serial.println("Testing LEDs and button functionality");
            Serial.println("Send 'exit-debug' to return to IDLE state");
        }
        else if (command.equalsIgnoreCase("exit-debug"))
        {
            // Only process exit-debug if in debug mode
            if (gameState == TEST_MODE)
            {
                Serial.println("exiting debug mode");
                gameState = IDLE;
                // Turn off all LEDs when exiting debug mode
                strip.clear();
                strip.show();
                Serial.println("ready");
            }
            else
            {
                Serial.println("Not in debug mode");
            }
        }
        else if (command == "get_data")
        {
            dataCollector.sendCollectedData();
        }
        else if (command.equalsIgnoreCase("exit"))
        {
            Serial.println("exiting");
            gameState = IDLE;
            dataCollector.endSession();
            Serial.println("ready");
        }
        else
        {
            Serial.println("Unknown command: " + command);
            Serial.println("Available commands: config, start, interrupt, debug, exit-debug, get_data, input_mode, exit");
        }
    }
}

void EmergencyPowerStabilization::processConfigCommand(String params)
{
    // Expected format: studyId,sessionNumber
    int firstComma = params.indexOf(',');
    int secondComma = params.indexOf(',', firstComma + 1);
    int thirdComma = params.indexOf(',', secondComma + 1);

    if (firstComma <= 0 || secondComma <= 0 || thirdComma <= 0)
    {
        Serial.println("Invalid config format. Use: config <studyId>,<sessionNumber>,<traversalTime>,<trialCount>");
        return;
    }

    String studyStr = params.substring(0, firstComma);
    String sessionStr = params.substring(firstComma + 1, secondComma);
    String traversalTimeStr = params.substring(secondComma + 1, thirdComma);
    String trialCountStr = params.substring(thirdComma + 1);

    // Parse values
    String study = studyStr;
    int session = sessionStr.toInt();
    int traversalTime = traversalTimeStr.toInt();
    int trialCount = trialCountStr.toInt();

    // Validate
    if (study.length() == 0 || session < 1 || traversalTime < 100 || trialCount < 1)
    {
        Serial.println("Failed to apply configuration - invalid parameters");
        return;
    }

    // Apply configuration
    studyId = study;
    sessionNumber = session;
    baseTraversalTimeMs = traversalTime;
    totalTrials = trialCount;

    Serial.println("Configuration updated:");
    Serial.println("Study ID: " + studyId);
    Serial.println("Session Number: " + String(sessionNumber));
    Serial.println("Traversal Time: " + String(baseTraversalTimeMs) + "ms");
    Serial.println("Trial Count: " + String(trialCount));
    Serial.println("Configuration applied successfully");
}

void EmergencyPowerStabilization::startSession()
{
    Serial.println("Task started");
    Serial.println("Power Stabilization task initialization");
    Serial.println("Study ID: " + studyId);
    Serial.println("Session Number: " + String(sessionNumber));

    // Start a new data collection session
    gameState = STARTED;
    dataCollector.startSession(studyId, sessionNumber);
}

void EmergencyPowerStabilization::endSession()
{
    // End the session
    dataCollector.endSession();
    Serial.println("task-completed");

    // Turn off all LEDs
    strip.clear();
    strip.show();
}

//==============================================================================
// Game Logic
//==============================================================================

void EmergencyPowerStabilization::startInterrupt()
{
    // Start the game
    gameState = INTERRUPT_TRIGGERED;
}

void EmergencyPowerStabilization::startTrials()
{
    // Reset game state
    gameState = IN_PROGRESS;
    currentTrial = 0;
    successCount = 0;
    for (int i = 0; i < MAX_TRIALS; i++)
        lastScores[i] = -1;

    // Initialize cursor
    cursorPosition = 0;
    cursorDirection = 1;

    // Reset the traversal time and delay per step
    randomizeSpeed(0);

    if (delayPerStep < 1)
    {
        Serial.println("Warning: Minimum delay per step is too low, Arduino might not keep up!");
    }
}

void EmergencyPowerStabilization::updateCursor()
{
    unsigned long currentTime = millis();

    // Use delayPerStep for cursor movement timing
    /*     Serial.println("Delay per step: " + String(delayPerStep));
     */
    if (currentTime - lastCursorUpdateTime >= delayPerStep)
    {
        // Update cursor position
        cursorPosition += cursorDirection;

        // Reverse direction at the edges
        if (cursorPosition >= NUM_LEDS - 1)
        {
            cursorPosition = NUM_LEDS - 1;
            cursorDirection = -1;
        }
        else if (cursorPosition <= 0)
        {
            cursorPosition = 0;
            cursorDirection = 1;
        }

        lastCursorUpdateTime = currentTime;
    }
}

void EmergencyPowerStabilization::displayZoneResult(int zone)
{
    String resultText;
    switch (zone)
    {
    case 0:
        resultText = "Critical failure! Left red zone hit.";
        break;
    case 1:
        resultText = "Poor stabilization. Left orange zone hit.";
        break;
    case 2:
        resultText = "Perfect stabilization! Green zone hit.";
        break;
    case 3:
        resultText = "Poor stabilization. Right orange zone hit.";
        break;
    case 4:
        resultText = "Critical failure! Right red zone hit.";
        break;
    }

    Serial.println(resultText);
}

void EmergencyPowerStabilization::startNextTrial()
{
    // Prepare next trial
    cursorPosition = 0;
    cursorDirection = 1;

    // Slightly change the speed randomly for the next trial
    randomizeSpeed(10);

    Serial.println("Trial " + String(currentTrial + 1) + " of " + String(totalTrials) + " - Stop the cursor in the green zone!");
}

void EmergencyPowerStabilization::endInterrupt()
{
    gameState = STARTED;
    printResults();

    // Victory animation - blink green
    blinkStrip(greenColor);

    // Turn off all LEDs
    strip.clear();
    strip.show();
}

void EmergencyPowerStabilization::printResults()
{
    Serial.println("\n===== Emergency Power Stabilization Results =====");
    Serial.println("Successful stabilizations: " + String(successCount) + " out of " + String(MAX_TRIALS));

    // Display individual trial results
    for (int i = 0; i < MAX_TRIALS; i++)
    {
        String zoneText;
        switch (getZone(lastScores[i]))
        {
        case 0:
            zoneText = "Left Red (Critical Failure)";
            break;
        case 1:
            zoneText = "Left Orange (Poor)";
            break;
        case 2:
            zoneText = "Green (Perfect)";
            break;
        case 3:
            zoneText = "Right Orange (Poor)";
            break;
        case 4:
            zoneText = "Right Red (Critical Failure)";
            break;
        default:
            zoneText = "Not Played";
            break;
        }
        Serial.println("Trial " + String(i + 1) + ": " + zoneText);
    }

    String finalAssessment;

    if (successCount >= 4)
    {
        finalAssessment = "Excellent! Power stabilization successful.";
    }
    else if (successCount >= 3)
    {
        finalAssessment = "Good. Power stabilization adequate.";
    }
    else if (successCount >= 2)
    {
        finalAssessment = "Mediocre. Additional training recommended.";
    }
    else
    {
        finalAssessment = "Poor. Significant training required.";
    }

    Serial.println("\nFinal Assessment: " + finalAssessment);
    Serial.println("==============================================");
    Serial.println("interrupt-over");
}

//==============================================================================
// LED Rendering
//==============================================================================

void EmergencyPowerStabilization::renderLEDs()
{
    // Clear all LEDs
    strip.clear();

    // Draw the zones
    for (int i = 0; i < NUM_LEDS; i++)
    {
        int zone = getZone(i);

        uint32_t color;
        switch (zone)
        {
        case 0: // Left red zone
            color = redColor;
            break;
        case 1: // Left orange zone
            color = orangeColor;
            break;
        case 2: // Green zone
            color = greenColor;
            break;
        case 3: // Right orange zone
            color = orangeColor;
            break;
        case 4: // Right red zone
            color = redColor;
            break;
        default:
            color = blackColor;
            break;
        }

        strip.setPixelColor(i, color);
    }

    // Draw the cursor (turn off the LED to make it visible)
    drawCursor(cursorPosition);

    // Update the LED strip
    strip.show();
}

void EmergencyPowerStabilization::drawCursor(int position)
{
    // Draw the cursor as 3 black LEDs
    strip.setPixelColor(position, blackColor);
    strip.setPixelColor(position + 1, blackColor);
    strip.setPixelColor(position - 1, blackColor);
}

void EmergencyPowerStabilization::renderAlarm()
{
    // Flash red LEDs to indicate emergency in a non-blocking way
    const int flashDelay = 500; // Flash every 500ms

    static unsigned long lastFlashTime = 0;
    static bool flashState = false;

    unsigned long currentTime = millis();

    // Flash every 500ms (adjust timing as needed)
    if (currentTime - lastFlashTime >= flashDelay)
    {
        lastFlashTime = currentTime;
        flashState = !flashState; // Toggle flash state

        if (flashState)
        {
            // Turn all LEDs red
            fillStrip(redColor);
        }
        else
        {
            // Turn all LEDs off
            strip.clear();
            strip.show();
        }
    }
}

void EmergencyPowerStabilization::fillStrip(uint32_t color)
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        strip.setPixelColor(i, color);
    }
    strip.show();
}

void EmergencyPowerStabilization::blinkStrip(uint32_t color)
{
    for (int i = 0; i < 5; i++)
    {
        fillStrip(color);
        delay(200);
        strip.clear();
        strip.show();
        delay(200);
    }
}

//==============================================================================
// Game Utility Functions
//==============================================================================

void EmergencyPowerStabilization::randomizeSpeed(int percentage)
{
    // Randomize the speed of the cursor
    int speedRange = 0;
    if (percentage > 0 && percentage < 100)
    {
        speedRange = baseTraversalTimeMs / percentage;
    }

    traversalTimeMs = baseTraversalTimeMs + random(-speedRange, speedRange);
    delayPerStep = traversalTimeMs / (NUM_LEDS * 2); // Time per LED step (x2 for back and forth)
}

int EmergencyPowerStabilization::getZone(int position)
{
    // Normalize the position to 0-100 range
    int normalizedPos = map(position, 0, NUM_LEDS - 1, 0, 100);

    // Calculate the zone boundaries on the 0-100 scale
    // First, calculate total parts to distribute the zones proportionally
    float totalParts = redZoneRatio + orangeZoneRatio + greenZoneRatio + orangeZoneRatio + redZoneRatio;

    // Calculate the width of each zone on the 0-100 scale
    int leftRedWidth = round((redZoneRatio / totalParts) * 100);
    int leftOrangeWidth = round((orangeZoneRatio / totalParts) * 100);
    int greenWidth = round((greenZoneRatio / totalParts) * 100);
    int rightOrangeWidth = round((orangeZoneRatio / totalParts) * 100);
    int rightRedWidth = 100 - leftRedWidth - leftOrangeWidth - greenWidth - rightOrangeWidth; // Ensure we total 100

    // Calculate the zone boundaries
    int leftOrangeBoundary = leftRedWidth;
    int greenBoundary = leftOrangeBoundary + leftOrangeWidth;
    int rightOrangeBoundary = greenBoundary + greenWidth;
    int rightRedBoundary = rightOrangeBoundary + rightOrangeWidth; // Should be 100

    // Determine which zone the normalized position falls into
    if (normalizedPos < leftOrangeBoundary)
    {
        return 0; // Left red zone
    }
    else if (normalizedPos < greenBoundary)
    {
        return 1; // Left orange zone
    }
    else if (normalizedPos < rightOrangeBoundary)
    {
        return 2; // Green zone
    }
    else if (normalizedPos < rightRedBoundary)
    {
        return 3; // Right orange zone
    }
    else
    {
        return 4; // Right red zone
    }
}

int EmergencyPowerStabilization::getAccuracy(int position)
{
    int optimalPosition = NUM_LEDS / 2;
    int accuracy = abs(position - optimalPosition);
    return accuracy;
}

//==============================================================================
// Debug Mode
//==============================================================================

void EmergencyPowerStabilization::handleDebug()
{
    static int currentPattern = 0;
    static unsigned long lastPatternChangeTime = 0;
    unsigned long currentTime = millis();
    bool inputState = isInputJustPressed();

    // Display input value
    if (inputMode == CAPACITIVE_INPUT && currentTime % 1000 == 0)
    {
        // Print touch value every second
        Serial.print("DEBUG: Touch sensor value: ");
        Serial.println(touchRead(TOUCH_PIN));
    }

    // Check for input press to indicate in debug mode
    if (inputState)
    {
        if (inputMode == BUTTON_INPUT)
        {
            Serial.println(F("DEBUG: Button PRESSED"));
        }
        else
        {
            Serial.println(F("DEBUG: Touch DETECTED"));
        }
    }

    // Pattern rotation every 5 seconds
    if (currentTime - lastPatternChangeTime >= 5000)
    {
        currentPattern = (currentPattern + 1) % 4;
        lastPatternChangeTime = currentTime;
        Serial.print(F("DEBUG: Pattern changed to "));
        Serial.println(currentPattern);
    }

    // Display the current pattern
    displayDebugPattern(currentPattern, inputState, currentTime);
}

void EmergencyPowerStabilization::displayDebugPattern(int pattern, bool buttonState, unsigned long currentTime)
{
    strip.clear();

    switch (pattern)
    {
    case 0: // Zone visualization
        // Show all color zones
        for (int i = 0; i < NUM_LEDS; i++)
        {
            switch (getZone(i))
            {
            case 0:
                strip.setPixelColor(i, redColor);
                break;
            case 1:
                strip.setPixelColor(i, orangeColor);
                break;
            case 2:
                strip.setPixelColor(i, greenColor);
                break;
            case 3:
                strip.setPixelColor(i, orangeColor);
                break;
            case 4:
                strip.setPixelColor(i, redColor);
                break;
            }
        }
        break;

    case 1: // Moving cursor
        strip.setPixelColor((currentTime / 200) % NUM_LEDS, blueColor);
        break;

    case 2: // Button reactive
        fillStrip(buttonState == LOW ? purpleColor : blueColor);
        break;

    case 3: // Rainbow pattern
        for (int i = 0; i < NUM_LEDS; i++)
        {
            int hue = ((i * 256 / NUM_LEDS) + (currentTime / 50)) % 256;
            strip.setPixelColor(i, wheelColor(hue));
        }
        break;
    }

    if (buttonState)
    {
        strip.setPixelColor(0, redColor);
    }

    strip.show();
}

// Color wheel helper function - converts hue (0-255) to RGB color
uint32_t EmergencyPowerStabilization::wheelColor(byte wheelPos)
{
    wheelPos = 255 - wheelPos;
    if (wheelPos < 85)
    {
        return strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
    }
    else if (wheelPos < 170)
    {
        wheelPos -= 85;
        return strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
    }
    else
    {
        wheelPos -= 170;
        return strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
    }
}
