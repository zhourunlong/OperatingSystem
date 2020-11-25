package nachos.threads;

import nachos.machine.*;

import java.util.*;
/**
 * Uses the hardware timer to provide preemption, and to allow threads to sleep
 * until a certain time.
 */
public class Alarm {
    /**
     * Allocate a new Alarm. Set the machine's timer interrupt handler to this
     * alarm's callback.
     *
     * <p><b>Note</b>: Nachos will not function correctly with more than one
     * alarm.
     */
    public Alarm() {
        Machine.timer().setInterruptHandler(new Runnable() {
            public void run() { timerInterrupt(); }
        });
    }

    /**
     * The timer interrupt handler. This is called by the machine's timer
     * periodically (approximately every 500 clock ticks). Causes the current
     * thread to yield, forcing a context switch if there is another thread
     * that should be run.
     */
    public void timerInterrupt() {
        boolean intStatus = Machine.interrupt().disable();

        while (!Q.isEmpty() && Q.peek().Time() <= Machine.timer().getTime())
            Q.poll().Thread().ready();
        
        Machine.interrupt().restore(intStatus);

        // ???
        // KThread.yield();
        KThread.currentThread().yield();
    }

    /**
     * Put the current thread to sleep for at least <i>x</i> ticks,
     * waking it up in the timer interrupt handler. The thread must be
     * woken up (placed in the scheduler ready set) during the first timer
     * interrupt where
     *
     * <p><blockquote>
     * (current time) >= (WaitUntil called time)+(x)
     * </blockquote>
     *
     * @param	x	the minimum number of clock ticks to wait.
     *
     * @see	nachos.machine.Timer#getTime()
     */
    public void waitUntil(long x) {
        TimeThreadPair ttp = new TimeThreadPair(
                                Machine.timer().getTime() + x,
                                KThread.currentThread());

        boolean intStatus = Machine.interrupt().disable();

        Q.add(ttp);
        // ??? KThread.sleep();
        KThread.currentThread().sleep();
        
        Machine.interrupt().restore(intStatus);
    }

    private class TimeThreadPair {
        long Time;
        KThread Thread;
        public TimeThreadPair(long _Time, KThread _Thread) {
            Time = _Time;
            Thread = _Thread;
        }
        public long Time() {return Time;}
        public KThread Thread() {return Thread;}
    }
    static Comparator<TimeThreadPair> cmp = new Comparator<TimeThreadPair>() {
        public int compare(TimeThreadPair a, TimeThreadPair b) {
            long tmp = a.Time - b.Time;
            return tmp > 0 ? 1 : (tmp == 0 ? 0 : -1);
        }
    };

    PriorityQueue<TimeThreadPair> Q = new PriorityQueue<TimeThreadPair>(cmp);

    /**
     * Tests whether this module is working.
     */
    public static void selfTest() {
        System.out.println("-----\nEnter Alarm.selfTest");

        Random r = new Random();
        for (int i = 0; i < 5; ++i) {
            long tmp = r.nextInt(998) + 2;
            KThread thread = new KThread(new AlarmTest("Alarm" + i, tmp));
            thread.fork();
            ThreadedKernel.alarm.waitUntil(r.nextInt((int)(tmp / 2) + 1));
        }  

        ThreadedKernel.alarm.waitUntil(10000);
        System.out.println("Finish Alarm.selfTest\n*****");
    }

    private static class AlarmTest implements Runnable {
        private String name;
        private long time;
        public AlarmTest(String _name, long _time) {
            name = _name;
            time = _time;
        }
        public void run() {
            long sleepTime = Machine.timer().getTime();
            System.out.println(name + ": sleep at " + sleepTime + ", for " + time);
            ThreadedKernel.alarm.waitUntil(time);
            long wakeTime = Machine.timer().getTime();
            System.out.println(name + ": wake at " + wakeTime);
            if (wakeTime - sleepTime < time)
                System.out.println(name + ": not enough sleep!");
            if (wakeTime - sleepTime > time + 500)
                System.out.println(name + ": over sleep!");
        }
    }
}
