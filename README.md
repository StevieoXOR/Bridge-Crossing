# Bridge-Crossing
Simulates a 1-lane bridge that can support 1 semi-truck or up to 3 cars using C Posix pthreads.


Program *will* create Segmentation Faults due to Memory Leaks. Also, it may occasionally get stuck due to an imbalance of pthread_cond_signal() and pthread_join() (I may have fixed that issue, but I don't know for sure). However, the program seems to mostly work.

Note that my pseudocode does not match my implementation due to an incorrect implementation stemming from pseudocode that is too vague and/or not fully inclusive of all scenarios.


Enforced Bridge Restrictions
* R1. Since the bridge has only one lane, the traffic may cross the bridge in one direction at a time.
* R2. Due to construction constraints, the bridge can accommodate at most 3 cars at a time.
* R3. While a truck is crossing the bridge, no other vehicle (car or truck) should be on the bridge.

Trucks have absolute priority over cars. I.e., no car should be allowed to start crossing the bridge if there is any waiting truck at either end of the bridge. In case there are waiting trucks in both directions, then the traffic direction must be switched after each truck to provide a fair waiting time to trucks in each direction.

If there are no waiting trucks, then the cars (if any) can start crossing the bridge. If there are waiting cars at both ends of the bridge, then the direction of the first car to cross the bridge is randomly chosen (happens initially, or following a group of trucks). However, once a car C starts crossing the bridge in one direction, the cars heading in the same direction as C will have priority over any car that may be waiting/arriving at the other end of the bridge. "Trucks have absolute priority over cars" will be effective again as soon as a southbound or northbound truck arrives, immediately stopping more cars from entering the bridge, but letting cars already on the bridge exit the bridge. Before allowing any truck, the program must wait until all the cars that are already on the bridge have left. Otherwise the bridge will collapse.

If there is sufficient traffic, the bridge capacity must be fully used. In other words, having cars cross the bridge one by one is unacceptable.

When multiple vehicles arrive simultaneously, no vehicle in a group is allowed to cross the bridge until all vehicles in the same group arrive.

When multiple vehicles arrive simultaneously, the direction and type of each vehicle will be determined randomly.

Each vehicle is represented by one pthread.

This program does not employ busy-waiting, it uses pthread_cond_signal() to alter a condition variable. Once the signal is > 0, a mutex lock is secured for an individual vehicle (a single thread gets to cross the bridge).

Vehicles do not necessarily leave the bridge in the same order they entered: a vehicle may overtake another vehicle traveling in the same direction (only applies to cars since only one truck can cross the bridge at a time.

Each crossing takes two seconds (+ computation time).
