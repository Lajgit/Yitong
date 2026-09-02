#ifndef __LIGHTTASK_H__
#define __LIGHTTASK_H__

#include "port_lighteffect.h"
#include "port_light.h"

#define Light1_RGBbuffer_SIZE 26
#define Light1_CRRbuffer_SIZE ((Light1_RGBbuffer_SIZE + 7) * 24)

/* 中文注释：控台第二路原理图实际为LD27~LD74，共48颗WS2812。 */
#define Light2_RGBbuffer_SIZE 48
#define Light2_CRRbuffer_SIZE ((Light2_RGBbuffer_SIZE + 7) * 24)

#define LightTime 300

void Light_Init(void);
void Light_Task(void);

#endif
