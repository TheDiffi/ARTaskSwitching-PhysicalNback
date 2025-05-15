# N-Back Task Serial Interface Documentation

This document describes how to interface with the Arduino N-Back Task program via the serial connection.

## Connection Setup

-   **Baud Rate**: 9600
-   **Line Ending**: Newline (`\n`)

## Command Reference

The Arduino accepts the following commands over serial:

### 1. Configuration

```
config <stimDuration>,<interStimulusInterval>,<nBackLevel>,<trialsNumber>,<studyId>,<sessionNumber>[,%<color1>,<color2>,...%]
```

Sets up the task with the specified parameters:

-   **stimDuration**: Duration in milliseconds that each stimulus is shown (e.g., 1500)
-   **interStimulusInterval**: Time in milliseconds between stimuli (e.g., 1000)
-   **nBackLevel**: The N value for the N-Back task (1 = 1-back, 2 = 2-back, etc.)
-   **trialsNumber**: Number of trials per session (maximum 100)
-   **studyId**: Identifier for the study (alphanumeric, max 9 chars)
-   **sessionNumber**: Session number (integer)
-   **color sequence** (optional): Custom sequence of colors enclosed in % symbols (e.g., %red,blue,green,yellow%)

Example:

```
config 1500,1000,2,30,STUDY01,1
```

With custom sequence:

```
config 1500,1000,2,5,STUDY01,1,%red,green,blue,yellow,red%
```

Response:

```
Configuration updated:
Stimulus Duration: 1500ms
Inter-Stimulus Interval: 1000ms
N-back Level: 2
Number of Trials: 30
Study ID: STUDY01
Session Number: 1
Configuration applied successfully
```

### 2. Start Task

```
start
```

Starts the task with the current configuration. The Arduino will display stimuli and collect user responses.

Initial response:

```
Task started
N-back level: 2
Study ID: STUDY01
Trial 1: Color 3
...
```

During execution, the system will output progress information for each trial. When the task is complete:

```
=== TASK COMPLETE ===
N-Back Level: 2
Total Trials: 30
Total Targets: 9
Correct Responses: 4
False Alarms: 3
Missed Targets: 5
Hit Rate: 44.44%
Average Reaction Time (correct responses only): 1052.50 ms
Session Duration: 00:00:34:786
======================
task-completed
```

The marker "task-completed" signals that the task has finished and the system is ready for the next command.

### 3. Pause/Resume Task

```
pause
```

Toggles between pausing and resuming the current task.

Response when pausing:

```
Task paused
```

Response when resuming:

```
Task resumed
```

### 4. Debug Mode

```
debug
```

Enters hardware test mode, cycling through colors and detecting button presses.

Response:

```
*** DEBUG MODE ***
Testing NeoPixel and button. NeoPixel will cycle through colors.
Press the button to test it.
Send 'exit-debug' to return to IDLE state or 'start' to begin task.
Debug: Showing color 0 (RED)
...
```

If using capacitive touch input, the system will periodically display touch sensor values.

### 5. Exit Debug Mode

```
exit-debug
```

Exits debug mode and returns to the IDLE state without starting a task.

Response:

```
exiting debug mode
ready
```

### 6. Cancel Current Task

```
exit
```

Cancels the current task (if running or paused) and discards any collected data.

Response:

```
exiting
ready
```

### 7. Retrieve Data

```
get_data
```

Retrieves collected data after a task is completed.

Response:

```
Sending data for X recorded trials...
Opening Data Socket
Format=study_id,session_number,timestamp,task_type,event_type,stimulus_number,stimulus_color,is_target,response_made,is_correct,stimulus_onset_time,response_time,reaction_time,stimulus_end_time
$$$
STUDY01,1,00:00:02:054,n-back,trial_complete,1,green,false,false,true,00:00:00:053,00:00:00:000,0,00:00:02:054
...additional rows...
$$$
Format=study_id,session_number,start_time_millis,start_time,completion_time,total_duration,total_trials
$$$
STUDY01,1,3452167,00:57:32:167,01:02:15:872,00:04:43:705,30
$$$
Closing Data Socket
data-completed
```

The marker "data-completed" indicates the end of data transmission.

### 8. Input Mode

```
input_mode
```

Enters a special mode where button/touch press events are immediately forwarded over serial to the host computer. This is useful for integrating the device as a generic input controller.

Initial response:

```
Nback Entering INPUT MODE
Button/touch events will be forwarded over serial
Format: button-press:<type>
Where <type> is 'CONFIRM' or 'WRONG'
Send 'exit' to return to IDLE state
```

When a button is pressed, it sends events like:

```
button-press:CONFIRM
```

or

```
button-press:WRONG
```

To exit input mode, send the `exit` command:

```
exit
```

Response:

```
INPUT_MODE_EXIT
ready
```

The special marker "INPUT_MODE_EXIT" is sent to help your master program detect when input mode has ended.

### 9. Time Synchronization

```
sync
```

Requests the current Arduino time in milliseconds. This command is useful for synchronizing the timing between the host computer and the Arduino device.

Response:

```
sync 1234567
```

Where the number is the current Arduino time in milliseconds (from `millis()`). This allows the host computer to calculate time offsets and correctly interpret timestamps in the response data.

## Data Format

### Trial Data

Each trial produces one row of CSV data with the following fields:

1. **study_id**: Study identifier
2. **session_number**: Session number
3. **timestamp**: Formatted time (HH:MM:SS:mmm) of trial completion
4. **task_type**: Always "n-back"
5. **event_type**: Always "trial_complete"
6. **stimulus_number**: Position in sequence (1-based)
7. **stimulus_color**: Color shown ("red", "green", "blue", "yellow", "purple")
8. **is_target**: Whether this was a target trial ("true"/"false")
9. **response_made**: Whether user responded ("true"/"false")
10. **is_correct**: Whether response was correct ("true"/"false")
11. **stimulus_onset_time**: When stimulus appeared (HH:MM:SS:mmm)
12. **response_time**: When response occurred (HH:MM:SS:mmm or "00:00:00:000" if none)
13. **reaction_time**: Milliseconds between stimulus and response (0 if none)
14. **stimulus_end_time**: When stimulus disappeared (HH:MM:SS:mmm)

### Session Summary Data

After the trial data, a session summary is provided with:

1. **study_id**: Study identifier
2. **session_number**: Session number
3. **start_time_millis**: Raw milliseconds when session started
4. **start_time**: Formatted time when session started (HH:MM:SS:mmm)
5. **completion_time**: Formatted time when session ended (HH:MM:SS:mmm)
6. **total_duration**: Total session duration (HH:MM:SS:mmm)
7. **total_trials**: Number of trials completed

## Real-Time Data Reporting

In addition to the end-of-session data collection, the N-Back task now supports real-time event reporting. These events are sent immediately as they occur with the prefix `write>` to indicate to the master program that this data should be saved to a file. This allows for live monitoring and recording of task events as they happen.

### Real-Time Events

The following events are reported in real-time:

1. **Trial Completion Events**

```
write>STUDY01,1,2054,n-back,trial_complete,1,green,false,false,true,53,0,0,2054
```

2. **Input Forwarding Events**

```
write>STUDY01,1,1234,n-back,input_forwarded,0,none,false,false,false,0,0,0,0,CONFIRM
```

3. **Pause/Resume Events**

```
write>STUDY01,1,3456,n-back,pause,0,none,false,false,false,0,0,0,0
write>STUDY01,1,5678,n-back,resume,0,none,false,false,false,0,0,0,0
```

4. **Start Events**

```
write>STUDY01,1,0,n-back,start,0,none,false,false,false,0,0,0,0,n-back_level:2,stim_duration:1500,inter_stim_interval:1000,trials:30
```

### Real-Time Data Format

Real-time events follow the same CSV format as the end-of-session data, with the `write>` prefix added to indicate that this data should be saved immediately. For event types that don't have all the trial-specific information, default values (0 or "none") are used for the empty fields.

## Task Flow

A typical session follows this sequence:

1. Configure the task with the `config` command
2. Start the task with the `start` command
    - Real-time events will now be reported with the `write>` prefix
3. Wait for the "task-completed" marker
4. Retrieve data with the `get_data` command
5. Wait for the "data-completed" marker
6. Process the data on the PC side

## Error Handling

-   If configuration fails, you'll receive: `Failed to apply configuration - invalid parameters`
-   If requesting data before task is complete: `No data available. Run task first.`
-   If configuration format is incorrect: `Invalid config format. Use: config stimDuration,interStimulusInterval,nBackLevel,trialsNumber,study_id,session_number[,%color1,color2,...%]`

## Implementation Notes

-   Timestamps are relative to session start (not absolute time)
-   All times are in milliseconds returned by millis()
-   The "sync" command can be used to synchronize timing between the host computer and Arduino device
    -   Send "sync" to get the current Arduino millis() value
    -   Compare with host computer time to calculate offset
    -   Use this offset to convert Arduino timestamps to host computer time if needed
-   Reaction times are reported in raw milliseconds for easier analysis
-   The maximum number of trials is limited to 100 due to memory constraints
-   Special marker words ("task-completed" and "data-completed") are used to signal completion of operations
-   Available colors: "red", "green", "blue", "yellow", "purple"
-   Input modes: Button (0) or Capacitive Touch (1)
-   The task has support for both physical buttons and capacitive touch sensors
-   The INPUT MODE provides direct forwarding of button press events, useful for custom applications
-   Real-time events with the `write>` prefix allow for immediate data collection while the task is running
