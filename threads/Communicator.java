package nachos.threads;

import nachos.machine.*;

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

        while (inUse)
            speaker.sleep();

        inUse = true;
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

        while (!inUse)
            listener.sleep();

        inUse = false;
        int ret = word;
        speaker.wake();

        lock.release();
        return ret;
    }

    private int word;
    private boolean inUse = false;
    private Lock lock;
    private Condition2 speaker, listener;

    private static final char dbgCommunicator = 'c';

    public static void selfTest() {
        Lib.debug(dbgCommunicator, "Enter Communicator.selfTest");

        //new KThread(new PingTest(1)).setName("forked thread").fork();
        //new PingTest(0).run();
        Lib.debug(dbgCommunicator, "Finish Communicator.selfTest\n*****");
    }
}
