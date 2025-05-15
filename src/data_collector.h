#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include <Arduino.h>

//==============================================================================
// Configuration
//==============================================================================

// Maximum number of trials to store data for
#define MAX_DATA_ROWS 100

//==============================================================================
// Data Structures
//==============================================================================

// Data structure for N-Back trial data
struct NBackTrialData
{
    // Trial identification
    uint8_t stimulus_number; // Sequence position (1-based)
    uint8_t stimulus_color;  // Color shown (0-3)
    bool is_target;          // Whether this is a target trial

    // Response data
    bool response_made;     // Whether user responded
    bool is_correct;        // Whether response was correct
    uint16_t reaction_time; // Milliseconds between stimulus and response (0 if none)

    // Timing information
    uint32_t stimulus_onset_time; // When stimulus appeared (relative to session start)
    uint32_t response_time;       // When response occurred (0 if none)
    uint32_t stimulus_end_time;   // When stimulus disappeared
};

//==============================================================================
// DataCollector Class
//==============================================================================

class DataCollector
{
public:
    //----------------------------------------------------------------------------
    // Core Interface
    //----------------------------------------------------------------------------

    // Constructor
    DataCollector();

    // Initialize the data collector for a new session
    void begin(const String &study_id, uint16_t session_number);

    // Reset all stored data
    void reset();

    // Record a completed trial
    void recordCompletedTrial(
        uint8_t stimulus_number,
        uint8_t stimulus_color,
        bool is_target,
        bool response_made,
        bool is_correct,
        uint32_t stimulus_onset_time,
        uint32_t response_time,
        uint16_t reaction_time,
        uint32_t stimulus_end_time);

    // Send all collected data over serial
    void sendDataOverSerial();

    // Send real-time event data with write> prefix for immediate file writing
    void sendRealTimeEvent(const String &event_type,
                           uint8_t stimulus_number = 0,
                           uint8_t stimulus_color = 0,
                           bool is_target = false,
                           bool response_made = false,
                           bool is_correct = false,
                           uint32_t stimulus_onset_time = 0,
                           uint32_t response_time = 0,
                           uint16_t reaction_time = 0,
                           uint32_t stimulus_end_time = 0);

    // Send a simple timestamped event with write> prefix
    void sendTimestampedEvent(const String &event_type, const String &additional_data = "");

    //----------------------------------------------------------------------------
    // Accessors
    //----------------------------------------------------------------------------

    // Get the number of trials recorded
    uint8_t getTrialCount() const;

    // Get session start time (millis() value when begin was called)
    uint32_t getSessionStartTime() const;

    // Get absolute millis() when session started
    uint32_t getSessionAbsoluteStartTime() const;

    // Get session number
    uint16_t getSessionNumber() const { return session_number; }

    //----------------------------------------------------------------------------
    // Utility Functions
    //----------------------------------------------------------------------------

    // Convert milliseconds to HH:MM:SS:mmm format
    void formatTimestamp(uint32_t milliseconds, char *buffer, size_t bufferSize);

    // Print color name based on index
    void printColorName(uint8_t color_index);

    // Print boolean value as "true" or "false"
    void printBool(bool value);

private:
    //----------------------------------------------------------------------------
    // Private Data Members
    //----------------------------------------------------------------------------

    // Configuration data
    String study_id;                  // Study identifier
    uint16_t session_number;          // Session number
    uint32_t session_start_time;      // millis() value when session started
    uint32_t session_absolute_millis; // Absolute millis() when session started

    // Data storage
    NBackTrialData trials[MAX_DATA_ROWS];
    uint8_t trial_count;
};

#endif // DATA_COLLECTOR_H