#ifndef __DIGITAL_TUBE_TASK_H__
#define __DIGITAL_TUBE_TASK_H__

#include "main.h"
#include "spi.h"
#include "port_digitaltube.h"

extern DigitalTube_t DigitalTube;

void DigitalTubeTask_Init(void);
void DigitalTube_Task(void);

#endif
