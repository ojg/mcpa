/*
 * preferences.h
 *
 * Created: 02.12.2014 14:12:02
 *  Author: ole
 */ 


#ifndef PREFERENCES_H_
#define PREFERENCES_H_

struct Preferences_t {
    q13_2 vol_stepsize;
    int8_t vol_mutedB;
    int8_t vol_startup;
    int8_t vol_min;
    int8_t vol_max;
};

#endif /* PREFERENCES_H_ */