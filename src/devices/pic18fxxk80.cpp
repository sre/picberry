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

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

#include "pic18fxxk80.h"

/* delays (in microseconds) */
#define DELAY_P1   	1
#define DELAY_P2   	1
#define DELAY_P2A  	1
#define DELAY_P2B  	1
#define DELAY_P3   	1
#define DELAY_P4   	1
#define DELAY_P5   	1
#define DELAY_P5A  	1
#define DELAY_P6   	1
#define DELAY_P9  	2000
#define DELAY_P9A  	10000
#define DELAY_P10  	200
#define DELAY_P11  	10000
#define DELAY_P11A 	8000
#define DELAY_P12  	500
#define DELAY_P13  	1
#define DELAY_P14  	1
#define DELAY_P16  	1
#define DELAY_P17  	1

/* commands for programming */
#define COMM_CORE_INSTRUCTION 				0x00
#define COMM_SHIFT_OUT_TABLAT 				0x02
#define COMM_TABLE_READ	     				0x08
#define COMM_TABLE_READ_POST_INC 			0x09
#define COMM_TABLE_READ_POST_DEC			0x0A
#define COMM_TABLE_READ_PRE_INC 			0x0B
#define COMM_TABLE_WRITE					0x0C
#define COMM_TABLE_WRITE_POST_INC_2			0x0D
#define COMM_TABLE_WRITE_STARTP_POST_INC_2	0x0E
#define COMM_TABLE_WRITE_STARTP				0x0F

#define ENTER_PROGRAM_KEY					0x4D434850

#define ERASE_DATA_EEPROM					0x800004
#define ERASE_BOOT_BLOCK					0x800005
#define ERASE_CONFIG_BITS					0x800002
#define ERASE_CODE_BLOCK_0					0x800104
#define ERASE_CODE_BLOCK_1					0x800204
#define ERASE_CODE_BLOCK_2					0x800404
#define ERASE_CODE_BLOCK_3					0x800804
#define ERASE_CODE_BLOCK_4					0x801004
#define ERASE_CODE_BLOCK_5					0x802004
#define ERASE_CODE_BLOCK_6					0x804004
#define ERASE_CODE_BLOCK_7					0x808004

#define LOCATION_USERID						0x200000
#define LOCATION_CONFIG						0x300000
#define LOCATION_DEVID						0x3FFFFE

#define EEPROM_SIZE							1024

/*
 * Memory Layout
 * 0x000000 - 0x00FFFF - Code Memory
 * 0x010000 - 0x1FFFFF - always '0x00'
 * 0x200000 - 0x200007 - User ID
 * 0x200008 - 0x2FFFFF - always '0x00'
 * 0x300000 - 0x30000D - Config
 * 0x30000E - 0x3FFFFD - always '0x00'
 * 0x3FFFFE - 0x3FFFFF - Device ID
 * 0x400000 - 0xFFFFFF - n/a
 */

void pic18fxxk80::enter_program_mode(void)
{
	int i;

	GPIO_IN(pic_mclr);
	GPIO_OUT(pic_mclr);

	GPIO_CLR(pic_mclr);			/* remove VDD from MCLR pin */
	delay_us(DELAY_P13);		/* wait P13 */
	GPIO_SET(pic_mclr);			/* apply VDD to MCLR pin */
	delay_us(10);				/* wait (no minimum time requirement) */
	GPIO_CLR(pic_mclr);			/* remove VDD from MCLR pin */
	delay_us(DELAY_P12);		/* wait P12 */

	GPIO_CLR(pic_clk);
	/* Shift in the "enter program mode" key sequence (MSB first) */
	for (i = 31; i >= 0; i--) {
		if ( (ENTER_PROGRAM_KEY >> i) & 0x01 )
			GPIO_SET(pic_data);
		else
			GPIO_CLR(pic_data);
		delay_us(DELAY_P2B);	/* Setup time */
		GPIO_SET(pic_clk);
		delay_us(DELAY_P2A);	/* Hold time */
		GPIO_CLR(pic_clk);

	}
	GPIO_CLR(pic_data);
	delay_us(10);				/* Wait (no minimum time requirement) */
	GPIO_SET(pic_mclr);			/* apply VDD to MCLR pin */
	delay_us(10);				/* Wait (no minimum time requirement) */
}

void pic18fxxk80::exit_program_mode(void)
{

	GPIO_CLR(pic_clk);			/* stop clock on PGC */
	GPIO_CLR(pic_data);			/* clear data pin PGD */
	delay_us(DELAY_P16);		/* wait P16 */
	GPIO_CLR(pic_mclr);			/* remove VDD from MCLR pin */
	delay_us(DELAY_P17);		/* wait (at least) P17 */
	GPIO_SET(pic_mclr);
	GPIO_IN(pic_mclr);
}

/* Send a 4-bit command to the PIC (LSB first) */
void pic18fxxk80::send_cmd(uint8_t cmd)
{
	int i;

	for (i = 0; i < 4; i++) {
		GPIO_SET(pic_clk);
		if ( (cmd >> i) & 0x01 )
			GPIO_SET(pic_data);
		else
			GPIO_CLR(pic_data);
		delay_us(DELAY_P2B);	/* Setup time */
		GPIO_CLR(pic_clk);
		delay_us(DELAY_P2A);	/* Hold time */
	}
	GPIO_CLR(pic_data);
	delay_us(DELAY_P5);
}

/* Read 8-bit data from the PIC (LSB first) */
uint16_t pic18fxxk80::read_data(void)
{
	uint8_t i;
	uint16_t data = 0x0000;

	for (i = 0; i < 8; i++) {
		GPIO_SET(pic_clk);
		delay_us(DELAY_P2B);
		GPIO_CLR(pic_clk);
		delay_us(DELAY_P2A);
	}

	delay_us(DELAY_P6);	/* wait for the data... */

	GPIO_IN(pic_data);

	for (i = 0; i < 8; i++) {
		GPIO_SET(pic_clk);
		delay_us(DELAY_P14);	/* Wait for data to be valid */
		data |= ( GPIO_LEV(pic_data) & 0x00000001 ) << i;
		delay_us(DELAY_P2B);
		GPIO_CLR(pic_clk);
		delay_us(DELAY_P2A);
	}

	delay_us(DELAY_P5A);
	GPIO_IN(pic_data);
	GPIO_OUT(pic_data);
	return data;
}

/* Load 16-bit data to the PIC (LSB first) */
void pic18fxxk80::write_data(uint16_t data)
{
	int i;

	for (i = 0; i < 16; i++) {
		GPIO_SET(pic_clk);
		if ( (data >> i) & 0x0001 )
			GPIO_SET(pic_data);
		else
			GPIO_CLR(pic_data);
		delay_us(DELAY_P2B);	/* Setup time */
		GPIO_CLR(pic_clk);
		delay_us(DELAY_P2A);	/* Hold time */
	}
	GPIO_CLR(pic_data);
	delay_us(DELAY_P5A);
}

void pic18fxxk80::send_instruction(uint8_t cmd, uint16_t data)
{
	if (flags.debug)
		fprintf(stderr, "instruction: %02x %04x\n", cmd, data);

	send_cmd(cmd);
	write_data(data);
}

/* set Table Pointer */
void pic18fxxk80::goto_mem_location(uint32_t data)
{
	uint8_t addr1 = (data >> 0) & 0xFF;
	uint8_t addr2 = (data >> 8) & 0xFF;
	uint8_t addr3 = (data >> 16) & 0xFF;

	send_instruction(COMM_CORE_INSTRUCTION, 0x0E00 | addr3);	/* MOVLW Addr[21:16] */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6EF8);			/* MOVWF TBLPTRU */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0E00 | addr2);	/* MOVLW Addr[15:8] */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6EF7);			/* MOVWF TBLPTRH */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0E00 | addr1);	/* MOVLW Addr[7:0] */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6EF6);			/* MOVWF TBLPTRL */
}

void pic18fxxk80::goto_mem_location2(uint8_t data)
{
	send_instruction(COMM_CORE_INSTRUCTION, 0x0E00 | data);		/* MOVLW Addr[7:0] */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6EF6);			/* MOVWF TBLPTRL */
}

void pic18fxxk80::dump_user_id(void)
{
	uint8_t id;
	cout << "User IDs:" << endl;

	goto_mem_location(LOCATION_CONFIG);

	for (int i=1; i<8; i++) {
		send_cmd(COMM_TABLE_READ_POST_INC);
		id = read_data();

		fprintf(stdout, " - ID Location %d: 0x%02x\n", i, id);
	}

	cout << endl;
}

/* Read PIC device id word, located at 0x3FFFFE:0x3FFFFF */
bool pic18fxxk80::read_device_id(void)
{
	uint16_t id;
	bool found = 0;

	goto_mem_location(LOCATION_DEVID);

	send_cmd(COMM_TABLE_READ_POST_INC);
	id = read_data();
	send_cmd(COMM_TABLE_READ_POST_INC);
	id = ( read_data() << 8) | (id) ;

	device_id = (id & 0xFFE0);
	device_rev = (id & 0x1F);

	for (unsigned short i=0; i < sizeof(piclist)/sizeof(piclist[0]); i++) {
		if (piclist[i].device_id == device_id) {
			strcpy(name,piclist[i].name);
			mem.code_memory_size = piclist[i].code_memory_size;
			mem.program_memory_size = 0x3FFFFF;
			mem.location = (uint16_t*) calloc(mem.program_memory_size, sizeof(uint16_t));
			mem.filled = (bool*) calloc(mem.program_memory_size, sizeof(bool));
			write_buffer_size = piclist[i].write_buffer_size;
			block_count = piclist[i].block_count;
			found = 1;
			break;
		}
	}

	if (found && flags.debug) {
		cout << endl;
		dump_user_id();
	}

	return found;
}

/* Blank Check */
uint8_t pic18fxxk80::blank_check(void)
{
	uint16_t addr, data;
	uint8_t ret = 0;
	unsigned int lcounter = 0;

	if (!flags.debug) cerr << "[ 0%]";

	goto_mem_location(0x000000);

	for (addr = 0; (2*addr) < mem.code_memory_size; addr++) {
		send_cmd(COMM_TABLE_READ_POST_INC);
		data = read_data();
		send_cmd(COMM_TABLE_READ_POST_INC);
		data = (read_data() << 8) | (data & 0xFF) ;

		if (data != 0xFFFF) {
			fprintf(stderr, "Chip not Blank! Address: 0x%x, Read: 0x%x.\n",  addr*2, data);
			ret = 1;
			break;
		}

		if (lcounter != (2*addr*100/mem.code_memory_size)) {
			lcounter = 2*addr*100/mem.code_memory_size;
			if (!flags.debug) fprintf(stderr, "\b\b\b\b\b[%2d%%]", lcounter);
		}
	}

	if (!flags.debug) cerr << "\b\b\b\b\b";

	return ret;

}

/* Block erase the chip */
void pic18fxxk80::block_erase(uint32_t address)
{
	uint16_t data;
	address = address & 0x00FFFFFF;	/* set the MSB byte to zero (it should already be zero)	*/

	goto_mem_location(0x3C0004);
	data = (address >> 0) & 0xFF;
	data = data << 8 | data;
	send_instruction(COMM_TABLE_WRITE, data);			/* Write Address[7:0] into 0x3C0004 */

	goto_mem_location2(0x05);
	data = (address >> 8) & 0xFF;
	data = data << 8 | data;
	send_instruction(COMM_TABLE_WRITE, data);			/* Write Address[15:8] into 0x3C0005 */

	goto_mem_location2(0x06);
	data = (address >> 16) & 0xFF;
	data = data << 8 | data;
	send_instruction(COMM_TABLE_WRITE, data);			/* Write Address[23:16] into 0x3C0006 */

	send_instruction(COMM_CORE_INSTRUCTION, 0x0000);	/* NOP */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0000);	/* NOP */

	/* Hold PGD low until erase completes. */
	GPIO_CLR(pic_data);
	delay_us(DELAY_P11);
	delay_us(DELAY_P10);
}

void pic18fxxk80::block_erase_data(void)
{
	if (block_count >= 4) {
		if (flags.debug) cerr << " - Erasing Block 0...\n";
		block_erase(ERASE_CODE_BLOCK_0);
		if (flags.debug) cerr << " - Erasing Block 1...\n";
		block_erase(ERASE_CODE_BLOCK_1);
		if (flags.debug) cerr << " - Erasing Block 2...\n";
		block_erase(ERASE_CODE_BLOCK_2);
		if (flags.debug) cerr << " - Erasing Block 3...\n";
		block_erase(ERASE_CODE_BLOCK_3);
	}
	if (block_count >= 8) {
		if (flags.debug) cerr << " - Erasing Block 4...\n";
		block_erase(ERASE_CODE_BLOCK_4);
		if (flags.debug) cerr << " - Erasing Block 5...\n";
		block_erase(ERASE_CODE_BLOCK_5);
		if (flags.debug) cerr << " - Erasing Block 6...\n";
		block_erase(ERASE_CODE_BLOCK_6);
		if (flags.debug) cerr << " - Erasing Block 7...\n";
		block_erase(ERASE_CODE_BLOCK_7);
	}
}

/* Bulk erase the chip */
void pic18fxxk80::bulk_erase(void)
{
	if (flags.debug) cerr << "\n";

	if (flags.boot_only) {
		if (flags.debug) cerr << " - Erasing Boot block...\n";
		block_erase(ERASE_BOOT_BLOCK);
	} else if (flags.program_only) {
		block_erase_data();
	} else if (flags.eeprom_only) {
		if (flags.debug) cerr << " - Erasing EEPROM...\n";
		eeprom_erase();
	} else {
		/* no need to erase EEPROM, it is erased when any block is erased */
		if (flags.debug) cerr << " - Erasing Config bits...\n";
		block_erase(ERASE_CONFIG_BITS);
		if (flags.debug) cerr << " - Erasing Boot block...\n";
		block_erase(ERASE_BOOT_BLOCK);

		block_erase_data();
	}

	if(flags.client) fprintf(stdout, "@FIN");
}

uint8_t pic18fxxk80::eeprom_read_cell(uint16_t address)
{
	/* Step 1: Direct access to data EEPROM */
	send_instruction(COMM_CORE_INSTRUCTION, 0x9e7f);							/* BCF EECON1, EEPGD */
	send_instruction(COMM_CORE_INSTRUCTION, 0x9c7f);							/* BCF EECON1, CFGS */

	/* Step 2: Set the data EEPROM Address Pointer */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0e00 | ((address >> 0) & 0xFF));	/* MOVLW <Addr> */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6e74);							/* MOVWF EEADR */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0e00 | ((address >> 8) & 0xFF));	/* MOVLW <AddrH> */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6e75);							/* MOVWF EEADRH */

	/* Step 3: Initiate a memory read */
	send_instruction(COMM_CORE_INSTRUCTION, 0x807f);							/* BSF EECON1, RD */

	/* Step 4: Load data into the Serial Data Holding register */
	send_instruction(COMM_CORE_INSTRUCTION, 0x5073);							/* MOVF EEDATA, W, 0 */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6ef5);							/* MOVWF TABLAT */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0000);							/* NOP */

	send_cmd(COMM_SHIFT_OUT_TABLAT);
	return (read_data() >> 8) & 0xFF;
}

void pic18fxxk80::eeprom_write_cell(uint16_t address, uint8_t data)
{
	/* Step 1: Direct access to data EEPROM */
	send_instruction(COMM_CORE_INSTRUCTION, 0x9e7f);							/* BCF EECON1, EEPGD */
	send_instruction(COMM_CORE_INSTRUCTION, 0x9c7f);							/* BCF EECON1, CFGS */

	/* Step 2: Set the data EEPROM Address Pointer */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0e00 | ((address >> 0) & 0xFF));	/* MOVLW <Addr> */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6e74);							/* MOVWF EEADR */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0e00 | ((address >> 8) & 0xFF));	/* MOVLW <AddrH> */
	send_instruction(COMM_CORE_INSTRUCTION, 0x6e75);							/* MOVWF EEADRH */

	/* Step 3: Load the data to be written */
	send_instruction(COMM_CORE_INSTRUCTION, 0x0e00 | (data & 0xFF));			/* MOVLW <Data> */

	/* Step 4: Enable memory writes */
	send_instruction(COMM_CORE_INSTRUCTION, 0x847f);							/* BSF EECON1, WREN */

	/* Step 5: Initiate write */
	send_instruction(COMM_CORE_INSTRUCTION, 0x827f);							/* BSF EECON1, WR */

	/* Step 6: Poll WR bit, repeat until the bit is clear */
	do {
		send_instruction(COMM_CORE_INSTRUCTION, 0x507f);						/* MOVF EECON1, W, 0 */
		send_instruction(COMM_CORE_INSTRUCTION, 0x6ef5);						/* MOVWF TABLAT */
		send_instruction(COMM_CORE_INSTRUCTION, 0x0000);						/* NOP */

		send_cmd(COMM_SHIFT_OUT_TABLAT);
	} while (read_data() & 0x0002);

	/* Step 7: Hold PGC low for time, P10 */
	GPIO_CLR(pic_clk);
	delay_us(DELAY_P10);
	
	/* Step 8: Disable writes */
	send_instruction(COMM_CORE_INSTRUCTION, 0x947f);							/* BCF EECON1, WREN */
}

void pic18fxxk80::eeprom_read(char *outfile)
{
	uint16_t address;
	uint8_t byte;

	for (address=0; address < EEPROM_SIZE; address++) {
		byte = eeprom_read_cell(address);
		if (flags.debug) fprintf(stdout, "EEPROM 0x%03x: 0x%02x\n", address, byte);
	}

	// TODO: generate outfile
}

void pic18fxxk80::eeprom_write(char *infile)
{
	uint16_t address;

	// TODO: read infile and write correct data instead of 0x00

	for (address=0; address < EEPROM_SIZE; address++) {
		eeprom_write_cell(address, 0x00);
	}
}

void pic18fxxk80::eeprom_erase()
{
	block_erase(ERASE_DATA_EEPROM);
}

/* Read PIC memory and write the contents to a .hex file */
void pic18fxxk80::read(char *outfile, uint32_t start, uint32_t count)
{
	uint16_t addr, data = 0x0000;
	unsigned int lcounter = 0;

	if (!flags.debug) cerr << "[ 0%]";
	if (flags.client) fprintf(stdout, "@000");

	/* Read Memory */

	goto_mem_location(0x000000);

	for (addr = 0; addr*2 < mem.code_memory_size; addr++) {
		send_cmd(COMM_TABLE_READ_POST_INC);
		data = read_data();
		send_cmd(COMM_TABLE_READ_POST_INC);
		data = ( read_data() << 8 ) | (data & 0x00FF);

		if (flags.debug)
			fprintf(stderr, "  addr = 0x%04X  data = 0x%04X\n", addr*2, data);

		if (data != 0xFFFF) {
			mem.location[addr] = data;
			mem.filled[addr] = 1;
		}

		if (lcounter != 2*addr*100/mem.code_memory_size) {
			lcounter = 2*addr*100/mem.code_memory_size;
			if (flags.client)
				fprintf(stderr,"RED@%2d\n", lcounter);
			if (!flags.debug)
				fprintf(stderr,"\b\b\b\b%2d%%]", lcounter);
		}
	}

	if (!flags.debug) cerr << "\b\b\b\b\b";
	if (flags.client) fprintf(stdout, "@FIN");
	write_inhx(&mem, outfile);
}

void pic18fxxk80::programming_sequence()
{
	uint8_t i;

	GPIO_CLR(pic_data);
	for (i = 0; i < 3; i++) {
		GPIO_SET(pic_clk);
		delay_us(DELAY_P2B);       /* Setup time */
		GPIO_CLR(pic_clk);
		delay_us(DELAY_P2A);       /* Hold time */
	}
	GPIO_SET(pic_clk);

	delay_us(DELAY_P9);        /* Programming time */

	GPIO_CLR(pic_clk);
	delay_us(DELAY_P10);
	write_data(0x0000);
}

void pic18fxxk80::write_code()
{
	int i;
	uint16_t data;
	uint32_t addr = 0x00000000;
	unsigned int lcounter;

	if(!flags.debug) cerr << "[ 0%]";
	if(flags.client) fprintf(stdout, "@000");
	lcounter = 0;

	/* Step 1: Direct access to code memory and enable writes */
	send_instruction(COMM_CORE_INSTRUCTION, 0x8e7f);	/* BSF EECON1, EEPGD */
	send_instruction(COMM_CORE_INSTRUCTION, 0x9c7f);	/* BCF EECON1, CFGS */
	send_instruction(COMM_CORE_INSTRUCTION, 0x847f);	/* BSF EECON1, WREN */

	goto_mem_location(0);

	for (addr = 0; (addr*2) < mem.code_memory_size; addr += write_buffer_size/2) {        /* address in WORDS (2 Bytes) */
		for (i=0; i<(write_buffer_size/2-1); i++) {		                        /* write all but the last word */
			if (mem.filled[addr+i]) {
				if (flags.debug)
					fprintf(stderr, "  Writing 0x%04X to address 0x%06X \n", mem.location[addr + i], (addr+i)*2 );
				send_instruction(COMM_TABLE_WRITE_POST_INC_2, mem.location[addr+i]);
			} else {
				if (flags.debug)
					fprintf(stderr, "  Writing 0xFFFF to address 0x%06X \n", (addr+i)*2 );
				send_instruction(COMM_TABLE_WRITE_POST_INC_2, 0xFFFF);
			};
		}

		/* write the last word (2 bytes) and start programming */
		if (mem.filled[addr+(write_buffer_size/2-1)]) {
			if (flags.debug)
				fprintf(stderr, "  Writing 0x%04X to address 0x%06X and then start programming...\n", mem.location[addr+(write_buffer_size/2-1)], (addr+(write_buffer_size/2-1))*2);
			send_instruction(COMM_TABLE_WRITE_STARTP_POST_INC_2, mem.location[addr+(write_buffer_size/2-1)]);
		} else {
			if (flags.debug)
				fprintf(stderr, "  Writing 0xFFFF to address 0x%06X and then start programming...\n", (addr+(write_buffer_size/2-1))*2);
			send_instruction(COMM_TABLE_WRITE_STARTP_POST_INC_2, 0xFFFF);
		};

		/* Programming Sequence */
		programming_sequence();

		if (lcounter != addr*2*100/mem.code_memory_size) {
			lcounter = addr*2*100/mem.code_memory_size;
			if(flags.client)
				fprintf(stdout,"@%03d", lcounter);
			if(!flags.debug)
				fprintf(stderr,"\b\b\b\b\b[%2d%%]", lcounter);
		}
	};

	if(!flags.debug) cerr << "\b\b\b\b\b\b";
	if(flags.client) fprintf(stdout, "@100");

	/* Verify Code Memory */
	if(!flags.noverify) {
		if(!flags.debug) cerr << "[ 0%]";
		if(flags.client) fprintf(stdout, "@000");
		lcounter = 0;

		goto_mem_location(0x000000);

		for (addr = 0; (addr*2) < mem.code_memory_size; addr++) {
			send_cmd(COMM_TABLE_READ_POST_INC);
			data = read_data();
			send_cmd(COMM_TABLE_READ_POST_INC);
			data = ( read_data() << 8 ) | ( data & 0xFF );

			if (flags.debug)
				fprintf(stderr, "addr = 0x%06X:  pic = 0x%04X, file = 0x%04X\n",
						addr*2, data, (mem.filled[addr]) ? (mem.location[addr]) : 0xFFFF);

			if ((data != mem.location[addr]) & ( mem.filled[addr])) {
				fprintf(stderr, "Error at addr = 0x%06X:  pic = 0x%04X, file = 0x%04X.\nExiting...",
						addr*2, data, mem.location[addr]);
				break;
			}
			if (lcounter != addr*2*100/mem.code_memory_size) {
				lcounter = addr*2*100/mem.code_memory_size;
				if(flags.client)
					fprintf(stdout,"@%03d", lcounter);
				if(!flags.debug)
					fprintf(stderr,"\b\b\b\b\b[%2d%%]", lcounter);
			}
		}

		if (!flags.debug) cerr << "\b\b\b\b\b";
		if (flags.client) fprintf(stdout, "@FIN");
	}
	else {
		if (flags.client) fprintf(stdout, "@FIN");
	}
}

void pic18fxxk80::write(char *infile)
{
	read_inhx(infile, &mem);
	write_code();
	write_configuration_registers();
}

void pic18fxxk80::dump_configuration_registers(void)
{
	uint16_t conf;
	cout << "Configuration Words:" << endl;

	goto_mem_location(LOCATION_CONFIG);

	for (int i=1; i<8; i++) {
		send_cmd(COMM_TABLE_READ_POST_INC);
		conf = read_data();
		send_cmd(COMM_TABLE_READ_POST_INC);
		conf = read_data() << 8 | conf;

		fprintf(stdout, " - CONFIG%d: 0x%04x\n", i, conf);
	}

	cout << endl;
}

uint16_t pic18fxxk80::configuration_register_read(uint8_t reg)
{
	uint16_t conf;

	goto_mem_location(LOCATION_CONFIG + 2*reg);
	send_cmd(COMM_TABLE_READ_POST_INC);
	conf = read_data();
	send_cmd(COMM_TABLE_READ);
	conf = read_data() << 8 | conf;
	return conf;
}

void pic18fxxk80::configuration_register_write(uint8_t reg, uint16_t data)
{
	printf("Writing configuration register %d: 0x%04X\n", reg, data);

	send_instruction(COMM_CORE_INSTRUCTION, 0x8E7F);	/* BSF EECON1, EEPGD */
	send_instruction(COMM_CORE_INSTRUCTION, 0x8C7F);	/* BSF EECON1, CFGS */

	/* write LSB */
	goto_mem_location(LOCATION_CONFIG | (reg*2));
	send_instruction(COMM_TABLE_WRITE_STARTP, data & 0xff);
	programming_sequence();

	/* write MSB */
	goto_mem_location2((reg*2)+1);
	send_instruction(COMM_TABLE_WRITE_STARTP, data & 0xff00);
	programming_sequence();
}

void pic18fxxk80::write_configuration_registers()
{
	for (int i=0; i<8; i++) {
		if (mem.filled[LOCATION_CONFIG / 2 + i])
			configuration_register_write(i, mem.location[LOCATION_CONFIG / 2 + i]);
		else
			printf("Skipping configuration register %d\n", i);
	}

	if(!flags.noverify) {
		for (int i=0; i<8; i++) {
			if (mem.filled[LOCATION_CONFIG / 2 + i]) {
				uint16_t devreg = configuration_register_read(i);
				if (mem.location[LOCATION_CONFIG / 2 + i] != devreg) {
					fprintf(stderr, "Failed to write config register at addr = 0x%06X:  pic = 0x%04X, file = 0x%04X.\n",
							LOCATION_CONFIG+2*i, devreg, mem.location[LOCATION_CONFIG / 2 + i]);
				}
			}
		}
	}
}
