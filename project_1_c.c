/*
* Current Date and Time (UTC): 2025-04-03 13:07:48
* Current User's Login: bratMaciek
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>

#define NUM_PHILOSOPHERS 5
#define MAX_WAIT_TIME 3  // Maximum waiting time before going back to thinking

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

int get_random(int max) {
    return rand() % (max + 1);
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
    
    int random_time = get_random(4);
    sleep(random_time);
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
    
    int random_time = get_random(5);
    sleep(random_time);
    
    if (atomic_load(&philosopher->must_think)) {
        int lowest = get_lowest_count();
        if (atomic_load(&philosopher->invoke_count) <= (lowest + 1)) {
            atomic_store(&philosopher->must_think, 0);
        }
    }
}

void wait(Philosopher* philosopher) {
    // Check if waited too long
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
    
    int no_one_eating = !is_anyone_eating();
    
    if ((atomic_load(&philosopher->must_think) == 0 || no_one_eating) && 
        atomic_load(&philosophers[prev_id].state) == 1 && 
        (atomic_load(&philosophers[next_id].state) == 1 || 
         atomic_load(&philosophers[next_id].state) == 2)) {
        
        pthread_mutex_lock(&state_mutex);
        atomic_store(&philosopher->state, 3);
        pthread_mutex_unlock(&state_mutex);
        return;
    }
    
    if (atomic_load(&philosopher->must_think)) {
        atomic_store(&philosopher->state, 1);
    }
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
    
    srand(time(NULL));
    
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        atomic_init(&philosophers[i].state, 1);
        philosophers[i].philosopher_id = i;
        atomic_init(&philosophers[i].invoke_count, 0);
        atomic_init(&philosophers[i].must_think, 0);
        philosophers[i].wait_start = 0;
    }
    
    int randomNum = get_random(4);
    atomic_store(&philosophers[randomNum].state, 2);
    philosophers[randomNum].wait_start = time(NULL);  // Set initial wait time
    
    while (1) {
        pthread_t threads[NUM_PHILOSOPHERS];
        printf("\n");
        
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            pthread_create(&threads[i], NULL, execute_task, &philosophers[i]);
        }
        
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            pthread_join(threads[i], NULL);
        }
        
        int random_add = get_random(1) + 2;
        int first_philosopher_id = lowest_invoke_count();
        
        pthread_mutex_lock(&state_mutex);
        atomic_store(&philosophers[first_philosopher_id].state, 2);
        philosophers[first_philosopher_id].wait_start = time(NULL);  // Set wait start time
        
        int second_philosopher_id = (first_philosopher_id + random_add) % NUM_PHILOSOPHERS;
        atomic_store(&philosophers[second_philosopher_id].state, 2);
        philosophers[second_philosopher_id].wait_start = time(NULL);  // Set wait start time
        pthread_mutex_unlock(&state_mutex);
        
        pthread_mutex_lock(&print_mutex);
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            printf("invoke count philosopher %d: %d (must_think: %d)\n", 
                   philosophers[i].philosopher_id, 
                   atomic_load(&philosophers[i].invoke_count),
                   atomic_load(&philosophers[i].must_think));
        }
        pthread_mutex_unlock(&print_mutex);
    }
    
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&state_mutex);
    return 0;
}