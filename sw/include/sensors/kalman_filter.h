#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H

class KalmanFilter {
public:
    KalmanFilter(float processNoise, float measurementNoise, float estimationError, float initialValue);
    float updateEstimate(float measurement);

private:
    float processNoise;
    float measurementNoise;
    float estimationError;
    float currentEstimate;
    float lastEstimate;
    float kalmanGain;
};

#endif // KALMAN_FILTER_H