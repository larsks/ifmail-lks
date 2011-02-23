#ifndef SCANOUT_H
#define SCANOUT_H
#include "ftn.h"

#define OUT_PKT 0
#define OUT_DIR 1
#define OUT_FLO 2
#define OUT_ARC 3
#define OUT_REQ 4

extern int scanout(int (*)(faddr*,char,int,char*));

#endif
