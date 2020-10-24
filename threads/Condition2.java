package nachos.threads;

import nachos.machine.*;

import java.util.*;

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

    /**
     * Tests whether this module is working.
     */
    public static void selfTest() {
        System.out.println("-----\nEnter Condition2.selfTest");

        int[] item = new int[1];
        item[0] = 0;
        Lock lock = new Lock();
        Condition2 cond2 = new Condition2(lock);
        KThread[] producer = new KThread[5];
        KThread[] consumer = new KThread[5];
        Random r = new Random();
        int cp = 0, cc = 0;
        for (int i = 0; i < 10; ++i) {
            int type = r.nextInt(2);
            while (true) {
                if ((type == 0 && cp < 5) || (type == 1 && cc < 5))
                    break;
                type = r.nextInt(2);
            }
            if (type == 0) {
                producer[cp] = new KThread(new Producer(item, lock, cond2));
                producer[cp].fork();
                ++cp;
            } else {
                consumer[cc] = new KThread(new Consumer(item, lock, cond2));
                consumer[cc].fork();
                ++cc;
            }
        }
        ThreadedKernel.alarm.waitUntil(100000);
        System.out.println(item[0] + " item(s) left");

        System.out.println("Finish Condition2.selfTest\n*****");
    }

    private static class Consumer implements Runnable{
        private int[] item;
        private Lock lock;
        private Condition2 cond2;
        public Consumer(int[] _item, Lock _lock, Condition2 _cond2){
            item = _item;
            lock = _lock;
            cond2 = _cond2;
        }
        public void run(){
            lock.acquire();
            while (item[0] < 1) {
                System.out.println("Consumer: no item, sleep.");
                cond2.sleep();
            }
            item[0] -= 1;
            System.out.println("Consumer: consume 1 item.");
            lock.release();
        }
    }
    
    private static class Producer implements Runnable{
        private int[] item;
        private Lock lock;
        private Condition2 cond2;
        public Producer(int[] _item, Lock _lock, Condition2 _cond2){
            item = _item;
            lock = _lock;
            cond2 = _cond2;
        }
        public void run(){
            lock.acquire();
            item[0] += 1;
            System.out.println("Producer: produce 1 item.");
            cond2.wakeAll();
            lock.release();
        }
    }
}
