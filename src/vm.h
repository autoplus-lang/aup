#ifndef _AUP_VM_H
#define _AUP_VM_H
#pragma once

#include "compiler.h"



struct _aupVM {
	uint8_t *ip;
	aupCh *chunk;

	aupV stack[];

};

#endif
