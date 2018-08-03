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
