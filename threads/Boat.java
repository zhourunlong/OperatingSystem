package nachos.threads;
import nachos.ag.BoatGrader;
import nachos.machine.Lib;

public class Boat
{
    static BoatGrader bg;


    // Define static variables.
    // (1) Location labels (for simplicity) and boat location.
    static final int Oahu    = 0;
    static final int Molokai = 1;
    static int boatLocation  = Oahu;

    // (2) Count of people on land and on boat.
    static int childrenAtOahu    = 0;
    static int adultsAtOahu      = 0;
    static int childrenAtMolokai = 0;
    static int adultsAtMolokai   = 0;
    static int passengersOnBoat  = 0;

    // (3) Synchronization objects.
    static Lock boatLock = new Lock();
    static Condition2 waitAtOahu    = new Condition2(boatLock);
    static Condition2 waitAtMolokai = new Condition2(boatLock);
    static Condition2 sailOnBoat    = new Condition2(boatLock);

    // (4) Communicator: report the number of people at Molokai.
    static Communicator reporterAtMolokai = new Communicator();


    // ***** Test cases provided below (with comments) *****
    public static void selfTest()
    {
        BoatGrader b = new BoatGrader();

        // System.out.println("\n*** Testing Boats with only 2 children ***");
        // begin(0, 2, b);

        // System.out.println("\n*** Testing Boats with 2 children & 1 adult ***");
        // begin(1, 2, b);

        // System.out.println("\n*** Testing Boats with 3 children & 3 adults ***");
        // begin(3, 3, b);

        // System.out.println("\n*** Testing Boats with 5 children & 5 adults ***");
        // begin(5, 5, b);

        // System.out.println("\n*** Testing Boats with 6 children & 6 adults ***");
        // begin(6, 6, b);

        // System.out.println("\n*** Testing Boats with 5 children & 6 adults ***");
        // begin(6, 5, b);

        // System.out.println("\n*** Testing Boats with 2 children & 200 adults ***");
        // begin(200, 2, b);

        // System.out.println("\n*** Testing Boats with 199 children & 2 adults ***");
        // begin(2, 199, b);

        // System.out.println("\n*** Testing Boats with 200 children & 2 adults ***");
        // begin(2, 200, b);

        System.out.println("\n*** Testing Boats with 75 children & 6 adults ***");
        begin(6, 75, b);
    }

    public static void begin( int adults, int children, BoatGrader b )
    {
        // Store the externally generated autograder in a class
        // variable to be accessible by children.
        bg = b;


        // Instantiate global variables here
        // Initialize counters
        childrenAtOahu    = children;
        adultsAtOahu      = adults;
        childrenAtMolokai = 0;
        adultsAtMolokai   = 0;


        // Create threads here. See section 3.4 of the Nachos for Java
        // Walkthrough linked from the projects page.
        Runnable sample_child = new Runnable() {
            public void run() { ChildItinerary(); };
        };

        Runnable sample_adult = new Runnable() {
            public void run() { AdultItinerary(); };
        };

        for (int i=0; i<children; i++) {
            KThread t = new KThread(sample_child);
            t.setName("Child #" + (i+1));
            t.fork();
        }

        for (int i=0; i<adults; i++) {
            KThread t = new KThread(sample_adult);
            t.setName("Adult #" + (i+1));
            t.fork();
        }

        while (true) {
            int reportedArrival = reporterAtMolokai.listen();
            System.out.println("--- Newly listened: " + reportedArrival + " people are now at Molokai.");
            if (reportedArrival == children + adults) {
                if (childrenAtMolokai + adultsAtMolokai == children + adults) {
                    break;
                } else {
                    System.out.println("--- THIS IS A MISREPORT. PROGRAM CONTINUES.");
                }
            }
        }
    }

    // ***** General strategy briefly explained *****
    // The key here is to have more people heading for Molokai than coming back to Oahu.
    // Therefore, a trip contributed most to success if:
    // (a) An adult travels to Molokai, while a child gets back to Oahu;
    // (b) Two children travel to Molokai, and one child gets back.
    // Children are crucial in this scenario, since they have an advantage over adults,
    // and thus are the only way to make progress.
    // Therefore, we always move children in pairs first (as many as possible),
    // and only deal with adults when #children <= 1.
    // In this way we shall not worry about the "residues" (adults or children).
    static void AdultItinerary()
    {
        bg.initializeAdult(); //Required for autograder interface. Must be the first thing called.
        //DO NOT PUT ANYTHING ABOVE THIS LINE.

        int myLocation = Oahu;
        boatLock.acquire();

        while (true) {
            if (myLocation == Oahu) {
                while ((boatLocation != Oahu) || (passengersOnBoat > 0) || (childrenAtOahu > 1))
                    waitAtOahu.sleep();  // Wait for a good opportunity.

                // Row the current thread to Molokai by himself.
                bg.AdultRowToMolokai();
                myLocation = Molokai;
                boatLocation = Molokai;
                adultsAtOahu--;
                adultsAtMolokai++;

                // Since the adult is in Molokai, he may report current number of arrivals.
                reporterAtMolokai.speak(childrenAtMolokai + adultsAtMolokai);

                // Wake up one children (adults remain asleep) at Molokai, and fall asleep.
                Lib.assertTrue(childrenAtMolokai > 0);
                waitAtMolokai.wakeAll();
                waitAtMolokai.sleep();
            } else if (myLocation == Molokai) {
                waitAtMolokai.sleep();  // Adults at Molokai always remain asleep (waking up does no good).
            } else {  // An error occurs.
                Lib.assertTrue(false);
                break;
            }
        }

        boatLock.release();
    }

    static void ChildItinerary()
    {
        bg.initializeChild(); //Required for autograder interface. Must be the first thing called.
        //DO NOT PUT ANYTHING ABOVE THIS LINE.

        int myLocation = Oahu;
        boatLock.acquire();

        while (true) {
            if (myLocation == Oahu) {
                while ( (boatLocation != Oahu) || (passengersOnBoat >= 2) ||
                        ((adultsAtOahu > 0) && (childrenAtOahu == 1)) )
                    waitAtOahu.sleep();  // Wait for a good opportunity.

                if ((adultsAtOahu == 0) && (childrenAtOahu == 1)) {  // Only 1 child to travel.
                    // Row the current thread to Molokai by himself.
                    bg.ChildRowToMolokai();
                    myLocation = Molokai;
                    boatLocation = Molokai;
                    childrenAtOahu--;
                    childrenAtMolokai++;
                    passengersOnBoat = 0;

                    // Since the child is in Molokai, he may report current number of arrivals.
                    reporterAtMolokai.speak(childrenAtMolokai+adultsAtMolokai);

                    // Fall asleep (probably forever).
                    waitAtMolokai.sleep();
                }
                // ***** Conventions on children traveling in pairs *****
                // Let the first child on board row, and the second on board ride.
                // Boat location, reported arrivals and further waking-ups are all updated by the second child.
                else if (childrenAtOahu >= 2) {
                    waitAtOahu.wakeAll();
                    passengersOnBoat++;

                    if (passengersOnBoat == 2) {  // Second child on board.
                        // Wake up the first child and fall asleep.
                        sailOnBoat.wake();
                        sailOnBoat.sleep();

                        // Ride the current thread to Molokai.
                        // This only happens when he is waken up by the first child.
                        bg.ChildRideToMolokai();
                        myLocation = Molokai;
                        boatLocation = Molokai;
                        childrenAtOahu--;
                        childrenAtMolokai++;
                        passengersOnBoat = 0;

                        // Since the child is in Molokai, he may report current number of arrivals.
                        reporterAtMolokai.speak(childrenAtMolokai+adultsAtMolokai);

                        // Wake up one children (adults remain asleep) at Molokai, and fall asleep.
                        waitAtMolokai.wakeAll();
                        waitAtMolokai.sleep();
                    } else if (passengersOnBoat == 1) {  // First child on board.
                        // Wait for another child.
                        sailOnBoat.sleep();

                        // Row the current thread to Molokai.
                        // This only happens when he is waken up by the second child.
                        bg.ChildRowToMolokai();
                        myLocation = Molokai;
                        childrenAtOahu--;
                        childrenAtMolokai++;
                        sailOnBoat.wake();  // Notify the second child to update boat state and leave.

                        // Fall asleep (waiting for the second child to take actions).
                        waitAtMolokai.sleep();
                    }
                }
            } else if (myLocation == Molokai) {
                while (boatLocation != Molokai)
                    waitAtMolokai.sleep();

                // The first child observing a boat at Molokai is responsible to drive it back to Oahu.
                Lib.assertTrue(childrenAtMolokai > 0);
                bg.ChildRowToOahu();
                myLocation = Oahu;
                boatLocation = Oahu;
                childrenAtMolokai--;
                childrenAtOahu++;

                // Handle an edge case: the child is the last person at Oahu.
                if ((adultsAtOahu == 0) && (childrenAtOahu == 1)) {
                    // Edge case: only 1 child to travel.
                    // Row the current thread to Molokai by himself.
                    bg.ChildRowToMolokai();
                    myLocation = Molokai;
                    boatLocation = Molokai;
                    childrenAtOahu--;
                    childrenAtMolokai++;
                    passengersOnBoat = 0;

                    // Since the child is in Molokai, he may report current number of arrivals.
                    reporterAtMolokai.speak(childrenAtMolokai+adultsAtMolokai);

                    // Fall asleep (probably forever).
                    waitAtMolokai.sleep();
                } else {
                    waitAtOahu.wakeAll();
                    waitAtOahu.sleep();
                }
            } else {  // An error occurs.
                Lib.assertTrue(false);
                break;
            }
        }

        boatLock.release();
    }

    static void SampleItinerary()
    {
        // Please note that this isn't a valid solution (you can't fit
        // all of them on the boat). Please also note that you may not
        // have a single thread calculate a solution and then just play
        // it back at the autograder -- you will be caught.
        System.out.println("\n ***Everyone piles on the boat and goes to Molokai***");
        bg.AdultRowToMolokai();
        bg.ChildRideToMolokai();
        bg.AdultRideToMolokai();
        bg.ChildRideToMolokai();
    }

}
