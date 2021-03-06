//=====================================================================//
/*! @file
    @brief  SEEDA03 (RX64M) メイン
	@copyright Copyright 2017 Kunihito Hiramatsu All Right Reserved.
    @author 平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "main.hpp"
#include "core.hpp"
#include "tools.hpp"
#include "nets.hpp"

namespace {

	static const bool wifi_enable_ = false;

	seeda::core 	core_;
	seeda::tools	tools_;
	seeda::nets		nets_;

	seeda::SPI		spi_;
	seeda::SDC		sdc_(spi_, 120000000);

	typedef utils::rtc_io RTC;
	RTC		rtc_;

	seeda::EADC		eadc_;

	uint32_t		sample_count_;
	seeda::sample	sample_[8];
	seeda::sample_data	sample_data_;

	uint16_t		signal_[8];

	volatile bool	enable_eadc_;

	void main_init_()
	{
		// RTC 開始
		rtc_.start();

		enable_eadc_ = false;

		{  // LTC2348ILX-16 初期化
			// 内臓リファレンスと内臓バッファ
			// VREFIN: 2.024V、VREFBUF: 4.096V、Analog range: 0V to 5.12V
			if(!eadc_.start(2000000, seeda::EADC::span_type::P5_12)) {
				utils::format("LTC2348_16 not found...\n");
			}
		}

		// 擬似波形発生、初期値
		uint16_t v = 0;
		for(int i = 0; i < 8; ++i) {
			signal_[i] = v;
			v += 0x2000;
		}
	}

	// 文字列表示、割り込み対応ロック／アンロック機構
	volatile bool putch_lock_ = false;
	utils::fixed_fifo<char, 1024> putch_tmp_;

	void service_putch_tmp_()
	{
		dis_int();
		while(putch_tmp_.length() > 0) {
			core_.sci_.putch(putch_tmp_.get());
		}
		ena_int();
	}


	uint8_t v_ = 91;
	uint8_t m_ = 123;
	uint8_t rand_()
	{
		v_ += v_ << 2;
		++v_;
		uint8_t n = 0;
		if(m_ & 0x02) n = 1;
		if(m_ & 0x40) n ^= 1;
		m_ += m_;
		if(n == 0) ++m_;
		return v_ ^ m_;
	}
}

namespace seeda {

	//-----------------------------------------------------------------//
	/*!
		@brief  プリファレンスの参照
		@return プリファレンス
	*/
	//-----------------------------------------------------------------//
	preference& at_pre() { return nets_.at_pre(); }


	//-----------------------------------------------------------------//
	/*!
		@brief  SDC_IO クラスへの参照
		@return SDC_IO クラス
	*/
	//-----------------------------------------------------------------//
	SDC& at_sdc() { return sdc_; }


	//-----------------------------------------------------------------//
	/*!
		@brief  EADC クラスへの参照
		@return EADC クラス
	*/
	//-----------------------------------------------------------------//
	EADC& at_eadc() { return eadc_; }


	//-----------------------------------------------------------------//
	/*!
		@brief  時間の作成
		@param[in]	date	日付
		@param[in]	time	時間
	*/
	//-----------------------------------------------------------------//
	size_t make_time(const char* date, const char* time)
	{
		time_t t = get_time();
		struct tm *m = localtime(&t);
		int vs[3];
		if((utils::input("%d/%d/%d", date) % vs[0] % vs[1] % vs[2]).status()) {
			if(vs[0] >= 1900 && vs[0] < 2100) m->tm_year = vs[0] - 1900;
			if(vs[1] >= 1 && vs[1] <= 12) m->tm_mon = vs[1] - 1;
			if(vs[2] >= 1 && vs[2] <= 31) m->tm_mday = vs[2];		
		} else {
			return 0;
		}

		if((utils::input("%d:%d:%d", time) % vs[0] % vs[1] % vs[2]).status()) {
			if(vs[0] >= 0 && vs[0] < 24) m->tm_hour = vs[0];
			if(vs[1] >= 0 && vs[1] < 60) m->tm_min = vs[1];
			if(vs[2] >= 0 && vs[2] < 60) m->tm_sec = vs[2];
		} else if((utils::input("%d:%d", time) % vs[0] % vs[1]).status()) {
			if(vs[0] >= 0 && vs[0] < 24) m->tm_hour = vs[0];
			if(vs[1] >= 0 && vs[1] < 60) m->tm_min = vs[1];
			m->tm_sec = 0;
		} else {
			return 0;
		}
		return mktime(m);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  GMT 時間の設定
		@param[in]	t	GMT 時間
	*/
	//-----------------------------------------------------------------//
	void set_time(time_t t)
	{
		if(!rtc_.set_time(t)) {
			utils::format("Stall RTC write...\n");
		}
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  時間の表示
		@param[in]	t		時間
		@param[in]	dst		出力文字列
		@param[in]	size	文字列の大きさ
		@return 生成された文字列の長さ
	*/
	//-----------------------------------------------------------------//
	int disp_time(time_t t, char* dst, uint32_t size)
	{
		struct tm *m = localtime(&t);
		auto n = (utils::sformat("%s %s %d %02d:%02d:%02d  %4d", dst, size)
			% get_wday(m->tm_wday)
			% get_mon(m->tm_mon)
			% static_cast<uint32_t>(m->tm_mday)
			% static_cast<uint32_t>(m->tm_hour)
			% static_cast<uint32_t>(m->tm_min)
			% static_cast<uint32_t>(m->tm_sec)
			% static_cast<uint32_t>(m->tm_year + 1900)).size();
		return n;
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  EADC サーバー
	*/
	//-----------------------------------------------------------------//
	void eadc_server()
	{
		if(!enable_eadc_) return;

#ifdef SEEDA
		if(nets_.get_dev_signal()) {
			for(int i = 0; i < 8; ++i) {
///				sample_[i].add(signal_[i]);
				// とりあえず、安全の為１４ビットにする
				sample_[i].add(signal_[i] & 0xfffc);
				++signal_[i];
			}
		} else {
			eadc_.convert();
			for(int i = 0; i < 8; ++i) {
///				sample_[i].add(eadc_.get_value(i));
				// とりあえず、安全の為１４ビットにする
				sample_[i].add(eadc_.get_value(i) & 0xfffc);
			}
		}
#endif
		++sample_count_;
		if(sample_count_ >= 1000) {  // 1sec
			for(int i = 0; i < 8; ++i) {
				sample_[i].collect();
				sample_data_.smp_[i] = sample_[i].get();
				sample_data_.smp_[i].ch_ = i;
				sample_[i].clear();
			}
			++sample_data_.time_;
			sample_count_ = 0;			
		}
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  EADC サーバー許可
		@param[in]	ena	「false」の場合不許可
	*/
	//-----------------------------------------------------------------//
	void enable_eadc_server(bool ena)
	{
		enable_eadc_ = ena;
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  A/D サンプルの参照
		@param[in]	ch	チャネル（０～７）
		@return A/D サンプル
	*/
	//-----------------------------------------------------------------//
	sample_t& at_sample(uint8_t ch)
	{
		return sample_[ch].at();
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  A/D サンプルの取得
		@return A/D サンプル
	*/
	//-----------------------------------------------------------------//
	const sample_data& get_sample_data()
	{
		return sample_data_;
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  内臓 A/D 変換値の取得
		@param[in]	ch	チャネル（５、６、７）
		@return A/D 変換値
	*/
	//-----------------------------------------------------------------//
	uint16_t get_adc(uint32_t ch)
	{
		return core_.get_adc(ch);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  ファイル作成テスト
		@param[in]	fname	ファイル名
		@param[in]	size	作成サイズ
		@return 成功なら「true」
	*/
	//-----------------------------------------------------------------//
	bool create_test_file(const char* fname, uint32_t size, sd_speed_t& t)
	{
		uint8_t buff[512];
		FIL fp;

		for(uint16_t i = 0; i < sizeof(buff); ++i) {
			buff[i] = rand_();
		}

		auto st = core_.get_cmt_counter();
		if(!sdc_.open(&fp, fname, FA_WRITE | FA_CREATE_ALWAYS)) {
			utils::format("Can't create file: '%s'\n") % fname;
			return false;
		}

		auto ed = core_.get_cmt_counter();
		t.open = ed - st;
		st = ed;

		auto rs = size;
		while(rs > 0) {
			UINT sz = sizeof(buff);
			if(sz > rs) sz = rs;
			UINT bw;
			f_write(&fp, buff, sz, &bw);
			rs -= bw;
		}
		ed = core_.get_cmt_counter();
		t.write = ed - st;
		st = ed;

		f_close(&fp);
		ed = core_.get_cmt_counter();
		t.close = ed - st;

//		auto ed = core_.get_cmt_counter();
//		uint32_t time = ed - st;
//		utils::format("Write frame: %d\n") % len;
//		auto pbyte = size * 1000 / time;
//		utils::format("Write: %d Bytes/Sec\n") % pbyte;
//		utils::format("Write: %d KBytes/Sec\n") % (pbyte / 1024);

		return true;
	}
}

extern "C" {

	//-----------------------------------------------------------------//
	/*!
		@brief  システム・文字出力
		@param[in]	ch	文字
	*/
	//-----------------------------------------------------------------//
	void sci_putch(char ch)
	{
		if(putch_lock_) {
			if((putch_tmp_.size() - putch_tmp_.length()) >= 2) {
				putch_tmp_.put(ch);
			}
			return;
		}
		putch_lock_ = true;
		core_.sci_.putch(ch);
		putch_lock_ = false;
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  システム・文字列出力
		@param[in]	s	文字列
	*/
	//-----------------------------------------------------------------//
	void sci_puts(const char* s)
	{
		core_.sci_.puts(s);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  システム・文字入力
		@return	文字
	*/
	//-----------------------------------------------------------------//
	char sci_getch(void)
	{
		return core_.sci_.getch();
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  システム・文字列長の取得
		@return	文字列長
	*/
	//-----------------------------------------------------------------//
	uint16_t sci_length(void)
	{
		return core_.sci_.recv_length();
	}


	//-----------------------------------------------------------------//
	/*!
		@brief	FatFs へ初期化関数を提供
		@param[in]	drv		Physical drive nmuber (0)
		@return ステータス
	 */
	//-----------------------------------------------------------------//
	DSTATUS disk_initialize(BYTE drv) {
		return sdc_.at_mmc().disk_initialize(drv);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief	FatFs へステータスを提供
		@param[in]	drv		Physical drive nmuber (0)
	 */
	//-----------------------------------------------------------------//
	DSTATUS disk_status(BYTE drv) {
		return sdc_.at_mmc().disk_status(drv);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief	FatFs へリード・セクターを提供
		@param[in]	drv		Physical drive nmuber (0)
		@param[out]	buff	Pointer to the data buffer to store read data
		@param[in]	sector	Start sector number (LBA)
		@param[in]	count	Sector count (1..128)
		@return リザルト
	 */
	//-----------------------------------------------------------------//
	DRESULT disk_read(BYTE drv, BYTE* buff, DWORD sector, UINT count) {
		return sdc_.at_mmc().disk_read(drv, buff, sector, count);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief	FatFs へライト・セクターを提供
		@param[in]	drv		Physical drive nmuber (0)
		@param[in]	buff	Pointer to the data to be written	
		@param[in]	sector	Start sector number (LBA)
		@param[in]	count	Sector count (1..128)
		@return リザルト
	 */
	//-----------------------------------------------------------------//
	DRESULT disk_write(BYTE drv, const BYTE* buff, DWORD sector, UINT count) {
		return sdc_.at_mmc().disk_write(drv, buff, sector, count);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief	FatFs へI/O コントロールを提供
		@param[in]	drv		Physical drive nmuber (0)
		@param[in]	ctrl	Control code
		@param[in]	buff	Buffer to send/receive control data
		@return リザルト
	 */
	//-----------------------------------------------------------------//
	DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void* buff) {
		return sdc_.at_mmc().disk_ioctl(drv, ctrl, buff);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief	FatFs へ時間を提供
		@return FatFs 時間
	 */
	//-----------------------------------------------------------------//
	DWORD get_fattime(void) {
		time_t t = get_time();
		return utils::str::get_fattime(t);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief	UTF-8 から ShiftJIS への変換
		@param[in]	src	UTF-8 文字列ソース
		@param[out]	dst	ShiftJIS 文字列出力
	 */
	//-----------------------------------------------------------------//
	void utf8_to_sjis(const char* src, char* dst) {
		utils::str::utf8_to_sjis(src, dst);
	}


	//-----------------------------------------------------------------//
	/*!
		@brief  GMT 時間の取得
		@return GMT 時間
	*/
	//-----------------------------------------------------------------//
	time_t get_time()
	{
		time_t t = 0;
		rtc_.get_time(t);
		return t;
	}


	unsigned long millis(void)
	{
		return core_.cmt0_.at_task().get_millis();
	}


	void delay(unsigned long ms)
	{
		core_.cmt0_.at_task().set_delay(ms);
		while(core_.cmt0_.at_task().get_delay() != 0) ;		
	}


	void set_task_10ms(void (*task)(void)) {
		core_.cmt0_.at_task().set_task_10ms(task);
	}
}

int main(int argc, char** argv);

int main(int argc, char** argv)
{
	using namespace seeda;

	// ポート初期化
	// 設定を除外するポート
//  DIP-SWITCH: P66, P67
//	Ethernet:   PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7, P71, P72
//  PHY:        P70, P73
//  LTC2348-16: P40, PC6, PD0, P53, P56, P86, P87, P20, P21, P22, P23
//  SDC:        PD3, PD4, PD5, PD6, PE6
//  EUI-SPI:    P14, P15, P16, P17, P80
//  RX64M A/D:
	device::PORT0::PDR.B5 = 1;  // (2)
	device::PORT0::PDR.B3 = 1;  // (4)
	device::PORT0::PDR.B2 = 1;  // (6)
	device::PORT0::PDR.B1 = 1;  // (7)
	device::PORT0::PDR.B0 = 1;  // (8)
	device::PORTJ::PDR.B5 = 1;  // (11)
	device::PORTJ::PDR.B3 = 1;  // (13)
	device::PORT3::PDR.B3 = 1;  // (26)
	device::PORT3::PDR.B2 = 1;  // (27)
	device::PORT2::PDR.B5 = 1;  // (32)
	device::PORT2::PDR.B4 = 1;  // (33)
	device::PORT1::PDR.B3 = 1;  // (44)
	device::PORT1::PDR.B2 = 1;  // (45)
	device::PORT5::PDR.B5 = 1;  // (51)
	device::PORT5::PDR.B4 = 1;  // (52)
	if(wifi_enable_) {
		device::PORT5::PDR.B2 = 1;  // (54) WIFI Module
		device::PORT5::PDR.B1 = 1;  // (55) WIFI Module
		device::PORT5::PDR.B0 = 1;  // (56) WIFI Module
	}
	device::PORT8::PDR.B3 = 1;  // (58)
	device::PORTC::PDR.B5 = 1;  // (62)
	device::PORT8::PDR.B2 = 1;  // (63)
	device::PORT8::PDR.B1 = 1;  // (64)
	device::PORTC::PDR.B4 = 1;  // (66)
	device::PORTC::PDR.B3 = 1;  // (67)
	if(wifi_enable_) {
		device::PORT7::PDR.B7 = 1;  // (68) WIFI Module
		device::PORT7::PDR.B6 = 1;  // (69) WIFI Module
	}
	device::PORTC::PDR.B2 = 1;  // (70)
	if(wifi_enable_) {
		device::PORT7::PDR.B5 = 1;  // (71) WIFI Module
		device::PORT7::PDR.B4 = 1;  // (72) WIFI Module
	}
	device::PORTC::PDR.B1 = 1;  // (73)
	device::PORTC::PDR.B0 = 1;  // (75)
	device::PORT6::PDR.B5 = 1;  // (100)
	device::PORTE::PDR.B5 = 1;  // (106)
	device::PORTE::PDR.B4 = 1;  // (107)
	device::PORTE::PDR.B2 = 1;  // (109)
	device::PORTE::PDR.B1 = 1;  // (110)
	device::PORTE::PDR.B0 = 1;  // (111)
	device::PORT6::PDR.B4 = 1;  // (112)
	device::PORT6::PDR.B3 = 1;  // (113)
	device::PORT6::PDR.B2 = 1;  // (114)
	device::PORT6::PDR.B1 = 1;  // (115)
	device::PORT6::PDR.B0 = 1;  // (117)
	device::PORTD::PDR.B1 = 1;  // (125)
	device::PORT9::PDR.B3 = 1;  // (127)
	device::PORT9::PDR.B2 = 1;  // (128)
	device::PORT9::PDR.B1 = 1;  // (129)
	device::PORT9::PDR.B0 = 1;  // (131)
	device::PORT0::PDR.B7 = 1;  // (144)

	device::PORTE::PDR.B3 = 1;  // LED output

	device::PORT3::PCR.B5 = 1; // P35(NMI) pull-up

	device::SYSTEM::PRCR = 0xA50B;	// クロック、低消費電力、関係書き込み許可

	device::SYSTEM::MOSCWTCR = 9;	// 1ms wait
	// メインクロック強制発振とドライブ能力設定
	device::SYSTEM::MOFCR = device::SYSTEM::MOFCR.MODRV2.b(0b10)
						  | device::SYSTEM::MOFCR.MOFXIN.b(1);
	device::SYSTEM::MOSCCR.MOSTP = 0;		// メインクロック発振器動作
	while(device::SYSTEM::OSCOVFSR.MOOVF() == 0) asm("nop");

	// Base Clock 12.5MHz
	// PLLDIV: 1/1, STC: 19 倍(237.5MHz)
	device::SYSTEM::PLLCR = device::SYSTEM::PLLCR.PLIDIV.b(0) |
							device::SYSTEM::PLLCR.STC.b(0b100101);
	device::SYSTEM::PLLCR2.PLLEN = 0;			// PLL 動作
	while(device::SYSTEM::OSCOVFSR.PLOVF() == 0) asm("nop");

	device::SYSTEM::SCKCR = device::SYSTEM::SCKCR.FCK.b(2)		// 1/2 (237.5/4=59.375)
						  | device::SYSTEM::SCKCR.ICK.b(1)		// 1/2 (237.5/2=118.75)
						  | device::SYSTEM::SCKCR.BCK.b(2)		// 1/2 (237.5/4=59.375)
						  | device::SYSTEM::SCKCR.PCKA.b(1)		// 1/2 (237.5/2=118.75)
						  | device::SYSTEM::SCKCR.PCKB.b(2)		// 1/4 (237.5/4=59.375)
						  | device::SYSTEM::SCKCR.PCKC.b(2)		// 1/4 (237.5/4=59.375)
						  | device::SYSTEM::SCKCR.PCKD.b(2);	// 1/4 (237.5/4=59.375)
	device::SYSTEM::SCKCR2 = device::SYSTEM::SCKCR2.UCK.b(0b0100) | 1;  // USB Clock: 1/5 (237.5/5=47.5)
	device::SYSTEM::SCKCR3.CKSEL = 0b100;	///< PLL 選択

	main_init_();

	sample_count_ = 0;

	core_.init();

	// SD カード・クラスの初期化
	sdc_.start();

	// 設定ファイルの確認
///	config_ = sdc_.probe("seeda.cfg");
	core_.title();

	tools_.init();
	tools_.title();

	nets_.init();
	nets_.title();

	enable_eadc_server();

	uint32_t cnt = 0;
	while(1) {
		core_.sync();

		service_putch_tmp_();

		core_.service();
		tools_.service();

		sdc_.service();

		nets_.service();

		++cnt;
		if(cnt >= 30) {
			cnt = 0;
		}
		device::PORTE::PODR.B3 = (cnt < 10) ? 0 : 1;
	}
}
