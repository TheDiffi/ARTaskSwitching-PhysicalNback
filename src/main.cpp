#include <Adafruit_NeoPixel.h>

// Define pins
#define NEOPIXEL_PIN 6
#define BUTTON_PIN 2

// Define NeoPixel parameters
#define NUM_PIXELS 1

// Initialize NeoPixel
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// N-Back Task Parameters
int nBackLevel = 2;              // Default to 2-back task (change as needed)
int stimulusDuration = 1500;     // Stimulus display time in ms
int interStimulusInterval = 500; // Time between stimuli in ms
int maxTrials = 30;              // Total number of trials in a session

// Colors (RGB values)
uint32_t colors[] = {
    pixels.Color(255, 0, 0),  // Red
    pixels.Color(0, 255, 0),  // Green
    pixels.Color(0, 0, 255),  // Blue
    pixels.Color(255, 255, 0) // Yellow
};
int numColors = 4;

// Task variables
int *colorSequence;               // Array to store color sequence
int currentTrial = 0;             // Current trial number
unsigned long trialStartTime;     // When current trial started
unsigned long reactionTime;       // Time taken to react
boolean awaitingResponse = false; // If we're in response window
boolean targetTrial = false;      // If current trial is a target
boolean paused = false;           // Pause state
boolean taskRunning = false;      // Is task currently running

// Performance metrics
int correctResponses = 0;            // Number of correct button presses
int falseAlarms = 0;                 // Number of incorrect button presses
int missedTargets = 0;               // Number of missed targets
unsigned long totalReactionTime = 0; // Sum of all reaction times
int reactionTimeCount = 0;           // Count of measured reaction times

// Button debouncing
int lastButtonState = HIGH; // Default is HIGH due to pull-up
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup()
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
  Serial.println("Commands: 'start' to begin, 'pause' to pause/resume, 'level X' to set N-back level");

  // Allocate memory for color sequence
  colorSequence = new int[maxTrials];

  // Generate random sequence
  generateSequence();
}

void loop()
{
  // Process any serial commands
  processSerialCommands();

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

  // Handle button press
  handleButtonPress();
}

void processSerialCommands()
{
  if (Serial.available() > 0)
  {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "start")
    {
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
  }
}

void startTask()
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

void generateSequence()
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

void manageTrials()
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

void startNextTrial()
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

void handleButtonPress()
{
  // Read button state
  int reading = digitalRead(BUTTON_PIN);

  // Debounce
  if (reading != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    // If button is pressed (LOW due to pull-up) and task is running
    if (reading == LOW && lastButtonState == HIGH && taskRunning && awaitingResponse)
    {
      buttonPressed();
    }
  }

  lastButtonState = reading;
}

void buttonPressed()
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

void endTask()
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
  Serial.println("Send 'start' to begin a new session");
}