/*
    Project:    Canyon
    Purpose:    Get/set flag to determine whether executing an ISR
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

volatile bool g_inISR = false;

bool inISR()
{
    return g_inISR;
}

void isrBegin()
{
    g_inISR = true;
}

void isrEnd()
{
    g_inISR = false;
}
