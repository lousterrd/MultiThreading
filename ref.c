My brief pseudo code below... leaving out a bunch; just including the main pieces
for the task queue and the signaling, we do, in fact, use a single mutex for both
Use the taskQueue_mutex
You should name the "cond" variable to something like "taskReadyCond"
The idea is that you're using the variable to say that a task is ready for processing

Look carefully at the order the mutex and cond functions are called below; it's the correct order

Oh - he doesn't want your debug output when he runs his tests
- That's why he wants you to use a #define DEBUG thing for any testing printfs
- Then you can turn it on and off quickly... and submit it with it turned off
- He's gonna run a bunch of test cases with lots of threads and doesn't want to see debug output


main()
   while reading tasks from file
      if task is a processing task
         mutex_lock(mutex)
         *Add to queue
         cond_signal(cond) // wake thread; call this before unlocking
         mutex_unlock(mutex)
     
   done = true
   cond_broadcast()
   join()   
   
   
worker()
   while(!done)
      mutex_lock(mutex)
      cond_wait(cond, mutex) //automatically releases lock
      // when awoken, lock is automatically made again
      // is possible to be awoken by the system unexpectedly... make sure to check things correctly and go back to waiting if not needed
      // get rid of the 3 lines with wait_mutex

      while queue is not empty
         remove task from queue
	   (and set queue to NULL if now empty... and last_node to false)

         mutex_unlock(mutex)
         // other queue can now be awoken
         process task, including sleep time
         mutex_lock(mutex)
         
      
      mutex_unlock(mutex)
      
      pthread_exit(NULL)