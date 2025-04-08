#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <signal.h>

#define NUM_PHILOSOPHERS 5
#define MAX_WAIT_TIME 6

// Forward declarations
void* philosopher_routine(void* arg);
void* print_status(void* arg);
void* execute_task(void* arg);

// Global variables
atomic_int should_print = 0;
atomic_int running = 1;  // Added for graceful shutdown

typedef struct {
    atomic_int state;  // 1: thinking, 2: waiting, 3: eating
    int philosopher_id;
    atomic_int invoke_count;
    atomic_int must_think;
    time_t wait_start;
} Philosopher;

Philosopher philosophers[NUM_PHILOSOPHERS];
pthread_mutex_t print_mutex;
pthread_mutex_t state_mutex;

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

int get_lowest_count() {
    int lowest = atomic_load(&philosophers[0].invoke_count);
    for (int i = 1; i < NUM_PHILOSOPHERS; i++) {
        int current = atomic_load(&philosophers[i].invoke_count);
        if (current < lowest) {
            lowest = current;
        }
    }
    return lowest;
}

// Philosopher actions
void eat(Philosopher* philosopher) {
    pthread_mutex_lock(&print_mutex);
    printf("Philosopher %d is eating.\n", philosopher->philosopher_id);
    pthread_mutex_unlock(&print_mutex);
    
    sleep(get_random(1, 4));  // 1-4 seconds
    
    atomic_fetch_add(&philosopher->invoke_count, 1);
    
    int lowest = get_lowest_count();
    if (atomic_load(&philosopher->invoke_count) > (lowest + 1)) {
        atomic_store(&philosopher->must_think, 1);
    }
    
    atomic_store(&philosopher->state, 1);
}

void think(Philosopher* philosopher) {
    pthread_mutex_lock(&print_mutex);
    printf("Philosopher %d is thinking.\n", philosopher->philosopher_id);
    pthread_mutex_unlock(&print_mutex);
    
    sleep(get_random(1, 5));  // 1-5 seconds
    
    if (atomic_load(&philosopher->must_think)) {
        int lowest = get_lowest_count();
        if (atomic_load(&philosopher->invoke_count) <= (lowest + 1)) {
            atomic_store(&philosopher->must_think, 0);
        }
    }
}

void wait(Philosopher* philosopher) {
    time_t current_time = time(NULL);
    if (current_time - philosopher->wait_start >= MAX_WAIT_TIME) {
        pthread_mutex_lock(&print_mutex);
        printf("Philosopher %d waited too long, going back to thinking.\n", 
               philosopher->philosopher_id);
        pthread_mutex_unlock(&print_mutex);
        
        atomic_store(&philosopher->state, 1);
        return;
    }
    
    pthread_mutex_lock(&print_mutex);
    printf("Philosopher %d is waiting.\n", philosopher->philosopher_id);
    pthread_mutex_unlock(&print_mutex);
    
    int prev_id = (philosopher->philosopher_id - 1 + NUM_PHILOSOPHERS) % NUM_PHILOSOPHERS;
    int next_id = (philosopher->philosopher_id + 1) % NUM_PHILOSOPHERS;
    
    pthread_mutex_lock(&state_mutex);
    
    int prev_state = atomic_load(&philosophers[prev_id].state);
    int next_state = atomic_load(&philosophers[next_id].state);
    int no_one_eating = !is_anyone_eating();
    int lowest = get_lowest_count();
    int my_count = atomic_load(&philosopher->invoke_count);
    
    int has_priority = (my_count == lowest) || 
                      (current_time - philosopher->wait_start >= MAX_WAIT_TIME/2);
    
    int can_eat = has_priority && 
                 prev_state != 3 && 
                 next_state != 3 && 
                 (no_one_eating || atomic_load(&philosopher->must_think) == 0);
    
    if (can_eat) {
        atomic_store(&philosopher->state, 3);
    } else if (current_time - philosopher->wait_start >= MAX_WAIT_TIME) {
        atomic_store(&philosopher->state, 1);
    }
    
    pthread_mutex_unlock(&state_mutex);
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
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            printf("Philosopher %d - State: %d, Invoke count: %d, Must think: %d\n", 
                   i,  // Using i instead of philosopher_id to ensure order
                   atomic_load(&philosophers[i].state),
                   atomic_load(&philosophers[i].invoke_count),
                   atomic_load(&philosophers[i].must_think));
        }
        pthread_mutex_unlock(&print_mutex);
        sleep(1);
    }
    return NULL;
}

int main() {
    // Set up signal handling
    signal(SIGINT, handle_signal);

    pthread_mutex_init(&print_mutex, NULL);
    pthread_mutex_init(&state_mutex, NULL);
    
    srand((unsigned int)time(NULL));
    
    // Initialize philosophers
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        atomic_init(&philosophers[i].state, 1);
        philosophers[i].philosopher_id = i;
        atomic_init(&philosophers[i].invoke_count, 0);
        atomic_init(&philosophers[i].must_think, 0);
        philosophers[i].wait_start = 0;
    }
    
    int randomNum = get_random(0, NUM_PHILOSOPHERS - 1);
    atomic_store(&philosophers[randomNum].state, 2);
    philosophers[randomNum].wait_start = time(NULL);
    
    pthread_t philosopher_threads[NUM_PHILOSOPHERS];
    pthread_t status_thread;
    
    // Create threads
    pthread_create(&status_thread, NULL, print_status, NULL);
    
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_create(&philosopher_threads[i], NULL, philosopher_routine, &philosophers[i]);
    }
    
    // Main management loop
    while (atomic_load(&running)) {
        pthread_mutex_lock(&state_mutex);
        
        int lowest = get_lowest_count();
        int waiting_count = 0;
        
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            if (atomic_load(&philosophers[i].state) == 2) {
                waiting_count++;
            }
        }
        
        if (waiting_count < 2) {
            int eligible[NUM_PHILOSOPHERS];
            int eligible_count = 0;
            
            for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
                if (atomic_load(&philosophers[i].state) == 1 && 
                    atomic_load(&philosophers[i].invoke_count) == lowest) {
                    eligible[eligible_count++] = i;
                }
            }
            
            if (eligible_count > 0) {
                int num_to_add = 2 - waiting_count;
                
                for (int i = 0; i < num_to_add && i < eligible_count; i++) {
                    int idx = get_random(0, eligible_count - 1 - i);
                    int phil_id = eligible[idx];
                    eligible[idx] = eligible[eligible_count - 1 - i];
                    
                    atomic_store(&philosophers[phil_id].state, 2);
                    philosophers[phil_id].wait_start = time(NULL);
                }
            }
        }
        
        pthread_mutex_unlock(&state_mutex);
        usleep(100000); // 100ms delay
    }
    
    // Cleanup
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_cancel(philosopher_threads[i]);
        pthread_join(philosopher_threads[i], NULL);
    }
    pthread_cancel(status_thread);
    pthread_join(status_thread, NULL);
    
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&state_mutex);
    
    printf("\nProgram terminated gracefully\n");
    printf("\nFinal Status:\n");
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        printf("Philosopher %d - State: %d, Invoke count: %d, Must think: %d\n", 
               i,
               atomic_load(&philosophers[i].state),
               atomic_load(&philosophers[i].invoke_count),
               atomic_load(&philosophers[i].must_think));
    }

    return 0;
    return 0;
}
