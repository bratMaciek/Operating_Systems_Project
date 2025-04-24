#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <signal.h>

#define NUM_PHILOSOPHERS 5
#define MAX_WAIT_TIME 6
#define SHARED_MEMORY_SIZE 5  // Number of shared memory cells for chopsticks

// Forward declarations
void* philosopher_routine(void* arg);
void* print_status(void* arg);
void* execute_task(void* arg);

// Global variables
atomic_int should_print = 0;
atomic_int running = 1;

// Philosopher structure
typedef struct {
    atomic_int state;         // 1:thinking, 2:waiting, 3:eating
    int philosopher_id;       // ID number
    atomic_int invoke_count;  // Times eaten
    atomic_int must_think;    // Fairness control
    time_t wait_start;       // Wait timestamp
} Philosopher;

Philosopher philosophers[NUM_PHILOSOPHERS];
pthread_mutex_t print_mutex;
pthread_mutex_t state_mutex;
atomic_int chopsticks[SHARED_MEMORY_SIZE]; // Shared memory for chopsticks

// Signal handler
void handle_signal(int sig) {
    if (sig == SIGINT) {
        atomic_store(&running, 0);
    }
}
// Utility functions
int get_random(int min, int max) {
    int result = 0, low_num = 0, hi_num = 0;
    if (min < max) {
        low_num = min;
        hi_num = max + 1;
    } else {
        low_num = max + 1;
        hi_num = min;
    }
    result = (rand() % (hi_num - low_num)) + low_num;
    return result;
}

int is_anyone_eating() {
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        if (atomic_load(&philosophers[i].state) == 3) {
            return 1;
        }
    }
    return 0;
}

// Philosopher actions
void eat(Philosopher* philosopher) {
    int left_chopstick_index = (philosopher->philosopher_id - 1 + NUM_PHILOSOPHERS) % NUM_PHILOSOPHERS; // Left chopstick
    int right_chopstick_index = philosopher->philosopher_id; // Right chopstick

    pthread_mutex_lock(&print_mutex);
    printf("Philosopher %d is eating.\n", philosopher->philosopher_id);
    pthread_mutex_unlock(&print_mutex);

    sleep(get_random(1, 4));  // 1-4 seconds

    atomic_fetch_add(&philosopher->invoke_count, 1);

    atomic_store(&philosopher->state, 1);

    // Release the chopsticks
    atomic_store(&chopsticks[left_chopstick_index], 0);
    atomic_store(&chopsticks[right_chopstick_index], 0);
}

void think(Philosopher* philosopher) {
    pthread_mutex_lock(&print_mutex);
    printf("Philosopher %d is thinking.\n", philosopher->philosopher_id);
    pthread_mutex_unlock(&print_mutex);

    sleep(get_random(2, 5));  // 2-5 seconds
}

void try_to_wait(Philosopher* philosopher) {
    int right_chopstick_index = philosopher->philosopher_id; // Right chopstick
    int expected = 0;

    // Try to get right chopstick and never release it
    if (atomic_compare_exchange_weak(&chopsticks[right_chopstick_index], &expected, philosopher->philosopher_id + 1)) {
        atomic_store(&philosopher->state, 2);
        // Once we get the right chopstick, we keep it and wait for the left one
    }
}

void wait(Philosopher* philosopher) {
    int left_chopstick_index = (philosopher->philosopher_id - 1 + NUM_PHILOSOPHERS) % NUM_PHILOSOPHERS;
    
    // Keep trying to get left chopstick without ever releasing the right one
    while (atomic_load(&running) && atomic_load(&philosophers[philosopher->philosopher_id].state) == 2) {
        int expected_left = 0;
        
        // Try to get left chopstick
        if (atomic_load(&chopsticks[left_chopstick_index]) == 0) {
            if (atomic_compare_exchange_weak(&chopsticks[left_chopstick_index], &expected_left, philosopher->philosopher_id + 1)) {
                atomic_store(&philosopher->state, 3);
                return;
            }
        }
        
        // Critical change: Don't release right chopstick even if we can't get the left one
        usleep(50000);
    }
}

void* execute_task(void* arg) {
    Philosopher* philosopher = (Philosopher*)arg;
    int current_state = atomic_load(&philosopher->state);

    if (atomic_load(&philosopher->must_think) && current_state != 2) {
        think(philosopher);
        return NULL;
    }

    if (current_state == 1) {
        think(philosopher);
        try_to_wait(philosopher);
    } else if (current_state == 2) {
        wait(philosopher);
    } else if (current_state == 3) {
        eat(philosopher);
    }

    return NULL;
}

void* philosopher_routine(void* arg) {
    Philosopher* philosopher = (Philosopher*)arg;
    while (atomic_load(&running)) {
        execute_task(philosopher);
        usleep(50000); // 50ms delay
    }
    return NULL;
}

void* print_status(void* arg) {
    while (atomic_load(&running)) {
        sleep(1);
        pthread_mutex_lock(&print_mutex);
        printf(" ");
        for (int i = 0; i < NUM_PHILOSOPHERS * 9; i++) printf("═");
        printf("\n");
        printf("║");
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            printf(" P%d      ", i);
        }
        printf("║\n║");

        // Chopstick representation
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            int left_chopstick_index = (i - 1 + NUM_PHILOSOPHERS) % NUM_PHILOSOPHERS;
            int right_chopstick_index = i;
            int left_taken = atomic_load(&chopsticks[left_chopstick_index]);
            int right_taken = atomic_load(&chopsticks[right_chopstick_index]);

            if (atomic_load(&philosophers[i].state) == 3) {
                printf(" ||      "); // Eating, so has both chopsticks
            } else if (left_taken == (i + 1) && right_taken == (i + 1)) {
                printf(" ||      "); // Eating, so has both chopsticks
            } else if (left_taken == (i + 1)) {
                printf(" |_      "); // Has only left chopstick
            } else if (right_taken == (i + 1)) {
                printf(" _|      "); // Has only right chopstick
            } else {
                printf(" __      "); // No chopsticks
            }
        }
        printf("║\n║");

        // State representation
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            char state_char;
            switch (atomic_load(&philosophers[i].state)) {
                case 1: state_char = 't'; break; // Thinking
                case 2: state_char = 'w'; break; // Waiting
                case 3: state_char = 'e'; break; // Eating
                default: state_char = '?'; break;
            }
            printf(" %c       ", state_char);
        }
        printf("║\n║");

        // Invoke count representation
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            printf(" %-7d ", atomic_load(&philosophers[i].invoke_count));
        }
        printf("║\n ");

        for (int i = 0; i < NUM_PHILOSOPHERS * 9; i++) printf("═");
        printf("\n");
        pthread_mutex_unlock(&print_mutex);
        sleep(1);
    }
    return NULL;
}

int main() {
    // Set up signal handling
    signal(SIGINT, handle_signal);

    pthread_mutex_init(&print_mutex, NULL);

    srand((unsigned int)time(NULL));

    // Initialize philosophers
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        atomic_init(&philosophers[i].state, 1); // Initial state: thinking
        philosophers[i].philosopher_id = i;
        atomic_init(&philosophers[i].invoke_count, 0);
    }

    // Initialize chopsticks (shared memory)
    for (int i = 0; i < SHARED_MEMORY_SIZE; i++) {
        atomic_init(&chopsticks[i], 0); // 0 means available, otherwise philosopher ID + 1
    }

    pthread_t philosopher_threads[NUM_PHILOSOPHERS];
    pthread_t status_thread;

    // Create threads
    pthread_create(&status_thread, NULL, print_status, NULL);

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_create(&philosopher_threads[i], NULL, philosopher_routine, &philosophers[i]);
    }

    // Wait for threads to finish
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_join(philosopher_threads[i], NULL);
    }
    pthread_join(status_thread, NULL);

    // Cleanup
    pthread_mutex_destroy(&print_mutex);

    printf("\nProgram terminated successfully\n");
    printf("\nFinal Status:\n");
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        printf("Philosopher %d - State: %d, Invoke count: %d\n",
               i,
               atomic_load(&philosophers[i].state),
               atomic_load(&philosophers[i].invoke_count));
    }
    return 0;
}
