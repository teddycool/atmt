#include "sensors/kalman_filter.h"
#include <cmath>

KalmanFilter::KalmanFilter(float processNoise, float measurementNoise, float estimationError, float initialValue) {
    this->processNoise = processNoise;
    this->measurementNoise = measurementNoise;
    this->estimationError = estimationError;
    this->currentEstimate = initialValue;
    this->lastEstimate = initialValue;
}

float KalmanFilter::updateEstimate(float measurement) {
    kalmanGain = estimationError / (estimationError + measurementNoise);
    currentEstimate = lastEstimate + kalmanGain * (measurement - lastEstimate);
    estimationError = (1.0 - kalmanGain) * estimationError + fabs(lastEstimate - currentEstimate) * processNoise;
    lastEstimate = currentEstimate;

    return currentEstimate;
}