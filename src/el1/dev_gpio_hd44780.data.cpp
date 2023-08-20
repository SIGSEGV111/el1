#include "dev_gpio_hd44780.hpp"

namespace el1::dev::gpio::hd44780
{
	struct char_map_t
	{
		TUTF32 chr;
		u8_t dram;
	};

	static const char_map_t CHAR_MAP[] = {
		{ 92U, 164U },
		{ 162U, 236U },
		{ 163U, 237U },
		{ 165U, 92U },
		{ 181U, 228U },
		{ 183U, 165U },
		{ 223U, 226U },
		{ 228U, 225U },
		{ 241U, 238U },
		{ 246U, 239U },
		{ 247U, 253U },
		{ 913U, 65U },
		{ 914U, 66U },
		{ 917U, 69U },
		{ 918U, 90U },
		{ 919U, 72U },
		{ 920U, 242U },
		{ 921U, 73U },
		{ 922U, 75U },
		{ 924U, 77U },
		{ 925U, 78U },
		{ 927U, 79U },
		{ 929U, 80U },
		{ 931U, 246U },
		{ 932U, 84U },
		{ 933U, 89U },
		{ 935U, 88U },
		{ 937U, 244U },
		{ 945U, 224U },
		{ 946U, 226U },
		{ 949U, 227U },
		{ 952U, 242U },
		{ 956U, 228U },
		{ 959U, 111U },
		{ 960U, 247U },
		{ 961U, 230U },
		{ 963U, 229U },
		{ 8592U, 127U },
		{ 8594U, 126U },
		{ 8730U, 232U },
		{ 8734U, 243U },
		{ 9082U, 224U },
		{ 12289U, 164U },
		{ 12290U, 161U },
		{ 12300U, 162U },
		{ 12301U, 163U },
		{ 12441U, 222U },
		{ 12442U, 223U },
		{ 12443U, 222U },
		{ 12444U, 223U },
		{ 12448U, 61U },
		{ 12449U, 167U },
		{ 12450U, 177U },
		{ 12451U, 168U },
		{ 12452U, 178U },
		{ 12453U, 169U },
		{ 12454U, 179U },
		{ 12455U, 170U },
		{ 12456U, 180U },
		{ 12457U, 171U },
		{ 12458U, 181U },
		{ 12459U, 182U },
		{ 12460U, 182U },
		{ 12461U, 183U },
		{ 12462U, 183U },
		{ 12463U, 184U },
		{ 12464U, 184U },
		{ 12465U, 185U },
		{ 12466U, 185U },
		{ 12467U, 186U },
		{ 12468U, 186U },
		{ 12469U, 187U },
		{ 12470U, 187U },
		{ 12471U, 188U },
		{ 12472U, 188U },
		{ 12473U, 189U },
		{ 12474U, 189U },
		{ 12475U, 190U },
		{ 12476U, 190U },
		{ 12477U, 191U },
		{ 12478U, 191U },
		{ 12479U, 192U },
		{ 12480U, 192U },
		{ 12481U, 193U },
		{ 12482U, 193U },
		{ 12483U, 175U },
		{ 12484U, 194U },
		{ 12485U, 194U },
		{ 12486U, 195U },
		{ 12487U, 195U },
		{ 12488U, 196U },
		{ 12489U, 196U },
		{ 12490U, 197U },
		{ 12491U, 198U },
		{ 12492U, 199U },
		{ 12493U, 200U },
		{ 12494U, 201U },
		{ 12495U, 202U },
		{ 12496U, 202U },
		{ 12497U, 202U },
		{ 12498U, 203U },
		{ 12499U, 203U },
		{ 12500U, 203U },
		{ 12501U, 204U },
		{ 12502U, 204U },
		{ 12503U, 204U },
		{ 12504U, 205U },
		{ 12505U, 205U },
		{ 12506U, 205U },
		{ 12507U, 206U },
		{ 12508U, 206U },
		{ 12509U, 206U },
		{ 12510U, 207U },
		{ 12511U, 208U },
		{ 12512U, 209U },
		{ 12513U, 210U },
		{ 12514U, 211U },
		{ 12515U, 172U },
		{ 12516U, 212U },
		{ 12517U, 173U },
		{ 12518U, 213U },
		{ 12519U, 174U },
		{ 12520U, 214U },
		{ 12521U, 215U },
		{ 12522U, 216U },
		{ 12523U, 217U },
		{ 12524U, 218U },
		{ 12525U, 219U },
		{ 12526U, 220U },
		{ 12527U, 220U },
		{ 12528U, 178U },
		{ 12529U, 180U },
		{ 12530U, 166U },
		{ 12531U, 221U },
		{ 12532U, 179U },
		{ 12533U, 182U },
		{ 12534U, 185U },
		{ 12535U, 220U },
		{ 12536U, 178U },
		{ 12537U, 180U },
		{ 12538U, 166U },
		{ 12539U, 165U },
		{ 12540U, 176U },
		{ 12541U, 164U },
		{ 12542U, 164U },
		{ 12543U, 186U },
		{ 20870U, 252U },
		{ 65377U, 161U },
		{ 65378U, 162U },
		{ 65379U, 163U },
		{ 65380U, 164U },
		{ 65381U, 165U },
		{ 65382U, 166U },
		{ 65383U, 167U },
		{ 65384U, 168U },
		{ 65385U, 169U },
		{ 65386U, 170U },
		{ 65387U, 171U },
		{ 65388U, 172U },
		{ 65389U, 173U },
		{ 65390U, 174U },
		{ 65391U, 175U },
		{ 65392U, 176U },
		{ 65393U, 177U },
		{ 65394U, 178U },
		{ 65395U, 179U },
		{ 65396U, 180U },
		{ 65397U, 181U },
		{ 65398U, 182U },
		{ 65399U, 183U },
		{ 65400U, 184U },
		{ 65401U, 185U },
		{ 65402U, 186U },
		{ 65403U, 187U },
		{ 65404U, 188U },
		{ 65405U, 189U },
		{ 65406U, 190U },
		{ 65407U, 191U },
		{ 65408U, 192U },
		{ 65409U, 193U },
		{ 65410U, 194U },
		{ 65411U, 195U },
		{ 65412U, 196U },
		{ 65413U, 197U },
		{ 65414U, 198U },
		{ 65415U, 199U },
		{ 65416U, 200U },
		{ 65417U, 201U },
		{ 65418U, 202U },
		{ 65419U, 203U },
		{ 65420U, 204U },
		{ 65421U, 205U },
		{ 65422U, 206U },
		{ 65423U, 207U },
		{ 65424U, 208U },
		{ 65425U, 209U },
		{ 65426U, 210U },
		{ 65427U, 211U },
		{ 65428U, 212U },
		{ 65429U, 213U },
		{ 65430U, 214U },
		{ 65431U, 215U },
		{ 65432U, 216U },
		{ 65433U, 217U },
		{ 65434U, 218U },
		{ 65435U, 219U },
		{ 65436U, 220U },
		{ 65437U, 221U },
		{ 65438U, 222U },
		{ 65439U, 223U },
		{ 65509U, 92U },
	};

	static const unsigned N_CHAR_MAP = sizeof(CHAR_MAP) / sizeof(CHAR_MAP[0]);

	u8_t THD44780::TranslateCharToDram(const TUTF32 chr)
	{
		for(unsigned i = 0; i < N_CHAR_MAP; i++)
			if(CHAR_MAP[i].chr == chr)
				return CHAR_MAP[i].dram;

		if(chr.code <= 127)
			return (u8_t)chr.code;

		EL_THROW(TException, TString::Format("character '%c' (UTF+%x) is not included in the LCDs fontset", chr, chr.code));
	}

	TUTF32 THD44780::TranslateDramToChar(const u8_t dram)
	{
		for(unsigned i = 0; i < N_CHAR_MAP; i++)
			if(CHAR_MAP[i].dram == dram)
				return CHAR_MAP[i].chr;

		if(dram <= 127)
			return TUTF32((u32_t)dram);

		EL_THROW(TException, TString::Format("dram value %xh is not mapped to any unicode character", dram));
	}
}
