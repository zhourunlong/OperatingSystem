package nachos.threads;

import nachos.machine.*;

import java.util.*;

/**
 * A scheduler that chooses threads using a lottery.
 *
 * <p>
 * A lottery scheduler associates a number of tickets with each thread. When a
 * thread needs to be dequeued, a random lottery is held, among all the tickets
 * of all the threads waiting to be dequeued. The thread that holds the winning
 * ticket is chosen.
 *
 * <p>
 * Note that a lottery scheduler must be able to handle a lot of tickets
 * (sometimes billions), so it is not acceptable to maintain state for every
 * ticket.
 *
 * <p>
 * A lottery scheduler must partially solve the priority inversion problem; in
 * particular, tickets must be transferred through locks, and through joins.
 * Unlike a priority scheduler, these tickets add (as opposed to just taking
 * the maximum).
 */
public class LotteryScheduler extends PriorityScheduler {
    /**
     * Allocate a new lottery scheduler.
     */
    public LotteryScheduler() {
    }

    public int getPriority(KThread thread) {
        Lib.assertTrue(Machine.interrupt().disabled());

        return getLThreadState(thread).getPriority();
    }

    public int getEffectivePriority(KThread thread) {
        Lib.assertTrue(Machine.interrupt().disabled());

        return getLThreadState(thread).getEffectivePriority();
    }

    public void setPriority(KThread thread, int priority) {
        Lib.assertTrue(Machine.interrupt().disabled());

        Lib.assertTrue(priority >= priorityMinimumL &&
                priority <= priorityMaximumL);

        getLThreadState(thread).setPriority(priority);
    }

    public boolean increasePriority() {
        boolean intStatus = Machine.interrupt().disable();

        KThread thread = KThread.currentThread();

        int priority = getPriority(thread);
        if (priority == priorityMaximumL)
            return false;

        setPriority(thread, priority+1);

        Machine.interrupt().restore(intStatus);
        return true;
    }

    public boolean decreasePriority() {
        boolean intStatus = Machine.interrupt().disable();

        KThread thread = KThread.currentThread();

        int priority = getPriority(thread);
        if (priority == priorityMinimumL)
            return false;

        setPriority(thread, priority-1);

        Machine.interrupt().restore(intStatus);
        return true;
    }

    protected LThreadState getLThreadState(KThread thread) {
        if (thread.schedulingState == null)
            thread.schedulingState = new LThreadState(thread);

        return (LThreadState) thread.schedulingState;
    }

    /**
     * Allocate a new lottery thread queue.
     *
     * @param	transferPriority	<tt>true</tt> if this queue should
     *					transfer tickets from waiting threads
     *					to the owning thread.
     * @return	a new lottery thread queue.
     */
    public ThreadQueue newThreadQueue(boolean transferPriority) {
	// implement me
        return new LotteryQueue(transferPriority);
    }

    public class LotteryQueue extends ThreadQueue {
        LotteryQueue (boolean transferPriority) {
            this.transferPriority = transferPriority;
        }

        public void waitForAccess(KThread thread) {
            Lib.assertTrue(Machine.interrupt().disabled());
            getLThreadState(thread).waitForAccess(this);
        }

        public void acquire(KThread thread) {
            Lib.assertTrue(Machine.interrupt().disabled());
            getLThreadState(thread).acquire(this);
        }

        public KThread nextThread() {
            Lib.assertTrue(Machine.interrupt().disabled());
            // implement me
            LThreadState next_thread_state = pickNextThread();
            if(next_thread_state != null) {
                acquire(next_thread_state.thread);
                return next_thread_state.thread;
            }
            return null;
        }

        /**
         * Return the next thread that <tt>nextThread()</tt> would return,
         * without modifying the state of this queue.
         *
         * @return	the next thread that <tt>nextThread()</tt> would
         *		return.
         */
        protected LThreadState pickNextThread() {
            // implement me
            // print();
            int Sum = 0;
            for(Iterator<KThread> i = waitQueue.keySet().iterator(); i.hasNext(); ) {
                KThread tmp_thread = i.next();
                Sum += getLThreadState(tmp_thread).getEffectivePriority();
                // System.out.println(Sum);
            }
            if(Sum == 0) {
                // if(!waitQueue.keySet().isEmpty())
                    // System.out.println("asdad");
                return null;
            }
            Random rand =new Random(2313);
            int lottery = rand.nextInt(Sum);
            LThreadState next_thread_state;
            for(Iterator<KThread> i = waitQueue.keySet().iterator(); i.hasNext(); ) {
                KThread tmp_thread = i.next();
                lottery -= getLThreadState(tmp_thread).getEffectivePriority();
                if (lottery < 0) {
                    next_thread_state = getLThreadState(tmp_thread);
                    return next_thread_state;
                }
            }
            // System.out.println(1);
            return null;
        }

        protected int getPrioritySum() {
            int Sum = 0;
            for(Iterator<KThread> i = waitQueue.keySet().iterator(); i.hasNext(); ) {
                KThread tmp_thread = i.next();
                Sum += getLThreadState(tmp_thread).getPriority();
                // System.out.println(Sum);
            }
            return Sum;
        }

        public void print() {
            Lib.assertTrue(Machine.interrupt().disabled());
            // implement me (if you want)
            System.out.println("Current queue info: ");

            for(Iterator<KThread> i = waitQueue.keySet().iterator(); i.hasNext(); ) {
                KThread tmp_thread = i.next();
                System.out.print(tmp_thread + " " + waitQueue.get(tmp_thread));
            }

            System.out.println();
        }

        protected Map<KThread, Integer> waitQueue = new HashMap<>();
        public boolean transferPriority;
        protected KThread current_holder = null;

    }

    /**
     * The default priority for a new thread. Do not change this value.
     */
    public static final int priorityDefault = 1;
    /**
     * The minimum priority that a thread can have. Do not change this value.
     */
    public static final int priorityMinimumL = 1;
    /**
     * The maximum priority that a thread can have. Do not change this value.
     */
    public static final int priorityMaximumL = Integer.MAX_VALUE;

    protected class LThreadState {
        /**
         * Allocate a new <tt>LThreadState</tt> object and associate it with the
         * specified thread.
         *
         * @param	thread	the thread this state belongs to.
         */
        public LThreadState(KThread thread) {
            this.thread = thread;

            setPriority(priorityDefault);
            setEffectivePriority(priorityDefault);
        }

        /**
         * Return the priority of the associated thread.
         *
         * @return	the priority of the associated thread.
         */
        public int getPriority() {
            return priority;
        }

        /**
         * Return the effective priority of the associated thread.
         *
         * @return	the effective priority of the associated thread.
         */
        public int getEffectivePriority() {
            // implement me
            return effective_priority;
        }

        /**
         * Set the priority of the associated thread to the specified value.
         *
         * @param	priority	the new priority.
         */
        public void setPriority(int priority) {
            if (this.priority == priority)
                return;
            if (this.effective_priority >= priority) {
                this.priority = priority;
                return ;
            }
            for(Iterator<LotteryQueue> i = list_of_queue.iterator(); i.hasNext(); ) {
                LotteryQueue tmp_queue = i.next();
                Lib.assertTrue(tmp_queue.waitQueue.containsKey(this.thread));
                tmp_queue.waitQueue.replace(this.thread, this.effective_priority, priority);
            }
            this.priority = priority;
            this.effective_priority = priority;
            recalculateDonation();
            // implement me
        }

        /**
         * Set the effective priority of the associated value
         *
         * @param effectivePriority the new donated priority
         */

        public void setEffectivePriority(int effectivePriority) {
            if(effectivePriority == effective_priority) {
                return ;
            }
            for(Iterator<LotteryQueue> i = list_of_queue.iterator(); i.hasNext(); ) {
                LotteryQueue tmp_queue = i.next();
                Lib.assertTrue(tmp_queue.waitQueue.containsKey(this.thread));
                tmp_queue.waitQueue.replace(this.thread, this.effective_priority, effectivePriority);
            }
            this.effective_priority = effectivePriority;
        }

        /**
         * Called when <tt>waitForAccess(thread)</tt> (where <tt>thread</tt> is
         * the associated thread) is invoked on the specified priority queue.
         * The associated thread is therefore waiting for access to the
         * resource guarded by <tt>waitQueue</tt>. This method is only called
         * if the associated thread cannot immediately obtain access.
         *
         * @param	waitQueue	the queue that the associated thread is
         *				now waiting on.
         *
         * @see	nachos.threads.ThreadQueue#waitForAccess
         */
        public void waitForAccess(LotteryQueue waitQueue) {
            // implement me
            Lib.assertTrue(Machine.interrupt().disabled());
            if(this.list_of_queue.contains(waitQueue) /*|| this.holding_resources.contains(waitQueue)*/) {
                return ;
            }
            // long birth = Machine.timer().getTime();
            // waitQueue.thread_info.put(this.thread, birth);
            // System.out.println(this.effective_priority);
            waitQueue.waitQueue.put(this.thread, this.effective_priority);
            list_of_queue.add(waitQueue);
            if(waitQueue.transferPriority) {
                if(waitQueue.current_holder != null) {
                    LotteryScheduler.LThreadState current_holder_state = getLThreadState(waitQueue.current_holder);
                    // System.out.println(this.effective_priority + current_holder_state.effective_priority);
                    // current_holder_state.setEffectivePriority(this.effective_priority + current_holder_state.effective_priority);
                    current_holder_state.recalculateDonation();
                }
            }
        }

        /**
         * Called when the associated thread has acquired access to whatever is
         * guarded by <tt>waitQueue</tt>. This can occur either as a result of
         * <tt>acquire(thread)</tt> being invoked on <tt>waitQueue</tt> (where
         * <tt>thread</tt> is the associated thread), or as a result of
         * <tt>nextThread()</tt> being invoked on <tt>waitQueue</tt>.
         *
         * @see	nachos.threads.ThreadQueue#acquire
         * @see	nachos.threads.ThreadQueue#nextThread
         */
        public void acquire(LotteryQueue waitQueue) {
            // implement me
            Lib.assertTrue(Machine.interrupt().disabled());
            if (waitQueue.current_holder != null) {
                getLThreadState(waitQueue.current_holder).holding_resources.remove(waitQueue);
                getLThreadState(waitQueue.current_holder).recalculateDonation();
            }
            waitQueue.current_holder = this.thread;
            this.holding_resources.add(waitQueue);
            waitQueue.waitQueue.remove(this.thread);
            // waitQueue.thread_info.remove(this.thread);
            list_of_queue.remove(waitQueue);
            recalculateDonation();
        }


        /**
         * Called when a lock is revoked from the associated thread or the associated thread get a new lock.
         */

        protected void recalculateDonation() {
            int Sum = 0;
            for(Iterator<LotteryQueue> i = holding_resources.iterator(); i.hasNext(); ) {
                LotteryQueue donating_queue = i.next();
                int tmp_priority = donating_queue.getPrioritySum();
                if(donating_queue.transferPriority)
                    Sum = tmp_priority;
            }
            if(this.getEffectivePriority() == Sum + this.priority)
                return ;
            this.setEffectivePriority(Sum + this.priority);
            for(Iterator<LotteryQueue> i = list_of_queue.iterator(); i.hasNext(); ) {
                LotteryQueue suc_queue = i.next();
                if (!suc_queue.transferPriority)
                    continue;
                KThread suc_thread = suc_queue.current_holder;
                if(suc_thread == null)
                    continue;
                getLThreadState(suc_thread).recalculateDonation();
            }
        }

        /** The thread with which this object is associated. */
        protected KThread thread;
        /** The priority of the associated thread. */
        protected int priority = 1;
        /** The effective priority of the associated thread. */
        protected int effective_priority = 1;
        /** The queues that contains the associate thread. */
        protected LinkedList<LotteryQueue> list_of_queue = new LinkedList<>();
        protected LinkedList<LotteryQueue> holding_resources = new LinkedList<>();
    }
}
