#include "data_collector.h"

//==============================================================================
// Core Interface
//==============================================================================

DataCollector::DataCollector()
    : study_id(""),
      session_number(0),
      session_start_time(0),
      session_absolute_millis(0),
      trial_count(0)
{
}

void DataCollector::begin(const String &study_id, uint16_t session_number)
{
    // Store study information
    this->study_id = study_id;
    this->session_number = session_number;
    this->session_start_time = millis();
    this->session_absolute_millis = millis(); // Store absolute timestamp
    this->trial_count = 0;
}

void DataCollector::reset()
{
    // Clear all stored trials
    trial_count = 0;
}

void DataCollector::recordCompletedTrial(
    uint8_t stimulus_number,
    uint8_t stimulus_color,
    bool is_target,
    bool response_made,
    bool is_correct,
    uint32_t stimulus_onset_time,
    uint32_t response_time,
    uint16_t reaction_time,
    uint32_t stimulus_end_time)
{
    // Only record if we have space
    if (trial_count < MAX_DATA_ROWS)
    {
        NBackTrialData &trial = trials[trial_count];

        // Store all trial data
        trial.stimulus_number = stimulus_number;
        trial.stimulus_color = stimulus_color;
        trial.is_target = is_target;
        trial.response_made = response_made;
        trial.is_correct = is_correct;
        trial.stimulus_onset_time = stimulus_onset_time;
        trial.response_time = response_time;
        trial.reaction_time = reaction_time;
        trial.stimulus_end_time = stimulus_end_time;

        // Increment trial counter
        trial_count++;
    }
}

void DataCollector::sendDataOverSerial()
{
    // Nothing to send if no trials recorded
    if (trial_count == 0)
    {
        Serial.println(F("No data to send"));
        return;
    }

    // Following the specified protocol
    Serial.println(F("Opening Data Socket"));

    // Print header format for trial data
    Serial.print(F("Format=study_id,session_number,timestamp,task_type,event_type,"));
    Serial.println(F("stimulus_number,stimulus_color,is_target,response_made,is_correct,stimulus_onset_time,response_time,reaction_time,stimulus_end_time"));

    // Start data section
    Serial.println(F("$$$"));

    // Buffer for timestamp formatting
    char timestampBuffer[16]; // HH:MM:SS:mmm format needs 13 chars + null

    // Send each data row
    for (uint8_t i = 0; i < trial_count; i++)
    {
        const NBackTrialData &trial = trials[i];

        // Common fields
        Serial.print(study_id);
        Serial.print(F(","));
        Serial.print(session_number);
        Serial.print(F(","));
        Serial.print(trial.stimulus_end_time);
        Serial.print(F(","));
        Serial.print(F("n-back"));
        Serial.print(F(","));
        Serial.print(F("trial_complete"));

        // N-Back specific fields
        Serial.print(F(","));
        Serial.print(trial.stimulus_number);
        Serial.print(F(","));

        // Color name
        printColorName(trial.stimulus_color);

        Serial.print(F(","));
        Serial.print(trial.is_target ? F("true") : F("false"));
        Serial.print(F(","));
        Serial.print(trial.response_made ? F("true") : F("false"));
        Serial.print(F(","));
        Serial.print(trial.is_correct ? F("true") : F("false"));
        Serial.print(F(","));

        // Keep all time values as milliseconds for analysis
        Serial.print(trial.stimulus_onset_time);
        Serial.print(F(","));
        Serial.print(trial.response_time);
        Serial.print(F(","));
        Serial.print(trial.reaction_time); // Keep reaction time as raw milliseconds for analysis
        Serial.print(F(","));
        Serial.print(trial.stimulus_end_time);

        Serial.println();
        delay(10);
    }

    // End trial data section
    Serial.println(F("$$$"));

    // Now add session timing information section
    Serial.println(F("Format=study_id,session_number,start_time_millis,start_time,completion_time,total_duration,total_trials"));

    // Start session data section
    Serial.println(F("$$$"));

    // Calculate total completion time
    uint32_t currentTime = millis();
    uint32_t totalDuration = currentTime - session_start_time;

    // Format timestamps for summary
    char startTimeBuffer[16];
    char completionTimeBuffer[16];
    char durationBuffer[16];
    formatTimestamp(session_absolute_millis, startTimeBuffer, sizeof(startTimeBuffer));
    formatTimestamp(currentTime, completionTimeBuffer, sizeof(completionTimeBuffer));
    formatTimestamp(totalDuration, durationBuffer, sizeof(durationBuffer));

    // Output session summary line
    Serial.print(study_id);
    Serial.print(F(","));
    Serial.print(session_number);
    Serial.print(F(","));
    Serial.print(session_absolute_millis);
    Serial.print(F(","));
    Serial.print(startTimeBuffer);
    Serial.print(F(","));
    Serial.print(completionTimeBuffer);
    Serial.print(F(","));
    Serial.print(durationBuffer);
    Serial.print(F(","));
    Serial.println(trial_count);

    // End session data section
    Serial.println(F("$$$"));

    // Close data socket
    Serial.println(F("Closing Data Socket"));
}

void DataCollector::sendRealTimeEvent(const String &event_type,
                                      uint8_t stimulus_number,
                                      uint8_t stimulus_color,
                                      bool is_target,
                                      bool response_made,
                                      bool is_correct,
                                      uint32_t stimulus_onset_time,
                                      uint32_t response_time,
                                      uint16_t reaction_time,
                                      uint32_t stimulus_end_time)
{
    // Start with the write> prefix to indicate this should be saved to a file
    Serial.print(F("write>"));

    // Common fields
    Serial.print(study_id);
    Serial.print(F(","));
    Serial.print(session_number);
    Serial.print(F(","));
    Serial.print(millis() - session_start_time); // Current timestamp relative to session start
    Serial.print(F(","));
    Serial.print(F("n-back"));
    Serial.print(F(","));
    Serial.print(event_type);

    // N-Back specific fields
    Serial.print(F(","));
    Serial.print(stimulus_number);
    Serial.print(F(","));

    printColorName(stimulus_color);

    Serial.print(F(","));
    Serial.print(is_target ? F("true") : F("false"));
    Serial.print(F(","));
    Serial.print(response_made ? F("true") : F("false"));
    Serial.print(F(","));
    Serial.print(is_correct ? F("true") : F("false"));
    Serial.print(F(","));
    Serial.print(stimulus_onset_time);
    Serial.print(F(","));
    Serial.print(response_time);
    Serial.print(F(","));
    Serial.print(reaction_time);
    Serial.print(F(","));
    Serial.print(stimulus_end_time);

    Serial.println();
}

String DataCollector::printColorName(uint8_t color_index)
{
    // Color name
    switch (color_index)
    {
    case 0:
        Serial.print(F("red"));
        break;
    case 1:
        Serial.print(F("green"));
        break;
    case 2:
        Serial.print(F("blue"));
        break;
    case 3:
        Serial.print(F("yellow"));
        break;
    case 4:
        Serial.print(F("purple"));
        break;
    default:
        Serial.print(F("unknown"));
        break;
    }
}

void DataCollector::sendTimestampedEvent(const String &event_type, const String &additional_data)
{
    // Start with the write> prefix to indicate this should be saved to a file
    Serial.print(F("write>"));

    // Common fields
    Serial.print(study_id);
    Serial.print(F(","));
    Serial.print(session_number);
    Serial.print(F(","));
    Serial.print(millis() - session_start_time); // Current timestamp relative to session start
    Serial.print(F(","));
    Serial.print(F("n-back"));
    Serial.print(F(","));
    Serial.print(event_type);

    // For simple events, fill remaining columns with defaults except for the additional data
    Serial.print(F(",0,none,false,false,false,0,0,0,0"));

    // If additional data provided, add it as a comment at the end
    if (additional_data.length() > 0)
    {
        Serial.print(F(","));
        Serial.print(additional_data);
    }

    Serial.println();
}

//==============================================================================
// Accessors
//==============================================================================

uint8_t DataCollector::getTrialCount() const
{
    return trial_count;
}

uint32_t DataCollector::getSessionStartTime() const
{
    return session_start_time;
}

uint32_t DataCollector::getSessionAbsoluteStartTime() const
{
    return session_absolute_millis;
}

//==============================================================================
// Utility Functions
//==============================================================================

void DataCollector::formatTimestamp(uint32_t milliseconds, char *buffer, size_t bufferSize)
{
    // Convert milliseconds to hours:minutes:seconds:milliseconds format
    uint32_t totalSeconds = milliseconds / 1000;
    uint16_t ms = milliseconds % 1000;
    uint8_t seconds = totalSeconds % 60;
    uint8_t minutes = (totalSeconds / 60) % 60;
    uint8_t hours = (totalSeconds / 3600);

    // Format as HH:MM:SS:mmm
    snprintf(buffer, bufferSize, "%02d:%02d:%02d:%03d", hours, minutes, seconds, ms);
}

void DataCollector::printBool(bool value)
{
    Serial.print(value ? F("true") : F("false"));
}