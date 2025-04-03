# Operating_Systems_Project

## Overview
This project implements a modified version of the classic Dining Philosophers problem with additional constraints and fairness mechanisms. The implementation uses C with POSIX threads and atomic operations to ensure thread safety.

## Key Features
- Modified dining philosophers algorithm with neighbor state constraints
- Fairness mechanism based on invoke counts
- Deadlock prevention through waiting timeout
- Thread-safe operations using mutexes and atomic variables

## Implementation Details

### State Management
Philosophers can be in one of three states:
1. Thinking (state = 1)
2. Waiting (state = 2)
3. Eating (state = 3)

### Eating Conditions
A philosopher can start eating when:
- Previous philosopher (ID - 1) is thinking
- Next philosopher (ID + 1) is either thinking or waiting

### Fairness Mechanisms
- Tracks invoke counts for each philosopher
- Forces philosophers to think when their invoke count exceeds lowest count + 1
- Prevents starvation by monitoring waiting time

### Deadlock Prevention
- Maximum waiting time limit (3 seconds)
- Automatic transition to thinking state if waiting timeout occurs
- Ensures system progress even under high contention

