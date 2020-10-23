package nachos.threads;

import nachos.machine.*;

/**
 * An implementation of condition variables that disables interrupt()s for
 * synchronization.
 *
 * <p>
 * You must implement this.
 *
 * @see	nachos.threads.Condition
 */
public class Condition2 {
    /**
     * Allocate a new condition variable.
     *
     * @param	conditionLock	the lock associated with this condition
     *				variable. The current thread must hold this
     *				lock whenever it uses <tt>sleep()</tt>,
     *				<tt>wake()</tt>, or <tt>wakeAll()</tt>.
     */
    public Condition2(Lock conditionLock) {
        this.conditionLock = conditionLock;
        waitQueue = ThreadedKernel.scheduler.newThreadQueue(false);
    }

    /**
     * Atomically release the associated lock and go to sleep on this condition
     * variable until another thread wakes it using <tt>wake()</tt>. The
     * current thread must hold the associated lock. The thread will
     * automatically reacquire the lock before <tt>sleep()</tt> returns.
     */
    public void sleep() {
        Lib.assertTrue(conditionLock.isHeldByCurrentThread());

        boolean intStatus = Machine.interrupt().disable();
        conditionLock.release();
        
        waitQueue.waitForAccess(KThread.currentThread());
        KThread.sleep();
        
        conditionLock.acquire();
        Machine.interrupt().restore(intStatus);
    }

    /**
     * Wake up at most one thread sleeping on this condition variable. The
     * current thread must hold the associated lock.
     */
    public void wake() {
        Lib.assertTrue(conditionLock.isHeldByCurrentThread());

        boolean intStatus = Machine.interrupt().disable();
        KThread nxtThread = waitQueue.nextThread();
        if (nxtThread != null)
            nxtThread.ready();
        Machine.interrupt().restore(intStatus);
    }

    /**
     * Wake up all threads sleeping on this condition variable. The current
     * thread must hold the associated lock.
     */
    public void wakeAll() {
        Lib.assertTrue(conditionLock.isHeldByCurrentThread());

        boolean intStatus = Machine.interrupt().disable();
        while (true) {
            KThread nxtThread = waitQueue.nextThread();
            if (nxtThread == null)
                break;
            nxtThread.ready();
        }
        Machine.interrupt().restore(intStatus);
    }

    private Lock conditionLock;
    private ThreadQueue waitQueue = null;

    private static final char dbgCond2 = 'c';

    /**
     * Tests whether this module is working.
     */
    public static void selfTest() {
        Lib.debug(dbgCond2, "Enter Condition2.selfTest");

        //new KThread(new PingTest(1)).setName("forked thread").fork();
        //new PingTest(0).run();
        Lib.debug(dbgCond2, "Finish Condition2.selfTest\n*****");
    }
}
