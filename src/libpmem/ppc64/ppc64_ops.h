#ifndef __PPC64__OPS__H__

#define SPRN_PVR	0x11F	/* Processor Version Register */
#define SPRN_PIR	0x3FF	/* Processor Identification Register */

#define __stringify_1(x)	#x
#define __stringify(x)		__stringify_1(x)

#define mfspr(rn)	({unsigned long rval; 				\
			asm volatile("mfspr %0," __stringify(rn) 	\
				: "=r" (rval)); rval; })

#define mtspr(rn, v)	asm volatile("mtspr " __stringify(rn) ",%0" : 	\
					: "r" ((unsigned long)(v)) 	\
					: "memory")

/* Read the processor PVR register */
#define mfpvr() 	mfspr(SPRN_PVR)

/* Processor Version Register (PVR) field extraction */
#define PVR_VER(pvr)	(((pvr) >>  16) & 0xFFFF)	/* Version field */
#define PVR_REV(pvr)	(((pvr) >>   0) & 0xFFFF)	/* Revison field */

#define pvr_version_is(pvr)	(PVR_VER(mfpvr()) == (pvr))

/* Processor Version Numbers */
#define PVR_403GA	0x00200000
#define PVR_403GB	0x00200100
#define PVR_403GC	0x00200200
#define PVR_403GCX	0x00201400
#define PVR_405GP	0x40110000
#define PVR_476		0x11a52000
#define PVR_476FPE	0x7ff50000
#define PVR_STB03XXX	0x40310000
#define PVR_NP405H	0x41410000
#define PVR_NP405L	0x41610000
#define PVR_601		0x00010000
#define PVR_602		0x00050000
#define PVR_603		0x00030000
#define PVR_603e	0x00060000
#define PVR_603ev	0x00070000
#define PVR_603r	0x00071000
#define PVR_604		0x00040000
#define PVR_604e	0x00090000
#define PVR_604r	0x000A0000
#define PVR_620		0x00140000
#define PVR_740		0x00080000
#define PVR_750		PVR_740
#define PVR_740P	0x10080000
#define PVR_750P	PVR_740P
#define PVR_7400	0x000C0000
#define PVR_7410	0x800C0000
#define PVR_7450	0x80000000
#define PVR_8540	0x80200000
#define PVR_8560	0x80200000
#define PVR_VER_E500V1	0x8020
#define PVR_VER_E500V2	0x8021
#define PVR_VER_E500MC	0x8023
#define PVR_VER_E5500	0x8024
#define PVR_VER_E6500	0x8040

/*
 * For the 8xx processors, all of them report the same PVR family for
 * the PowerPC core. The various versions of these processors must be
 * differentiated by the version number in the Communication Processor
 * Module (CPM).
 */
#define PVR_8xx		0x00500000

#define PVR_8240	0x00810100
#define PVR_8245	0x80811014
#define PVR_8260	PVR_8240

/* 476 Simulator seems to currently have the PVR of the 602... */
#define PVR_476_ISS	0x00052000

/* 64-bit processors */
#define PVR_NORTHSTAR	0x0033
#define PVR_PULSAR	0x0034
#define PVR_POWER4	0x0035
#define PVR_ICESTAR	0x0036
#define PVR_SSTAR	0x0037
#define PVR_POWER4p	0x0038
#define PVR_970		0x0039
#define PVR_POWER5	0x003A
#define PVR_POWER5p	0x003B
#define PVR_970FX	0x003C
#define PVR_POWER6	0x003E
#define PVR_POWER7	0x003F
#define PVR_630		0x0040
#define PVR_630p	0x0041
#define PVR_970MP	0x0044
#define PVR_970GX	0x0045
#define PVR_POWER7p	0x004A
#define PVR_POWER8E	0x004B
#define PVR_POWER8NVL	0x004C
#define PVR_POWER8	0x004D
#define PVR_POWER9	0x004E
#define PVR_BE		0x0070
#define PVR_PA6T	0x0090

#endif /* __PPC64__OPS__H__ */
