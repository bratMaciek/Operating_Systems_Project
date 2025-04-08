# Operating_Systems_Project

## Overview
This project implements a modified version of the classic Dining Philosophers problem with additional constraints and fairness mechanisms. The implementation uses C with POSIX threads and atomic operations to ensure thread safety.

## Key Features
- Modified dining philosophers algorithm with state-based synchronization
- Fairness mechanism using invoke counts and priority system
- Deadlock prevention through waiting timeout and state management
- Thread-safe operations using mutexes and atomic variables
- Random state transitions with controlled timing

## Implementation Details

### State Management
Philosophers can be in one of three states:
1. Thinking (state = 1)
2. Waiting (state = 2)
3. Eating (state = 3)

### Eating Conditions
A philosopher can start eating when:
- They have priority (lowest invoke count or waited long enough)
- Previous philosopher is not eating
- Next philosopher is not eating
- No one else is eating OR the philosopher is not required to think

### Fairness Mechanisms
- Tracks invoke counts for each philosopher
- Forces philosophers to think when their invoke count exceeds lowest count + 1
- Priority system based on:
  - Having the lowest invoke count
  - Waiting time exceeding half of MAX_WAIT_TIME

### Deadlock Prevention
- Maximum waiting time limit (6 seconds)
- Automatic transition to thinking state if waiting timeout occurs
- Limited number of waiting philosophers (max 2)
- Priority-based selection of new waiting philosophers

### Timing and Randomization
- Random thinking time: 1-5 seconds
- Random eating time: 1-4 seconds
- 5ms delay between main loop iterations
- Random selection of initial waiting philosopher


## Build and Run
To compile the program:
```bash
gcc -o dining_philosophers main.c -pthread
