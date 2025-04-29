# N-Back Task Project: Complete Context and Requirements

## Project Overview

This project implements a cognitive psychology experimental paradigm called the "N-Back task" on an Arduino Uno R3. The system presents a sequence of colored stimuli to participants who must identify when the current color matches the color shown "n" positions earlier in the sequence. The hardware consists of an Arduino connected to a NeoPixel for visual stimuli and a button for participant responses. Data is collected and transmitted to a master PC for analysis.

## Key Research Context

-   **Task Purpose**: The N-Back task is a working memory assessment used in cognitive psychology research.
-   **Measurement Goals**: Measure error rate, response time, and working memory performance.
-   **Study Framework**: Part of a larger research study where data will be collected from multiple participants.

## Hardware Configuration

-   **Microcontroller**: Arduino Uno R3 (2KB SRAM, 32KB flash memory)
-   **Display**: One NeoPixel RGB LED connected to pin 6
-   **Input**: One button connected to pin 2 (with internal pull-up resistor)
-   **Communication**: USB serial connection to master PC (9600 baud)

## Core Task Functionality

1. **Stimulus Presentation**:

    - System shows a sequence of colored stimuli (red, green, blue, yellow)
    - Each stimulus appears for a configurable duration (default 2000ms)
    - Inter-stimulus interval is configurable (default 2000ms)
    - Sequence is pseudo-random with controlled percentage of targets (~25%)

2. **Participant Response**:

    - Participant presses button when current color matches the color shown "n" positions earlier
    - System records reaction time and response accuracy
    - System provides brief visual feedback on button press (white flash)

3. **N-Back Levels**:

    - Configurable "n" value (e.g., 1-back, 2-back, 3-back)
    - Higher levels increase working memory load

4. **Task Configuration**:
    - All parameters configurable via serial commands
    - Settings include stimulus duration, interval time, n-back level, number of trials, study ID, and session number
    - Maximum 50 trials per session due to memory constraints

## Data Collection Requirements

1. **Trial Data Collected**:

    - Study ID and session number
    - Trial timestamp
    - Stimulus number, color, and target status
    - Participant response (made/not made)
    - Response correctness (correct/incorrect)
    - Stimulus onset, response, and end times
    - Reaction time

2. **Output Format**:

    - CSV format transmitted over serial
    - Custom protocol with format headers and data section markers
    - Timestamps formatted as HH:MM:SS:mmm
    - Both per-trial data and session summary data

3. **Data Transmission Protocol**:
    ```
    Opening Data Socket
    Format=[column headers]
    $$$
    [data rows in CSV format]
    $$$
    Format=[session summary headers]
    $$$
    [session summary row]
    $$$
    Closing Data Socket
    ```

## System States and Controls

1. **Operation Modes**:

    - IDLE: Waiting for commands
    - RUNNING: Actively presenting stimuli
    - PAUSED: Temporarily suspended
    - DEBUG: Hardware testing mode
    - DATA_READY: Task complete, data available

2. **Serial Commands**:

    - `config [params]`: Configure task parameters
    - `start`: Begin task execution
    - `pause`: Pause or resume task
    - `debug`: Enter hardware test mode
    - `get_data`: Retrieve collected data

3. **Non-Blocking Design**:
    - Uses state-based approach without blocking delays
    - Maintains responsiveness during inter-stimulus intervals
    - Handles button debouncing properly

## Performance Metrics

1. **Calculated Metrics**:

    - Correct responses (hits)
    - False alarms (false positives)
    - Missed targets (false negatives)
    - Hit rate percentage
    - Average reaction time (for correct responses)
    - Total session duration

2. **Trial Outcome Classification**:
    - Target + response = Correct response (hit)
    - Target + no response = Missed target (false negative)
    - Non-target + response = False alarm (false positive)
    - Non-target + no response = Correct rejection

## Memory Optimization

-   Efficient data structures with bit fields to save RAM
-   Careful management of string constants using F() macro
-   Limited to 50 trials maximum due to SRAM constraints
-   Currently uses approximately 52.8% of available RAM (1082/2048 bytes)
-   Uses approximately 46.9% of available flash memory (15112/32256 bytes)

## Master PC Integration

-   PC software will interface with Arduino via serial
-   PC will send configuration commands and trigger task execution
-   PC will receive and parse data for storage and analysis
-   Protocol documentation provided for implementation

## Future Extensions

-   Master PC software development
-   Potential integration with other types of cognitive tasks (mentioned "steam-engine" task)
-   Possible real-time data visualization on PC side

## Code Organization

The implementation is split across several files:

-   main.cpp: Entry point with setup() and loop()
-   nback_task.h: Class and structure definitions
-   nback_task.cpp: Main task implementation
-   data_collector.h: Data collection definitions
-   data_collector.cpp: Data collection and formatting implementation

## Design Priorities

1. **Reliability**: Ensuring consistent timing and accurate data collection
2. **Memory Efficiency**: Working within Arduino's limited resources
3. **Configurability**: Supporting various experimental protocols
4. **Code Structure**: Well-organized, maintainable implementation
5. **Usability**: Clear interface for both participants and researchers

## Input Mode Usage

The device supports a special input mode where button/touch press events are immediately forwarded over serial to the host computer. This is useful for integrating the device as a general input controller for another application.

### How to Use Input Mode

1. Send the command `input_mode` to enter the special input forwarding mode
2. The device will send button press events in the format: `button-press:CONFIRM` or `button-press:WRONG`
3. To exit input mode, send the command `exit`
4. The device will respond with `INPUT_MODE_EXIT` followed by `ready`

### Integration with Master Program

To use this in the Master Program:

-   Configure a serial port for the N-Back device
-   Send the `input_mode` command to enable input forwarding
-   Listen for messages that match the pattern `button-press:*`
-   Parse these messages to extract the button type (CONFIRM/WRONG)
-   When done, send `exit` to return to normal mode
-   Look for the `INPUT_MODE_EXIT` message to confirm exiting

This functionality allows using the N-Back device as a simple dual-button input controller for other applications without requiring task configuration.
