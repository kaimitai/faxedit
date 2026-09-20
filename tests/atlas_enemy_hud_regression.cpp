#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	constexpr word ORG{0xfcce};
	void require(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
	std::size_t off(byte bank, word cpu) { return klib::Asm6502::get_file_offset(bank,cpu); }
	void put(std::vector<byte>& rom, byte bank, word cpu, std::initializer_list<byte> bytes) {
		auto at{off(bank,cpu)}; for (byte b : bytes) rom.at(at++)=b;
	}
	std::vector<byte> fixture() {
		std::vector<byte> rom(0x40010,0xff);
		const byte header[]{'N','E','S',0x1a,16,0,0x10,0,0,0,0,0,0,0,0,0};
		for (int i{0};i<16;++i) rom[i]=header[i];
		put(rom,14,0x8897,{0x9d,0x44,3}); put(rom,14,0x8203,{0x9d,0x44,3});
		put(rom,15,0xc134,{0x9d,0xcc,2}); put(rom,15,0xc238,{0x9d,0x44,3});
		put(rom,15,0xdb6e,{0x20,0x16,0xe0}); put(rom,15,0xc9af,{0xa9,7,0x8d,0x14,0x40});
		put(rom,15,0xf845,{0x9d,0,5,0xe8,0x60});
		put(rom,15,0xf859,{0x85,0xde,0x86,0xdf,0x84,0xe0,0x68,0x85,0xec});
		put(rom,15,0xe016,{0xa5,0x24,0xd0,4,0xa5,0x63,0xf0,0x0c});
		put(rom,15,0xc235,{0xb9,0xa9,0xb5});
		for (int i{0};i<101;++i) rom[off(14,0xb5a9)+i]=17;
		return rom;
	}
	fe::Config config(const std::vector<byte>& rom,const std::string& region="us") {
		fe::Config cfg; cfg.load_definitions(EOE_TEST_CONFIG_PATH,""); cfg.set_region(region);
		cfg.load_config_data(EOE_TEST_CONFIG_PATH,"",rom); return cfg;
	}
	std::size_t install(std::vector<byte>& rom,const std::string& spec,word org=ORG,
		std::size_t end=0xffe0,const std::string& region="us") {
		return fh::HackManager{}.install_general_hacks(config(rom,region),rom,15,org,end,
			fh::filter_general_hacks(15,fh::parse_general_hacks(spec)),nullptr);
	}
	void refused(std::vector<byte> rom,const std::string& spec,word org=ORG,
		std::size_t end=0xffe0,const std::string& region="us") {
		const auto before{rom}; bool threw{false};
		try { install(rom,spec,org,end,region); } catch(const std::exception&) { threw=true; }
		require(threw,"expected refusal: "+spec+" / "+region);
		require(rom==before,"refused install changed ROM");
	}
}

int main(int argc,char** argv) {
	try {
		// Optional ROM export for gameplay tests.
		if(argc==5 || argc==6) {
			std::ifstream input(argv[1],std::ios::binary);
			std::vector<byte> rom((std::istreambuf_iterator<char>(input)),{});
			require(input.good() || input.eof(),"input read failed");
			const std::string region{argc==6 ? argv[5] : "us"};
			install(rom,std::string{"AtlasDevEnemyHud names="}+argv[3]+" visibility="+argv[4],
				region=="jp" ? word{0xfdb2} : ORG,0xffe0,region);
			std::ofstream output(argv[2],std::ios::binary);
			output.write(reinterpret_cast<const char*>(rom.data()),rom.size());
			require(output.good(),"output write failed"); return 0;
		}
		require(argc==1,"expected input ROM, output ROM, names boolean, visibility");
		const auto source{fixture()};
		for(bool names : {false,true}) for(bool persistent : {false,true}) {
			const std::string spec{std::string{"AtlasDevEnemyHud names="}+(names?"true":"false")+
				" visibility="+(persistent?"always":"timed")};
			for(word org : {ORG,word{0xfd40}}) {
				auto rom{source}; require(install(rom,spec,org)==95,"fixed allocation changed");
				const std::size_t size{static_cast<std::size_t>((names?3187:1577)-(persistent?5:0))};
				require(rom[off(9,0x8000)+size]==0xff,"body exceeded expected size");
				require(rom[off(15,org)+95]==0xff,"fixed code exceeded expected size");
				require(rom[off(14,0x8897)]==0x20,"sword observer absent");
				require(rom[off(14,0x8897)+1]==(org&255),"fixed relocation absent");
				require(rom[off(14,0x8897)+2]==(org>>8),"fixed relocation high byte absent");
				for(std::size_t i{0};i<rom.size();++i) if(rom[i]!=source[i]) {
					bool owned{i>=off(9,0x8000) && i<off(9,0x8000)+size};
					owned=owned || (i>=off(15,org) && i<off(15,org)+95);
					for(auto at : {off(14,0x8897),off(14,0x8203),off(15,0xc134),off(15,0xc238),off(15,0xdb6e)})
						owned=owned || (i>=at && i<at+3);
					require(owned,"write outside owned ranges");
				}
				refused(rom,spec,org);
			}
		}
		for(const auto& spec : {"AtlasDevEnemyHud names=maybe","AtlasDevEnemyHud visibility=nearest",
			"AtlasDevEnemyHud mode=unknown","AtlasDevEnemyHud typo=1",
			"AtlasDevEnemyHud names=maybe mode=vanilla","AtlasDevEnemyHud visibility=bad mode=vanilla",
			"AtlasDevEnemyHud,AtlasDevEnemyHud","AtlasDevEnemyHud,AtlasDevFrameScheduler",
			"AtlasDevFrameScheduler,AtlasDevEnemyHud","AtlasDevEnemyHud,AtlasDevEnemyStats",
			"AtlasDevEnemyHud,AtlasDevInfectedTint","AtlasDevTimeOfDay,AtlasDevEnemyHud",
			"AtlasDevDayNightCycle,AtlasDevEnemyHud"})
			refused(source,spec);
		for(const auto& region : {"randum","en-transl","512"}) refused(source,"AtlasDevEnemyHud",ORG,0xffe0,region);
		for(const std::string region : {"us-rev-a","eu","jp"}) {
			auto regional{source};
			if(region=="jp") {
				put(regional,15,0xf90d,{0x9d,0,5,0xe8,0x60});
				put(regional,15,0xf921,{0x85,0xde,0x86,0xdf,0x84,0xe0,0x68,0x85,0xec});
			} else put(regional,15,0xf859,{0x85,0xe2,0x86,0xe3,0x84,0xe4,0x68,0x85,0xec});
			for(const auto& spec : {"AtlasDevEnemyHud", "AtlasDevEnemyHud names=false",
				"AtlasDevEnemyHud visibility=always", "AtlasDevEnemyHud names=false visibility=always"}) {
				auto rom{regional};require(install(rom,spec,0xfdb2,0xffe0,region)==95,"regional allocation");
			}
		}
		for(auto at : {off(14,0x8897),off(14,0x8203),off(15,0xc134),off(15,0xc238),
			off(15,0xdb6e),off(15,0xc9af),off(15,0xf845),off(15,0xf859),off(15,0xe016),
			off(15,0xc235),off(9,0x8000),off(9,0x8000)+1571,off(15,ORG)}) {
			auto rom{source}; rom[at]^=1; refused(rom,"AtlasDevEnemyHud");
		}
		refused(source,"AtlasDevEnemyHud",ORG,ORG+94);
		refused(source,"AtlasDevEnemyHud",0xffd0);
		auto short_rom{source}; short_rom.resize(100); refused(short_rom,"AtlasDevEnemyHud");
		for(int at : {0,4,5,6,7}) {auto rom{source};rom[at]=255;refused(rom,"AtlasDevEnemyHud");}
		auto rom{source}; require(install(rom,"AtlasDevEnemyHud mode=vanilla")==0 && rom==source,"vanilla mode writes");
		std::cout<<"enemy HUD native variants, relocation, ownership and refusal checks passed\n";
		return 0;
	} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
