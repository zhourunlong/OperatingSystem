#ifndef cleaner_h
#define cleaner_h

void collect_garbage(bool clean_thoroughly);

/* Structs to record utilization and timestamps of each segment. */
struct util_entry {
    int segment_number;
    int count;
};
struct time_entry {
    int segment_number;
    int update_sec;
    int update_nsec;
};

#endif