/*
    Project:    Canyon
    Purpose:    Get/set flag to determine whether executing an ISR
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#ifndef CANYON_ISRSTATE_H
#define CANYON_ISRSTATE_H 1

bool inISR();

void isrBegin();

void isrEnd();

#endif
