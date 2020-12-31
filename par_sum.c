/*
 * Louis Doherty
 *
 * par_sum.c
 *
 * CS 446.646 Project 1 (Pthreads)
 *
 * Compile with --std=c99
 */
#include <pthread.h>
#include <stdatomic.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEBUG 0
#define debug_printf(fmt, ...) do {if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while(0)

/******************************************************************************/
// Pthread mutexes and conditionals
static pthread_mutex_t taskQueue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t globalVariable_mutex = PTHREAD_MUTEX_INITIALIZER;

// worker threads wait on this
pthread_cond_t thread_cond = PTHREAD_COND_INITIALIZER;

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);


// aggregate variables
volatile long sum = 0;
volatile long odd = 0;
volatile long min = INT_MAX;
volatile long max = INT_MIN;
bool done = false;


/*****************************************************************************/

// Global linked list methods (task queue)
typedef struct node {
    long val;
    struct node * next;
} node_t;

// function prototypes
void print_list(node_t *head);
void push(node_t ** head, long val);
long remove_last(node_t *head);

// Function declarations
void print_list(node_t *head) {
    node_t *current = head;

    while (current != NULL) {
        printf("%ld ", current->val);
        current = current->next;
    }
}

void push(node_t ** head, long val) {
    node_t * new_node;
    new_node = (node_t *) malloc(sizeof(node_t));

    new_node->val = val;
    new_node->next = *head;
    *head = new_node;
}

long pop(node_t ** head) {
    long retval = -1;
    node_t * next_node = NULL;

    if (*head == NULL) {
        return -1;
    }

    next_node = (*head)->next;
    retval = (*head)->val;
    free(*head);
    *head = next_node;

    return retval;
}

long remove_last(node_t * head) {
    long retval = 0;
    /* if there is only one item in the list, remove it */
    if (head->next == NULL) {
        retval = head->val;
        free(head);
        return retval;
    }

    /* get to the second to last node in the list */
    node_t * current = head;
    while (current->next->next != NULL) {
        current = current->next;
    }

    retval = current->next->val;
    free(current->next);
    current->next = NULL;
    return retval;
}

// Global linked list
node_t *taskQueue = NULL;

/*****************************************************************************/



// function prototypes
void calculate_square(long number);
void *workerFunction(void *line);


/*
 * update global aggregate variables given a number
 */

void calculate_square(long number)
{
  // calculate the square
  long the_square = number * number;

  // ok that was not so hard, but let's pretend it was
  // simulate how hard it is to square this number!
  sleep(number);

  pthread_mutex_lock(&globalVariable_mutex);
  // let's add this to our (global) sum
  sum += the_square;

  // now we also tabulate some (meaningless) statistics
  if (number % 2 == 1) {
    // how many of our numbers were odd?
    odd++;
  }

  // what was the smallest one we had to deal with?
  if (number < min) {
    min = number;
  }

  // and what was the biggest one?
  if (number > max) {
    max = number;
  }
  pthread_mutex_unlock(&globalVariable_mutex);
}


// The worker thread function called when new threads are created
void *workerFunction(void *threadID) {

  long thread_id = (long) threadID;
  bool last_node = false;     // Used to determine when the queue is empty

  debug_printf("Worker %ld starting.\n", thread_id);

  // Stay in loop until the master finishes processing the file
  while (!done) {
    debug_printf("Worker %ld going idle.\n", thread_id);

    pthread_mutex_lock(&taskQueue_mutex);
    pthread_cond_wait(&thread_cond, &taskQueue_mutex);

    debug_printf("Worker %ld awakening.\n", thread_id);

    // removeLast doesn't know where the head of the linked list is
    // the list must be manually reset to NULL
    while (taskQueue != NULL) {
      debug_printf("Worker %ld processing non-empty queue.\n", thread_id);

      if (taskQueue->next == NULL) {
        last_node = true;
      }

      long val = remove_last(taskQueue);
      debug_printf("Worker %ld removed %ld from queue\n", thread_id ,val);

      if (last_node){
        taskQueue = NULL;
        last_node = false;
      }

      pthread_mutex_unlock(&taskQueue_mutex);
      calculate_square(val);
      pthread_mutex_lock(&taskQueue_mutex);
    }
    debug_printf("%s\n", "Queue empty.");
    // Unlock the task queue
    pthread_mutex_unlock(&taskQueue_mutex);
  }

  debug_printf("Worker %ld finishing.\n", thread_id);
  return NULL;
}



// Master Thread
int main(int argc, char* argv[])
{
  // check and parse command line options
  if (argc != 3) {
    printf("Usage: par_sum <infile> <numThreads>\n");
    exit(EXIT_FAILURE);
  }
  char *fn = argv[1];

  // load numbers and add them to the queue
  FILE *fin = fopen(fn, "r");
  char action;
  long num;

  // Check if positive; else give error
  long numThreads = strtol(argv[2], NULL, 10);
  debug_printf("Number of threads: %ld\n", numThreads);

  if (!(numThreads > 0)) {
    printf("Error, please enter a positive integer.\n");
    exit(EXIT_FAILURE);
  }


  // Create array of pthreads for worker threads
  pthread_t workers[numThreads];
  long i;
  for (i = 0; i < numThreads; i++) {
    pthread_create(&workers[i], NULL, workerFunction, (void *)i);
  }


  while (fscanf(fin, "%c %ld\n", &action, &num) == 2) {
    debug_printf("%c ", action);
    debug_printf("%ld\n", num);

    if (action == 'p') {            // process, do some work
      pthread_mutex_lock(&taskQueue_mutex);
      push(&taskQueue, num);        // append task to linked list (long)
      debug_printf("Master added %ld to queue.\n", num);
      pthread_cond_signal(&thread_cond);
      pthread_mutex_unlock(&taskQueue_mutex);

    }
    else if (action == 'w') {     // simulate a delay in incoming data
      sleep(num);
    }
    else {
      printf("ERROR: Unrecognized action: '%c'\n", action);
      exit(EXIT_FAILURE);
    }
  }
  fclose(fin);
  done = true;

  // Wake up all threads and wait for them to
  // recognize that the master is done processing
  pthread_cond_broadcast(&thread_cond);

  // Clean up and return
  for (long i = 0; i < numThreads; i++) {
    pthread_join(workers[i], NULL);
  }
  // Print results
  printf("%ld %ld %ld %ld\n", sum, odd, min, max);
  return (EXIT_SUCCESS);
}
