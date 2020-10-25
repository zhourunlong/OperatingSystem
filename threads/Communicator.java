package nachos.threads;

import nachos.machine.*;

import java.util.*;

/**
 * A <i>communicator</i> allows threads to synchronously exchange 32-bit
 * messages. Multiple threads can be waiting to <i>speak</i>,
 * and multiple threads can be waiting to <i>listen</i>. But there should never
 * be a time when both a speaker and a listener are waiting, because the two
 * threads can be paired off at this point.
 */
public class Communicator {
    /**
     * Allocate a new communicator.
     */
    public Communicator() {
        lock = new Lock();
        speaker = new Condition2(lock);
        listener = new Condition2(lock);
    }

    /**
     * Wait for a thread to listen through this communicator, and then transfer
     * <i>word</i> to the listener.
     *
     * <p>
     * Does not return until this thread is paired up with a listening thread.
     * Exactly one listener should receive <i>word</i>.
     *
     * @param	word	the integer to transfer.
     */
    public void speak(int _word) {
        lock.acquire();

        while (activeSpeak|| activeListen == 0)
            speaker.sleep();
        activeSpeak = true;
        word = _word;
        listener.wake();

        lock.release();
    }

    /**
     * Wait for a thread to speak through this communicator, and then return
     * the <i>word</i> that thread passed to <tt>speak()</tt>.
     *
     * @return	the integer transferred.
     */
    public int listen() {
        lock.acquire();

        activeListen += 1;
        while (!activeSpeak)
            listener.sleep();
        activeListen -= 1;
        activeSpeak = false;
        int ret = word;
        speaker.wake();

        lock.release();
        return ret;
    }

    private int word, activeListen = 0;
    private boolean activeSpeak = false;
    private Lock lock;
    private Condition2 speaker, listener;

    public static void selfTest() {
        System.out.println("-----\nEnter Communicator.selfTest");

        KThread[] speaker = new KThread[5];
        KThread[] listener = new KThread[5];
        Random r = new Random();
        Random rw = new Random();
        Communicator comm = new Communicator();

        int cs = 0, cl = 0;
        for (int i = 0; i < 10; ++i) {
            int type = r.nextInt(2);
            while (true) {
                if ((type == 0 && cs < 5) || (type == 1 && cl < 5))
                    break;
                type = r.nextInt(2);
            }
            if (type == 0) {
                speaker[cs] = new KThread(new Speaker("Speaker" + cs, rw, comm));
                speaker[cs].fork();
                ++cs;
            } else {
                listener[cl] = new KThread(new Listener("Listener" + cl, comm));
                listener[cl].fork();
                ++cl;
            }
        }

        ThreadedKernel.alarm.waitUntil(10000);
        System.out.println("Finish Communicator.selfTest\n*****");
    }

    private static class Speaker implements Runnable {
        String name;
        Random r;
        Communicator comm;
        public Speaker(String _name, Random _r, Communicator _comm) {
            name = _name;
            r = _r;
            comm = _comm;
            System.out.println(name);
        }
        public void run() {
            int tmp = r.nextInt(999);
            System.out.println(name + ": speak " + tmp);
            comm.speak(tmp);
        }
    }
    
    private static class Listener implements Runnable {
        String name;
        Communicator comm;
        public Listener(String _name, Communicator _comm) {
            name = _name;
            comm = _comm;
            System.out.println(name);
        }
        public void run() {
            int ret = comm.listen();
            System.out.println(name + ": listen " + ret);
        }
    }
}
