#ifndef DRIVER_H
#define DRIVER_H

// This package provides higher abstration regarding driving the vehicle in ceertain directions and distances

class Driver
{

private:
    static void driverService(void);

public:
    Driver();

    void Begin();

    void drive_absolute(int direction, int distance, int speed); // direction is the direction since start in degrees....will drift. Distance measured in mm (is relative)
                                                                 // will return true if successful. Failure can be caused by the direction was not achieved or distance was not achieved

    void drive_relative(int turn, int distance, int speed); // turn is relative in degrees, distance is relative in mm
                                                            // will return true if successful. Failure can be caused by the turn was not achieved or distance was not achieved
};

#endif