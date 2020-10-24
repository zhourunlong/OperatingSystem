package nachos.threads;

import nachos.machine.*;

import java.util.*;

/**
 * A scheduler that chooses threads based on their priorities.
 *
 * <p>
 * A priority scheduler associates a priority with each thread. The next thread
 * to be dequeued is always a thread with priority no less than any other
 * waiting thread's priority. Like a round-robin scheduler, the thread that is
 * dequeued is, among all the threads of the same (highest) priority, the
 * thread that has been waiting longest.
 *
 * <p>
 * Essentially, a priority scheduler gives access in a round-robin fashion to
 * all the highest-priority threads, and ignores all other threads. This has
 * the potential to
 * starve a thread if there's always a thread waiting with higher priority.
 *
 * <p>
 * A priority scheduler must partially solve the priority inversion problem; in
 * particular, priority must be donated through locks, and through joins.
 */
public class PriorityScheduler extends Scheduler {
    /**
     * Allocate a new priority scheduler.
     */
    public PriorityScheduler() {
    }

    /**
     * Allocate a new priority thread queue.
     *
     * @param	transferPriority	<tt>true</tt> if this queue should
     *					transfer priority from waiting threads
     *					to the owning thread.
     * @return	a new priority thread queue.
     */
    public ThreadQueue newThreadQueue(boolean transferPriority) {
        return new PriorityQueue(transferPriority);
    }

    public int getPriority(KThread thread) {
        Lib.assertTrue(Machine.interrupt().disabled());

        return getThreadState(thread).getPriority();
    }

    public int getEffectivePriority(KThread thread) {
        Lib.assertTrue(Machine.interrupt().disabled());

        return getThreadState(thread).getEffectivePriority();
    }

    public void setPriority(KThread thread, int priority) {
        Lib.assertTrue(Machine.interrupt().disabled());

        Lib.assertTrue(priority >= priorityMinimum &&
                priority <= priorityMaximum);

        getThreadState(thread).setPriority(priority);
    }

    public boolean increasePriority() {
        boolean intStatus = Machine.interrupt().disable();

        KThread thread = KThread.currentThread();

        int priority = getPriority(thread);
        if (priority == priorityMaximum)
            return false;

        setPriority(thread, priority+1);

        Machine.interrupt().restore(intStatus);
        return true;
    }

    public boolean decreasePriority() {
        boolean intStatus = Machine.interrupt().disable();

        KThread thread = KThread.currentThread();

        int priority = getPriority(thread);
        if (priority == priorityMinimum)
            return false;

        setPriority(thread, priority-1);

        Machine.interrupt().restore(intStatus);
        return true;
    }

    /**
     * The default priority for a new thread. Do not change this value.
     */
    public static final int priorityDefault = 1;
    /**
     * The minimum priority that a thread can have. Do not change this value.
     */
    public static final int priorityMinimum = 0;
    /**
     * The maximum priority that a thread can have. Do not change this value.
     */
    public static final int priorityMaximum = 7;

    /**
     * Return the scheduling state of the specified thread.
     *
     * @param	thread	the thread whose scheduling state to return.
     * @return	the scheduling state of the specified thread.
     */
    protected ThreadState getThreadState(KThread thread) {
        if (thread.schedulingState == null)
            thread.schedulingState = new ThreadState(thread);

        return (ThreadState) thread.schedulingState;
    }

    /**
     * A <tt>ThreadQueue</tt> that sorts threads by priority.
     */
    protected class PriorityQueue extends ThreadQueue {
        PriorityQueue(boolean transferPriority) {
            for(int i = priorityMinimum; i <= priorityMaximum; i++) {
                waitQueues[i] = new LinkedList<KThread>();
            }
            this.transferPriority = transferPriority;
        }

        public void waitForAccess(KThread thread) {
            Lib.assertTrue(Machine.interrupt().disabled());
            getThreadState(thread).waitForAccess(this);
            print();
        }

        public void acquire(KThread thread) {
            Lib.assertTrue(Machine.interrupt().disabled());
            getThreadState(thread).acquire(this);
        }

        public KThread nextThread() {
            Lib.assertTrue(Machine.interrupt().disabled());
            // implement me
            ThreadState next_thread_state = pickNextThread();
            if(next_thread_state != null) {
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
        protected ThreadState pickNextThread() {
            // implement me
            print();
            long Min = Long.MAX_VALUE;
            ThreadState next_thread_state = null;
            for(int i = priorityMaximum; i >= priorityMinimum; i--) {
                if(!waitQueues[i].isEmpty()) {
                    for(Iterator<KThread> j = waitQueues[i].iterator(); j.hasNext(); ) {
                        KThread tmp_thread = j.next();
                        if (thread_info.get(tmp_thread) == null)
                            System.out.println("WRONG");
                        if(Min > thread_info.get(tmp_thread)) {
                            Min = thread_info.get(tmp_thread);
                            next_thread_state = getThreadState(tmp_thread);
                        }
                    }
                    if(next_thread_state != null) {
                        acquire(next_thread_state.thread);
                    }
                    return next_thread_state;
                }
            }
            // System.out.println(1);
            return null;
        }

        /**
         * Return the highest priority of threads in the associated queue.
         */

        protected int highestPriority() {
            for (int i = priorityMaximum; i >= priorityMinimum; i--) {
                if(!waitQueues[i].isEmpty()) {
                    return i;
                }
            }
            return 0;
        }

        public void print() {
            Lib.assertTrue(Machine.interrupt().disabled());
            // implement me (if you want)
            // System.out.println("Current queue info: ");
            boolean is_queue_empty = true;

            for (int j = priorityMaximum; j >= priorityMinimum; j--) {
                for (Iterator i = waitQueues[j].iterator(); i.hasNext(); ) {
                    KThread tmp_thread = (KThread) i.next();
                    System.out.print(tmp_thread + " ");
                    System.out.print(getThreadState(tmp_thread).getEffectivePriority() + " ");
                    is_queue_empty = false;
                }
            }

            if (current_holder != null && !is_queue_empty) {
                System.out.println();
                System.out.print(current_holder + " ");
                System.out.print(getThreadState(current_holder).getEffectivePriority() + " ");
                is_queue_empty = false;
            }

            if(!is_queue_empty) {
                System.out.println();
                System.out.println();
            }
        }

        /**
         * <tt>true</tt> if this queue should transfer priority from waiting
         * threads to the owning thread.
         */
        public boolean transferPriority;
        protected LinkedList<KThread>[] waitQueues = new LinkedList [priorityMaximum + 1];
        protected Map<KThread, Long> thread_info = new HashMap<>();
        protected KThread current_holder = null;
    }

    /**
     * The scheduling state of a thread. This should include the thread's
     * priority, its effective priority, any objects it owns, and the queue
     * it's waiting for, if any.
     *
     * @see	nachos.threads.KThread#schedulingState
     */
    protected class ThreadState {
        /**
         * Allocate a new <tt>ThreadState</tt> object and associate it with the
         * specified thread.
         *
         * @param	thread	the thread this state belongs to.
         */
        public ThreadState(KThread thread) {
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
            for(Iterator<PriorityQueue> i = list_of_queue.iterator(); i.hasNext(); ) {
                PriorityQueue tmp_queue = i.next();
                Lib.assertTrue(tmp_queue.waitQueues[this.effective_priority].contains(this.thread));
                tmp_queue.waitQueues[this.effective_priority].remove(this.thread);
                tmp_queue.waitQueues[priority].add(this.thread);
            }
            this.priority = priority;
            this.effective_priority = priority;
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
            for(Iterator<PriorityQueue> i = list_of_queue.iterator(); i.hasNext(); ) {
                PriorityQueue tmp_queue = i.next();
                Lib.assertTrue(tmp_queue.waitQueues[this.effective_priority].contains(this.thread));
                tmp_queue.waitQueues[this.effective_priority].remove(this.thread);
                tmp_queue.waitQueues[effectivePriority].add(this.thread);
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
        public void waitForAccess(PriorityQueue waitQueue) {
            // implement me
            if(this.list_of_queue.contains(waitQueue) || this.holding_resources.contains(waitQueue)) {
                return ;
            }
            long birth = Machine.timer().getTime();
            waitQueue.waitQueues[this.effective_priority].add(this.thread);
            waitQueue.thread_info.put(this.thread, birth);
            list_of_queue.add(waitQueue);
            if(waitQueue.transferPriority) {
                if(waitQueue.current_holder != null) {
                    ThreadState current_holder_state = getThreadState(waitQueue.current_holder);
                    current_holder_state.setEffectivePriority(Math.max(this.effective_priority, current_holder_state.effective_priority));
                    current_holder_state.update();
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
        public void acquire(PriorityQueue waitQueue) {
            // implement me
            Lib.assertTrue(Machine.interrupt().disabled());
            if (waitQueue.current_holder != null) {
                getThreadState(waitQueue.current_holder).holding_resources.remove(waitQueue);
                getThreadState(waitQueue.current_holder).recalculateDonation();
            }
            waitQueue.current_holder = this.thread;
            if (waitQueue.transferPriority) {
                this.holding_resources.add(waitQueue);
            }
            waitQueue.waitQueues[this.effective_priority].remove(this.thread);
            waitQueue.thread_info.remove(this.thread);
            list_of_queue.remove(waitQueue);
            recalculateDonation();
        }

        /**
         * Called when the associated thread donates its priority to the thread which occupies the requested lock.
         */

        protected void update() {
            for(Iterator<PriorityQueue> i = list_of_queue.iterator(); i.hasNext(); ) {
                PriorityQueue suc_queue = i.next();
                if(!suc_queue.transferPriority)
                    continue;
                KThread suc_thread = suc_queue.current_holder;
                if(suc_thread == null)
                    continue;
                if(this.effective_priority <= getThreadState(suc_thread).getEffectivePriority())
                    continue;
                getThreadState(suc_thread).setEffectivePriority(this.effective_priority);
                getThreadState(suc_thread).update();
            }
        }

        /**
         * Called when a lock is revoked from the associated thread or the associated thread get a new lock.
         */

        protected void recalculateDonation() {
            int Max = -1;
            for(Iterator<PriorityQueue> i = holding_resources.iterator(); i.hasNext(); ) {
                PriorityQueue donating_queue = i.next();
                int tmp_priority = donating_queue.highestPriority();
                Max = Math.max(tmp_priority, Max);
            }
            Max = Math.max(this.priority, Max);
            if (Max == this.effective_priority) {
                return ;
            }
            this.setEffectivePriority(Max);
            for(Iterator<PriorityQueue> i = list_of_queue.iterator(); i.hasNext(); ) {
                PriorityQueue suc_queue = i.next();
                if (!suc_queue.transferPriority)
                    continue;
                KThread suc_thread = suc_queue.current_holder;
                if(suc_thread == null)
                    continue;
                getThreadState(suc_thread).recalculateDonation();
            }
        }

        /** The thread with which this object is associated. */
        protected KThread thread;
        /** The priority of the associated thread. */
        protected int priority;
        /** The effective priority of the associated thread. */
        protected int effective_priority = 0;
        /** The queues that contains the associate thread. */
        protected LinkedList<PriorityQueue> list_of_queue = new LinkedList<>();
        protected LinkedList<PriorityQueue> holding_resources = new LinkedList<>();
    }
}
