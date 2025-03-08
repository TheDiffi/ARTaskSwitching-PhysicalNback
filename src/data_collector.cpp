#include "data_collector.h"

DataCollector::DataCollector()
    : session_number(0),
      session_start_time(0),
      trial_count(0)
{
    // Initialize study_id with empty string
    study_id[0] = '\0';
}

void DataCollector::begin(const char *study_id, uint16_t session_number)
{
    // Store study information
    strncpy(this->study_id, study_id, sizeof(this->study_id) - 1);
    this->study_id[sizeof(this->study_id) - 1] = '\0'; // Ensure null termination

    this->session_number = session_number;
    this->session_start_time = millis();
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

    // Print header format
    Serial.print(F("Format=study_id,session_number,timestamp,task_type,event_type,"));
    Serial.println(F("stimulus_number,stimulus_color,is_target,response_made,is_correct,stimulus_onset_time,response_time,reaction_time,stimulus_end_time"));

    // Start data section
    Serial.println(F("$$$"));

    // Send each data row
    for (uint8_t i = 0; i < trial_count; i++)
    {
        const NBackTrialData &trial = trials[i];

        // Common fields
        Serial.print(study_id);
        Serial.print(F(","));
        Serial.print(session_number);
        Serial.print(F(","));
        Serial.print(trial.stimulus_end_time); // Use end time as the timestamp
        Serial.print(F(","));
        Serial.print(F("n-back"));
        Serial.print(F(","));
        Serial.print(F("trial_complete"));

        // N-Back specific fields
        Serial.print(F(","));
        Serial.print(trial.stimulus_number);
        Serial.print(F(","));

        // Color name
        switch (trial.stimulus_color)
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
        default:
            Serial.print(F("unknown"));
            break;
        }

        Serial.print(F(","));
        Serial.print(trial.is_target ? F("true") : F("false"));
        Serial.print(F(","));
        Serial.print(trial.response_made ? F("true") : F("false"));
        Serial.print(F(","));
        Serial.print(trial.is_correct ? F("true") : F("false"));
        Serial.print(F(","));
        Serial.print(trial.stimulus_onset_time);
        Serial.print(F(","));
        Serial.print(trial.response_time);
        Serial.print(F(","));
        Serial.print(trial.reaction_time);
        Serial.print(F(","));
        Serial.print(trial.stimulus_end_time);

        Serial.println();
    }

    // End data section
    Serial.println(F("$$$"));
    Serial.println(F("Closing Data Socket"));
}

uint8_t DataCollector::getTrialCount() const
{
    return trial_count;
}

uint32_t DataCollector::getSessionStartTime() const
{
    return session_start_time;
}

void DataCollector::printColorName(uint8_t color_index)
{
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
    default:
        Serial.print(F("unknown"));
        break;
    }
}

void DataCollector::printBool(bool value)
{
    Serial.print(value ? F("true") : F("false"));
}