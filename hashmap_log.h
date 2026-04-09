#pragma once

#include <stdio.h>

#define CB_LOG_ERROR(message) fprintf(stderr, "%s: %s\n", __func__, message)

