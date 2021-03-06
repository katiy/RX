#pragma once
//=====================================================================//
/*!	@file
	@brief	RX64M グループ・省電力制御
    @author 平松邦仁 (hira@rvf-rc45.net)
	@copyright	Copyright (C) 2016 Kunihito Hiramatsu @n
				Released under the MIT license @n
				https://github.com/hirakuni45/RX/blob/master/LICENSE
*/
//=====================================================================//
#include "RX64M/system.hpp"
#include "RX64M/peripheral.hpp"

namespace device {

	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	/*!
		@brief  省電力制御クラス
	*/
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	struct power_cfg {

		//-----------------------------------------------------------------//
		/*!
			@brief  周辺機器に切り替える
			@param[in]	t	周辺機器タイプ
			@param[in]	ena	オフにする場合「false」
		*/
		//-----------------------------------------------------------------//
		static void turn(peripheral t, bool ena = true)
		{
			bool f = !ena;
			switch(t) {
			case peripheral::CMT0:
			case peripheral::CMT1:
				SYSTEM::MSTPCRA.MSTPA15 = f;	// CMT0, CMT1 のストップ状態解除
				break;
			case peripheral::CMT2:
			case peripheral::CMT3:
				SYSTEM::MSTPCRA.MSTPA14 = f;	// CMT2, CMT3 のストップ状態解除
				break;

			case peripheral::S12AD:
				SYSTEM::MSTPCRA.MSTPA17 = f;	// S12AD のストップ状態解除
				break;
			case peripheral::S12AD1:
				SYSTEM::MSTPCRA.MSTPA16 = f;	// S12AD1 のストップ状態解除
				break;

			case peripheral::R12DA:
				SYSTEM::MSTPCRA.MSTPA19 = f;	// R12DA のストップ状態解除
				break;

			case peripheral::SCI0:
				SYSTEM::MSTPCRB.MSTPB31 = f;	// B31 (SCI0)のストップ状態解除
				break;
			case peripheral::SCI1:
				SYSTEM::MSTPCRB.MSTPB30 = f;	// B30 (SCI1)のストップ状態解除
				break;
			case peripheral::SCI2:
				SYSTEM::MSTPCRB.MSTPB29 = f;	// B29 (SCI2)のストップ状態解除
				break;
			case peripheral::SCI3:
				SYSTEM::MSTPCRB.MSTPB28 = f;	// B28 (SCI3)のストップ状態解除
				break;
			case peripheral::SCI4:
				SYSTEM::MSTPCRB.MSTPB27 = f;	// B27 (SCI4)のストップ状態解除
				break;
			case peripheral::SCI5:
				SYSTEM::MSTPCRB.MSTPB26 = f;	// B26 (SCI5)のストップ状態解除
				break;
			case peripheral::SCI6:
				SYSTEM::MSTPCRB.MSTPB25 = f;	// B25 (SCI6)のストップ状態解除
				break;
			case peripheral::SCI7:
				SYSTEM::MSTPCRB.MSTPB24 = f;	// B24 (SCI7)のストップ状態解除
				break;

			case peripheral::SCI12:
				SYSTEM::MSTPCRB.MSTPB4 = f;		// B4 (SCI12)のストップ状態解除
				break;

			case peripheral::RSPI:
				SYSTEM::MSTPCRB.MSTPB17 = f;	// RSPI のストップ状態解除
				break;

			case peripheral::SDHI:
				SYSTEM::MSTPCRD.MSTPD19 = f;	// SDHI のストップ状態解除
				break;

			case peripheral::ETHERC0:
			case peripheral::ETHERCA:
			case peripheral::EDMAC0:
				SYSTEM::MSTPCRB.MSTPB15 = f;	// ETHER0, EDMAC0 のストップ状態解除
				BUS::BEREN.TOEN = 1;
				break;
			case peripheral::ETHERC1:
			case peripheral::EDMAC1:
				SYSTEM::MSTPCRB.MSTPB14 = f;	// ETHER1, EDMAC1 のストップ状態解除
				BUS::BEREN.TOEN = 1;
				break;
			default:
				break;
			}
		}
	};
}
