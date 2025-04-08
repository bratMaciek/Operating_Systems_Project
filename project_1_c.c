#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>

#define NUM_PHILOSOPHERS 5
#define MAX_WAIT_TIME 6  // Maximum waiting time before going back to thinking

typedef struct {
    atomic_int state;  // 1: thinking, 2: waiting, 3: eating
    int philosopher_id;
    atomic_int invoke_count;
    atomic_int must_think;
    time_t wait_start;  // Time when philosopher started waiting
} Philosopher;

Philosopher philosophers[NUM_PHILOSOPHERS];
pthread_mutex_t print_mutex;
pthread_mutex_t state_mutex;

// Move random seed initialization to main()
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

void eat(Philosopher* philosopher) {
    pthread_mutex_lock(&print_mutex);
    printf("Philosopher %d is eating.\n", philosopher->philosopher_id);
    pthread_mutex_unlock(&print_mutex);
    
    // Use microseconds for sleep to allow more variation
    // usleep(get_random(1000, 4000));  // 1-4 milliseconds
    sleep(get_random(1, 4));  // 1-5 milliseconds
    
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
    
    //usleep(get_random(1000, 5000));  // 1-5 milliseconds
    sleep(get_random(1, 5));  // 1-5 milliseconds
    

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
    
    // Priority condition: either has lowest count or has been waiting too long
    int has_priority = (my_count == lowest) || 
                      (current_time - philosopher->wait_start >= MAX_WAIT_TIME/2);
    
    // Modified eating condition
    int can_eat = has_priority && 
                 prev_state != 3 && // prev not eating
                 next_state != 3 && // next not eating
                 (no_one_eating || atomic_load(&philosopher->must_think) == 0);
    
    if (can_eat) {
        atomic_store(&philosopher->state, 3); // eating
    } else if (current_time - philosopher->wait_start >= MAX_WAIT_TIME) {
        // Force back to thinking if waited too long
        atomic_store(&philosopher->state, 1);
    }
    
    pthread_mutex_unlock(&state_mutex);
}

int lowest_invoke_count() {
    int lowest_invoke_philosopher_id = 0;
    int count = atomic_load(&philosophers[0].invoke_count);
    
    for (int i = 1; i < NUM_PHILOSOPHERS; i++) {
        int current_count = atomic_load(&philosophers[i].invoke_count);
        if (current_count < count) {
            count = current_count;
            lowest_invoke_philosopher_id = philosophers[i].philosopher_id;
        }
    }
    return lowest_invoke_philosopher_id;
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

int main() {
    pthread_mutex_init(&print_mutex, NULL);
    pthread_mutex_init(&state_mutex, NULL);
    
    // Initialize random seed only once at the start
    srand((unsigned int)time(NULL));
    
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        atomic_init(&philosophers[i].state, 1);
        philosophers[i].philosopher_id = i;
        atomic_init(&philosophers[i].invoke_count, 0);
        atomic_init(&philosophers[i].must_think, 0);
        philosophers[i].wait_start = 0;
    }
    
    int randomNum = get_random(0, 4);
    atomic_store(&philosophers[randomNum].state, 2);
    philosophers[randomNum].wait_start = time(NULL);
    
    while (1) {
        pthread_t threads[NUM_PHILOSOPHERS];
        
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            pthread_create(&threads[i], NULL, execute_task, &philosophers[i]);
        }
        
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            pthread_join(threads[i], NULL);
        }
        
        pthread_mutex_lock(&state_mutex);
        
        // Find philosophers with lowest invoke count
        int lowest = get_lowest_count();
        int waiting_count = 0;
        
        // Count currently waiting philosophers
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            if (atomic_load(&philosophers[i].state) == 2) {
                waiting_count++;
            }
        }
        
        // Only add new waiting philosophers if there aren't too many already waiting
        if (waiting_count < 2) {
            // Create array of eligible philosophers
            int eligible[NUM_PHILOSOPHERS];
            int eligible_count = 0;
            
            for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
                if (atomic_load(&philosophers[i].state) == 1 && // thinking
                    atomic_load(&philosophers[i].invoke_count) == lowest) {
                    eligible[eligible_count++] = i;
                }
            }
            
            // Add new waiting philosophers if we found eligible ones
            if (eligible_count > 0) {
                int num_to_add = 2 - waiting_count; // Add up to 2 total waiting
                
                for (int i = 0; i < num_to_add && i < eligible_count; i++) {
                    int idx = get_random(0, eligible_count - 1 - i);
                    int phil_id = eligible[idx];
                    
                    // Swap selected philosopher to end of array
                    eligible[idx] = eligible[eligible_count - 1 - i];
                    
                    // Set to waiting state
                    atomic_store(&philosophers[phil_id].state, 2);
                    philosophers[phil_id].wait_start = time(NULL);
                }
            }
        }
        
        pthread_mutex_unlock(&state_mutex);
        
        printf("\n");
        pthread_mutex_lock(&print_mutex);
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            printf("invoke count philosopher %d: %d (must_think: %d)\n", 
                   philosophers[i].philosopher_id, 
                   atomic_load(&philosophers[i].invoke_count),
                   atomic_load(&philosophers[i].must_think));
        }
        pthread_mutex_unlock(&print_mutex);
        
        // Longer delay between iterations to allow state changes to settle
        usleep(5000); // 5ms delay
    }
    
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&state_mutex);
    return 0;
}
