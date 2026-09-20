#ifndef TIMER_PLATFORM_H
#define TIMER_PLATFORM_H

#include <stdbool.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct
{
    LARGE_INTEGER frequency;
} monotonic_clock_t;

typedef struct
{
    LARGE_INTEGER value;
} monotonic_time_t;

static inline bool monotonicClockInit(
    monotonic_clock_t *clock)
{
    if (clock == NULL)
    {
        return false;
    }

    return QueryPerformanceFrequency(
        &clock->frequency
    ) != 0;
}

static inline bool monotonicTimeNow(
    monotonic_time_t *timestamp)
{
    if (timestamp == NULL)
    {
        return false;
    }

    return QueryPerformanceCounter(
        &timestamp->value
    ) != 0;
}

static inline double monotonicElapsedMilliseconds(
    const monotonic_clock_t *clock,
    monotonic_time_t start,
    monotonic_time_t end)
{
    if (clock == NULL ||
        clock->frequency.QuadPart == 0)
    {
        return 0.0;
    }

    return
        ((double)(
            end.value.QuadPart -
            start.value.QuadPart
        ) * 1000.0) /
        (double)clock->frequency.QuadPart;
}

#else

#include <time.h>

typedef struct
{
    int initialized;
} monotonic_clock_t;

typedef struct
{
    struct timespec value;
} monotonic_time_t;

static inline bool monotonicClockInit(
    monotonic_clock_t *clock)
{
    if (clock == NULL)
    {
        return false;
    }

    clock->initialized = 1;

    return true;
}

static inline bool monotonicTimeNow(
    monotonic_time_t *timestamp)
{
    if (timestamp == NULL)
    {
        return false;
    }

    return clock_gettime(
        CLOCK_MONOTONIC,
        &timestamp->value
    ) == 0;
}

static inline double monotonicElapsedMilliseconds(
    const monotonic_clock_t *clock,
    monotonic_time_t start,
    monotonic_time_t end)
{
    if (clock == NULL ||
        !clock->initialized)
    {
        return 0.0;
    }

    time_t seconds =
        end.value.tv_sec -
        start.value.tv_sec;

    long nanoseconds =
        end.value.tv_nsec -
        start.value.tv_nsec;

    return
        (double)seconds * 1000.0 +
        (double)nanoseconds / 1000000.0;
}

#endif

#endif