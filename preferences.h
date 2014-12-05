/*
 * preferences.h
 *
 * Created: 02.12.2014 14:12:02
 *  Author: ole
 */ 


#ifndef PREFERENCES_H_
#define PREFERENCES_H_

#include <inttypes.h>

typedef int16_t q13_2;

struct Preferences_t {
    q13_2 vol_stepsize;
    int8_t vol_mutedB;
    int8_t vol_startup;
    int8_t vol_min;
    int8_t vol_max;
};

struct Preferences_t * get_preferences(void);

#define Q13_2_TO_FLOAT(x) (((float)(x)) * 0.25f)
#define FLOAT_TO_Q13_2(x) ((q13_2)((x) * 4.0f))

#endif /* PREFERENCES_H_ */