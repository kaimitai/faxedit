#include <SDL.h>
#include <iostream>
#include "./common/klib/Kfile.h"
#include "./fe/Game.h"

int main(int argc, char** argv) {

	fe::Game l_game(klib::file::read_file_as_bytes("c:/Temp/Faxanadu (USA) (Rev A).nes"));

}
