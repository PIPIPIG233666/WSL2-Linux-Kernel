/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright(c) 2007 - 2011 Realtek Corporation. */
#ifndef __HAL_VERSION_DEF_H__
#define __HAL_VERSION_DEF_H__

enum HAL_CHIP_TYPE {
	TEST_CHIP	=	0,
	NORMAL_CHIP	=	1,
};

enum HAL_CUT_VERSION {
	A_CUT_VERSION	=	0,
	B_CUT_VERSION	=	1,
	C_CUT_VERSION	=	2,
	D_CUT_VERSION	=	3,
	E_CUT_VERSION	=	4,
	F_CUT_VERSION	=	5,
	G_CUT_VERSION	=	6,
};

enum HAL_VENDOR {
	CHIP_VENDOR_TSMC	=	0,
	CHIP_VENDOR_UMC		=	1,
};

struct HAL_VERSION {
	enum HAL_CHIP_TYPE	ChipType;
	enum HAL_CUT_VERSION	CUTVersion;
	enum HAL_VENDOR		VendorType;
	u8			ROMVer;
};

/*  Get element */
#define GET_CVID_CHIP_TYPE(version)	(((version).ChipType))
#define GET_CVID_MANUFACTUER(version)	(((version).VendorType))
#define GET_CVID_CUT_VERSION(version)	(((version).CUTVersion))

/* Common Macro. -- */
/* HAL_VERSION VersionID */

/* HAL_CHIP_TYPE_E */
#define IS_NORMAL_CHIP(version)				\
	((GET_CVID_CHIP_TYPE(version) == NORMAL_CHIP) ? true : false)

/* HAL_CUT_VERSION_E */
#define IS_A_CUT(version)				\
	((GET_CVID_CUT_VERSION(version) == A_CUT_VERSION) ? true : false)
#define IS_B_CUT(version)				\
	((GET_CVID_CUT_VERSION(version) == B_CUT_VERSION) ? true : false)
#define IS_C_CUT(version)				\
	((GET_CVID_CUT_VERSION(version) == C_CUT_VERSION) ? true : false)
#define IS_D_CUT(version)				\
	((GET_CVID_CUT_VERSION(version) == D_CUT_VERSION) ? true : false)
#define IS_E_CUT(version)				\
	((GET_CVID_CUT_VERSION(version) == E_CUT_VERSION) ? true : false)

/* HAL_VENDOR_E */
#define IS_CHIP_VENDOR_TSMC(version)			\
	((GET_CVID_MANUFACTUER(version) == CHIP_VENDOR_TSMC) ? true : false)

#endif
