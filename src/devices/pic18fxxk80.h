/*
 * Raspberry Pi PIC Programmer using GPIO connector
 * https://github.com/WallaceIT/picberry
 * Copyright 2014 Francesco Valla
 * Copyright 2021 Sebastian Reichel
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>

#include "../common.h"
#include "device.h"

using namespace std;

class pic18fxxk80: public Pic {
	public:
		void enter_program_mode(void);
		void exit_program_mode(void);
		bool setup_pe(void){return true;};
		bool read_device_id(void);
		void bulk_erase(void);
		void dump_configuration_registers(void);
		void read(char *outfile, uint32_t start, uint32_t count);
		void write(char *infile);
		uint8_t blank_check(void);

		void eeprom_read(char *outfile);
		void eeprom_write(char *infile);
		void eeprom_erase();

	protected:
		void programming_sequence();
		void send_cmd(uint8_t cmd);
		uint16_t read_data(void);
		void write_data(uint16_t data);
		void send_instruction(uint8_t cmd, uint16_t data);
		void block_erase(uint32_t address);
		void goto_mem_location(uint32_t data);
		void goto_mem_location2(uint8_t data);
		void dump_user_id(void);
		uint8_t eeprom_read_cell(uint16_t address);
		void eeprom_write_cell(uint16_t address, uint8_t value);
		void configuration_register_write(uint8_t reg, uint16_t data);
		uint16_t configuration_register_read(uint8_t reg);
		void write_configuration_registers();
		void write_code();

		/*
		* DEVICES SECTION
		*   ID        NAME            MEMSIZE
		*/
		pic_device piclist[12] = {
			{0x6180,  "PIC18F25K80",   0x8000},
			{0x6260,  "PIC18LF25K80",  0x8000},
			{0x6120,  "PIC18F26K80",  0x10000},
			{0x6200,  "PIC18LF26K80", 0x10000},
			{0x6160,  "PIC18F45K80",   0x8000},
			{0x6240,  "PIC18LF45K80",  0x8000},
			{0x6100,  "PIC18F46K80",  0x10000},
			{0x61e0,  "PIC18LF46K80", 0x10000},
			{0x6140,  "PIC18F65K80",   0x8000},
			{0x6220,  "PIC18LF65K80",  0x8000},
			{0x60e0,  "PIC18F66K80",  0x10000},
			{0x61c0,  "PIC18LF66K80", 0x10000}
		};
};
