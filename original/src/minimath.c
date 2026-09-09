unsigned short icnt32k = 32767;
int	holdrand = 0;	//0x12345678;


float fsin(float inval) {
	_asm {
		fld		dword ptr [inval]
		fsin
	}
}

float fcos(float inval) {
	_asm {
		fld		dword ptr [inval]
		fcos
	}
}

int float2int (float inval){
	_asm {
		fld		dword ptr [inval]
		push	eax
		fistp	dword ptr [esp]
		pop		eax
	}
	//return tm_iret;
}

float fsqrt (float inval){
	_asm {
		fld		dword ptr [inval]
		fsqrt
	}
}

float myRandFloat() {
	_asm {
		mov	eax, [holdrand]
		imul	eax, 214013
		add	eax, 2531011
		mov	[holdrand], eax
		push	ax
		fild	word ptr [esp]
		pop	ax
		fidiv	word ptr [icnt32k]
	}
}