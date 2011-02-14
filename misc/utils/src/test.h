#ifndef __TEST_h__
#define __TEST_h__

#include "msgsock.h"

struct test_msg
{
    int name;
    double value;
};

struct big_test_msg
{
    char pad[8192];
};

struct point_msg
{
    float x;
    float y;
};

struct kv_msg
{
    int32_t key;
    double value;
};

enum
{
    TAG_POINT_MSG = 0,
    TAG_KV_MSG
};

#endif
