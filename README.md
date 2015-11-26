# sigsched
Unix singal-based native code scheduler

# TODO

  * Fix all valgrind complains
  * Block context switch in critical sections, like scheduler
  * Rewrite signal handling to controllable timer - SIGALRM, SIGVTALRM and SIGPROF
  * Implement dynamic process spawn
  * Implement own sleep function
  * Implement repl for introspection
  * Replace all existing scheduler implementations and get job offer from Google

# docs to read:

  * http://www.opennet.ru/base/dev/unix_signals.txt.html (Russian)
  * http://willnewton.name/uncategorized/setcontext-and-signal-handlers/
  * https://sourceware.org/ml/libc-alpha/2014-03/msg00124.html
  * http://www.unix.com/programming/90783-signal-handling-context-switches-2.html?s=444f66ad429417a930850e67eb5a0c77
  * https://labs.portcullis.co.uk/blog/ohm-2013-review-of-returning-signals-for-fun-and-profit/
  * http://www.researchgate.net/publication/2621969_Portable_Multithreading_-_The_Signal_Stack_Trick_For_User-Space_Thread_Creation
  * man 3 getcontext, makecontext, setcontext, swapcontext
  * http://cs2007.googlecode.com/svn/nreduce_full/examples/userthreads/userthreads.c
  * http://stackoverflow.com/questions/9145321/implementing-a-user-level-threads-package

