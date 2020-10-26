package nachos.threads;

import nachos.machine.*;

import java.util.Random;

/**
 * A <tt>Lock</tt> is a synchronization primitive that has two states,
 * <i>busy</i> and <i>free</i>. There are only two operations allowed on a
 * lock:
 *
 * <ul>
 * <li><tt>acquire()</tt>: atomically wait until the lock is <i>free</i> and
 * then set it to <i>busy</i>.
 * <li><tt>release()</tt>: set the lock to be <i>free</i>, waking up one
 * waiting thread if possible.
 * </ul>
 *
 * <p>
 * Also, only the thread that acquired a lock may release it. As with
 * semaphores, the API does not allow you to read the lock state (because the
 * value could change immediately after you read it).
 */
public class Lock {
    /**
     * Allocate a new lock. The lock will initially be <i>free</i>.
     */
    public Lock() {
    }

    /**
     * Atomically acquire this lock. The current thread must not already hold
     * this lock.
     */
    public void acquire() {
        Lib.assertTrue(!isHeldByCurrentThread());

        boolean intStatus = Machine.interrupt().disable();
        KThread thread = KThread.currentThread();

        if (lockHolder != null) {
            waitQueue.waitForAccess(thread);
            KThread.sleep();
        }
        else {
            waitQueue.acquire(thread);
            lockHolder = thread;
        }

        Lib.assertTrue(lockHolder == thread);

        Machine.interrupt().restore(intStatus);
    }

    /**
     * Atomically release this lock, allowing other threads to acquire it.
     */
    public void release() {
        Lib.assertTrue(isHeldByCurrentThread());

        boolean intStatus = Machine.interrupt().disable();

        if ((lockHolder = waitQueue.nextThread()) != null)
            lockHolder.ready();

        Machine.interrupt().restore(intStatus);
    }

    /**
     * Test if the current thread holds this lock.
     *
     * @return	true if the current thread holds this lock.
     */
    public boolean isHeldByCurrentThread() {
        return (lockHolder == KThread.currentThread());
    }

    /**
     *
     * @return lockHolder
     */
    public KThread getLockHolder() {
        return lockHolder;
    }

    /**
     * @return waitQueue
     */

    public ThreadQueue getWaitQueue() {
        return waitQueue;
    }

    private KThread lockHolder = null;
    private ThreadQueue waitQueue = ThreadedKernel.scheduler.newThreadQueue(true);

    public static void schedulerTest() {
        qTester.critical = new Lock();
        final int num_thread = 10;
        KThread[] qThreads = new KThread[num_thread];
        for (int i = 0; i < num_thread; i++) {
            qTester tmp_tester = new qTester();
            qThreads[i] = new KThread(tmp_tester).setName("qTester" + i);
            tmp_tester.thread = qThreads[i];
            boolean intStatus = Machine.interrupt().disable();
            ThreadedKernel.scheduler.setPriority(qThreads[i], Math.min(7, i));
            Machine.interrupt().restore(intStatus);
        }
        qTester.N = num_thread;
        qTester.threads = qThreads;
        qTester.ID = 0;
        qThreads[0].fork();

        ThreadedKernel.alarm.waitUntil(50000);

        KThread blocker = new KThread(new ThreadHogger());
        boolean intStatus = Machine.interrupt().disable();
        ThreadedKernel.scheduler.setPriority(blocker, 2);
        Machine.interrupt().restore(intStatus);
        blocker.fork();

        for (int i = 1; i < num_thread; i++) {
            qThreads[i].fork();
        }

        ThreadedKernel.alarm.waitUntil(50000);

    }

    public static class qTester implements Runnable {
        public KThread thread;
        public static KThread[] threads;
        public static Lock critical;
        public static int ID;
        public static int N;
        public void run() {
            System.out.print("Running: ");
            System.out.println(thread);
            critical.acquire();
            boolean intStatus = Machine.interrupt().disable();
            critical.getWaitQueue().print();
            Machine.interrupt().restore(intStatus);
            ID += 1;
            critical.release();
            System.out.print("Finished: ");
            System.out.println(thread);
        }
    }

    private static class ThreadHogger implements Runnable{
        public int d = 0;
        public void run() {
            while (d <= 10) {
                KThread.yield();
                ThreadedKernel.alarm.waitUntil(50000);
                d += 1;
            }
        }
    }
}
