#include "IPG_Lab2.h"

#if defined(_WIN32)
    #define KORAL_EXPORT extern "C" __declspec(dllexport)
#else
    #define KORAL_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// The engine runs the windowed path because this library exports CreateScene.
KORAL_EXPORT kor::Scene* CreateScene()
{
    return new IPG_Lab2();
}
