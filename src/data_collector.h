#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include <Arduino.h>

// Maximum number of trials to store data for
#define MAX_DATA_ROWS 50

// Data structure for N-Back trial data
struct NBackTrialData
{
    // Common fields
    uint8_t stimulus_number;      // Sequence position (1-based)
    uint8_t stimulus_color;       // Color shown (0-3)
    bool is_target;               // Whether this is a target trial
    bool response_made;           // Whether user responded
    bool is_correct;              // Whether response was correct
    uint32_t stimulus_onset_time; // When stimulus appeared (relative to session start)
    uint32_t response_time;       // When response occurred (0 if none)
    uint16_t reaction_time;       // Milliseconds between stimulus and response (0 if none)
    uint32_t stimulus_end_time;   // When stimulus disappeared
};

class DataCollector
{
public:
    // Constructor
    DataCollector();

    // Initialize the data collector for a new session
    void begin(const char *study_id, uint16_t session_number);

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

    // Get the number of trials recorded
    uint8_t getTrialCount() const;

    // Get session start time (millis() value when begin was called)
    uint32_t getSessionStartTime() const;

private:
    // Configuration data
    char study_id[10];           // Study identifier
    uint16_t session_number;     // Session number
    uint32_t session_start_time; // millis() value when session started

    // Data storage
    NBackTrialData trials[MAX_DATA_ROWS];
    uint8_t trial_count;

    // Helper methods
    void printColorName(uint8_t color_index);
    void printBool(bool value);
};

#endif // DATA_COLLECTOR_H