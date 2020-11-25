package nachos.threads;

import nachos.machine.*;

import javax.crypto.Mac;
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
        waitQueue = ThreadedKernel.scheduler.newThreadQueue(true);
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
    private ThreadQueue waitQueue;

    public static void schedulerTest() {
        qTester.critical = new Lock();
        final int num_thread = 10;
        KThread[] qThreads = new KThread[num_thread];
        for (int i = 0; i < num_thread; i++) {
            qTester tmp_tester = new qTester();
            qThreads[i] = new KThread(tmp_tester).setName("qTester" + i);
            tmp_tester.thread = qThreads[i];
            boolean intStatus = Machine.interrupt().disable();
            ThreadedKernel.scheduler.setPriority(qThreads[i], Math.min(7, i + 2));
            Machine.interrupt().restore(intStatus);
        }

        qTester.N = num_thread;
        qTester.threads = qThreads;
        qTester.ID = 0;
        boolean intStatus = Machine.interrupt().disable();
        ThreadedKernel.scheduler.setPriority(qThreads[0], Math.min(7, 1));
        Machine.interrupt().restore(intStatus);
        qThreads[0].fork();

        while(qTester.critical.getLockHolder() == null) {
            ThreadedKernel.alarm.waitUntil(5000);
            KThread.yield();
        }

        System.out.print("LockHolder: ");
        System.out.println(qTester.critical.getLockHolder());

        for (int i = 1; i < num_thread; i++) {
            qThreads[i].fork();
        }

        KThread blocker = new KThread(new ThreadHogger());
        intStatus = Machine.interrupt().disable();
        ThreadedKernel.scheduler.setPriority(blocker, 2);
        Machine.interrupt().restore(intStatus);
        blocker.fork();

        KThread.yield();

        ThreadedKernel.alarm.waitUntil(100000000);

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
            boolean intStatus = Machine.interrupt().disable();
            critical.getWaitQueue().print();
            Machine.interrupt().restore(intStatus);
            critical.acquire();
            ID += 1;

            ThreadedKernel.alarm.waitUntil(10000000);
            // KThread.yield();
            System.out.print(thread);
            System.out.println(" releases the lock");
            critical.release();
            N -= 1;
            System.out.print("Finished: ");
            System.out.println(thread);
        }
    }

    private static class ThreadHogger implements Runnable{
        public int d = 0;
        public void run() {
            System.out.println("Hogger started");
            while (qTester.N >= 1) {
                KThread.yield();
            }
            System.out.println("Hogger finished");
        }
    }
}
