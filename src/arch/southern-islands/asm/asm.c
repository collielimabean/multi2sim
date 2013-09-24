/* 
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <ctype.h>

#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/list.h>
#include <lib/util/misc.h>
#include <lib/util/string.h>

#include "asm.h"
#include "bin-file.h"
#include "inst.h"
#include "opengl-bin-file.h"



StringMap si_inst_fmt_map =
{
	{ "<invalid>", SIInstFormatInvalid },
	{ "sop2", SIInstFormatSOP2 },
	{ "sopk", SIInstFormatSOPK },
	{ "sop1", SIInstFormatSOP1 },
	{ "sopc", SIInstFormatSOPC },
	{ "sopp", SIInstFormatSOPP },
	{ "smrd", SIInstFormatSMRD },
	{ "vop2", SIInstFormatVOP2 },
	{ "vop1", SIInstFormatVOP1 },
	{ "vopc", SIInstFormatVOPC },
	{ "vop3a", SIInstFormatVOP3a },
	{ "vop3b", SIInstFormatVOP3b },
	{ "vintrp", SIInstFormatVINTRP },
	{ "ds", SIInstFormatDS },
	{ "mubuf", SIInstFormatMUBUF },
	{ "mtbuf", SIInstFormatMTBUF },
	{ "mimg", SIInstFormatMIMG },
	{ "exp", SIInstFormatEXP },
	{ 0, 0 }
};

/* String maps for assembly dump. */
StringMap si_inst_sdst_map = {
	{"reserved", 0},
	{"reserved", 1},
	{"vcc_lo", 2},
	{"vcc_hi", 3},
	{"tba_lo", 4},
	{"tba_hi", 5},
	{"tma_lo", 6},
	{"tma_hi", 7},
	{"ttmp0", 8},
	{"ttmp1", 9},
	{"ttmp2", 10},
	{"ttmp3", 11},
	{"ttmp4", 12},
	{"ttmp5", 13},
	{"ttmp6", 14},
	{"ttmp7", 15},
	{"ttmp8", 16},
	{"ttmp9", 17},
	{"ttmp10", 18},
	{"ttmp11", 19},
	{"m0", 20},
	{"reserved", 21},
	{"exec_lo", 22},
	{"exec_hi", 23},
	{ 0, 0 }
};

StringMap si_inst_ssrc_map = {
	{"0.5", 0},
	{"-0.5", 1},
	{"1.0", 2},
	{"-1.0", 3},
	{"2.0", 4},
	{"-2.0", 5},
	{"4.0", 6},
	{"-4.0", 7},
	{"reserved", 8},
	{"reserved", 9},
	{"reserved", 10},
	{"vccz", 11},
	{"execz", 12},
	{"scc", 13},
	{"reserved", 14},
	{"literal constant", 15},
	{0, 0}
};

StringMap si_inst_buf_data_format_map = {
	{"invalid", SIInstBufDataFormatInvalid },
	{"BUF_DATA_FORMAT_8", SIInstBufDataFormat8 },
	{"BUF_DATA_FORMAT_16", SIInstBufDataFormat16 },
	{"BUF_DATA_FORMAT_8_8", SIInstBufDataFormat8_8 },
	{"BUF_DATA_FORMAT_32", SIInstBufDataFormat32 },
	{"BUF_DATA_FORMAT_16_16", SIInstBufDataFormat16_16 },
	{"BUF_DATA_FORMAT_10_11_11", SIInstBufDataFormat10_11_11 },
	{"BUF_DATA_FORMAT_11_10_10", SIInstBufDataFormat11_10_10 },
	{"BUF_DATA_FORMAT_10_10_10_2", SIInstBufDataFormat10_10_10_2 },
	{"BUF_DATA_FORMAT_2_10_10_10", SIInstBufDataFormat2_10_10_10 },
	{"BUF_DATA_FORMAT_8_8_8_8", SIInstBufDataFormat8_8_8_8 },
	{"BUF_DATA_FORMAT_32_32", SIInstBufDataFormat32_32 },
	{"BUF_DATA_FORMAT_16_16_16_16", SIInstBufDataFormat16_16_16_16 },
	{"BUF_DATA_FORMAT_32_32_32", SIInstBufDataFormat32_32_32 },
	{"BUF_DATA_FORMAT_32_32_32_32", SIInstBufDataFormat32_32_32_32 },
	{"reserved", SIInstBufDataFormatReserved },
	{ 0, 0 }
};

StringMap si_inst_buf_num_format_map = {
	{"BUF_NUM_FORMAT_UNORM", SIInstBufNumFormatUnorm },
	{"BUF_NUM_FORMAT_SNORM", SIInstBufNumFormatSnorm },
	{"BUF_NUM_FORMAT_UNSCALED", SIInstBufNumFormatUnscaled },
	{"BUF_NUM_FORMAT_SSCALED", SIInstBufNumFormatSscaled },
	{"BUF_NUM_FORMAT_UINT", SIInstBufNumFormatUint },
	{"BUF_NUM_FORMAT_SINT", SIInstBufNumFormatSint },
	{"BUF_NUM_FORMAT_SNORM_NZ", SIInstBufNumFormatSnormNz },
	{"BUF_NUM_FORMAT_FLOAT", SIInstBufNumFormatFloat },
	{"reserved", SIInstBufNumFormatReserved },
	{"BUF_NUM_FORMAT_SRGB", SIInstBufNumFormatSrgb },
	{"BUF_NUM_FORMAT_UBNORM", SIInstBufNumFormatUbnorm },
	{"BUF_NUM_FORMAT_UBNORM_NZ", SIInstBufNumFormatUbnormNz },
	{"BUF_NUM_FORMAT_UBINT", SIInstBufNumFormatUbint },
	{"BUF_NUM_FORMAT_UBSCALED", SIInstBufNumFormatUbscaled },
	{ 0, 0 }
};

StringMap si_inst_OP16_map = {
	{"f", 0},
	{"lt", 1},
	{"eq", 2},
	{"le", 3},
	{"gt", 4},
	{"lg", 5},
	{"ge", 6},
	{"o", 7},
	{"u", 8},
	{"nge", 9},
	{"nlg", 10},
	{"ngt", 11},
	{"nle", 12},
	{"neq", 13},
	{"nlt", 14},
	{"tru", 15},
	{ 0, 0 }
};

StringMap si_inst_OP8_map = {
	{"f", 0},
	{"lt", 1},
	{"eq", 2},
	{"le", 3},
	{"gt", 4},
	{"lg", 5},
	{"ge", 6},
	{"tru", 7},
	{ 0, 0 }
};

StringMap si_inst_special_reg_map = {
	{ "vcc", SIInstSpecialRegVcc },
	{ "scc", SIInstSpecialRegScc },
	{ "exec", SIInstSpecialRegExec },
	{ "tma", SIInstSpecialRegTma },
	{ 0, 0 }
};




/*
 * Class 'SIAsm'
 */

void SIAsmCreate(SIAsm *self)
{
	SIInstInfo *info;
	int i;

	/* Allocate array of instruction information */
	self->inst_info = xcalloc(SIInstOpcodeCount, sizeof(SIInstInfo));

	/* Type size assertions */
	assert(sizeof(SIInstReg) == 4);

	/* Read information about all instructions */
#define DEFINST(_name, _fmt_str, _fmt, _op, _size, _flags) \
	info = &self->inst_info[SI_INST_##_name]; \
	info->opcode = SI_INST_##_name; \
	info->name = #_name; \
	info->fmt_str = _fmt_str; \
	info->fmt = SIInstFormat##_fmt; \
	info->op = _op; \
	info->size = _size; \
	info->flags = _flags;
#include "asm.dat"
#undef DEFINST

	/* Tables of pointers to 'si_inst_info' */
	for (i = 1; i < SIInstOpcodeCount; i++)
	{
		info = &self->inst_info[i];

		if (info->fmt == SIInstFormatSOPP)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_sopp_count - 1));
			self->inst_info_sopp[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatSOPC)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_sopc_count - 1));
			self->inst_info_sopc[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatSOP1)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_sop1_count - 1));
			self->inst_info_sop1[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatSOPK)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_sopk_count - 1));
			self->inst_info_sopk[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatSOP2)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_sop2_count - 1));
			self->inst_info_sop2[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatSMRD) 
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_smrd_count - 1));
			self->inst_info_smrd[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatVOP3a || info->fmt == SIInstFormatVOP3b)
		{
			int i;

			assert(IN_RANGE(info->op, 0, 
				si_inst_info_vop3_count - 1));
			self->inst_info_vop3[info->op] = info;
			if (info->flags & SIInstFlagOp8)
			{
				for (i = 1; i < 8; i++)
				{
					self->inst_info_vop3[info->op + i] = 
						info;
				}
			}
			if (info->flags & SIInstFlagOp16)
			{
				for (i = 1; i < 16; i++)
				{
					self->inst_info_vop3[info->op + i] = 
						info;
				}
			}
			continue;
		}
		else if (info->fmt == SIInstFormatVOPC)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_vopc_count - 1));
			self->inst_info_vopc[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatVOP1)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_vop1_count - 1));
			self->inst_info_vop1[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatVOP2)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_vop2_count - 1));
			self->inst_info_vop2[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatVINTRP)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_vintrp_count - 1));
			self->inst_info_vintrp[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatDS)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_ds_count - 1));
			self->inst_info_ds[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatMTBUF)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_mtbuf_count - 1));
			self->inst_info_mtbuf[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatMUBUF)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_mubuf_count - 1));
			self->inst_info_mubuf[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatMIMG)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_mimg_count - 1));
			self->inst_info_mimg[info->op] = info;
			continue;
		}
		else if (info->fmt == SIInstFormatEXP)
		{
			assert(IN_RANGE(info->op, 0, 
				si_inst_info_exp_count - 1));
			self->inst_info_exp[info->op] = info;
			continue;
		}
		else 
		{
			fprintf(stderr, "warning: '%s' not indexed\n", 
				info->name);
		}
	}
}


void SIAsmDestroy(SIAsm *self)
{
	free(self->inst_info);
}


static void SIAsmDisassembleBuffer(SIAsm *self, void *ptr, int size, FILE *f)
{
	void *original_ptr = ptr;

	int inst_count = 0;
	int rel_addr = 0;

	int label_addr[size / 4];	/* A list of created labels sorted by rel_addr. */

	int *next_label = &label_addr[0];	/* The next label to dump. */
	int *end_label = &label_addr[0];	/* The address after the last label. */

	SIInst inst;

	/* Create instruction */
	new_static(&inst, SIInst, self);

	/* Read through instructions to find labels. */
	while (ptr < original_ptr + size)
	{
		/* Decode instruction */
		SIInstDecode(&inst, ptr, rel_addr);

		/* If ENDPGM, break. */
		if (inst.info->fmt == SIInstFormatSOPP && 
			inst.bytes.sopp.op == 1)
		{
			break;
		}
		/* If the instruction branches, insert the label into 
		 * the sorted list. */
		if (inst.info->fmt == SIInstFormatSOPP &&
			(inst.bytes.sopp.op >= 2 && 
			 inst.bytes.sopp.op <= 9))
		{
			short simm16 = inst.bytes.sopp.simm16;
			int se_simm = simm16;
			int label = rel_addr + (se_simm * 4) + 4;

			/* Find position to insert label. */
			int *t_label = &label_addr[0];

			while (t_label < end_label && *t_label < label)
				t_label++;

			if (label != *t_label || t_label == end_label)
			{

				/* Shift labels after position down. */
				int *t2_label = end_label;

				while (t2_label > t_label)
				{
					*t2_label = *(t2_label - 1);
					t2_label--;
				}
				end_label++;

				/* Insert the new label. */
				*t_label = label;
			}

		}

		ptr += inst.size;
		rel_addr += inst.size;
	}


	/* Reset to disassemble. */
	ptr = original_ptr;
	rel_addr = 0;

	/* Disassemble */
	while (ptr < original_ptr + size)
	{
		/* Parse the instruction */
		SIInstDecode(&inst, ptr, rel_addr);
		inst_count++;

		/* Dump a label if necessary. */
		if (*next_label == rel_addr && next_label != end_label)
		{
			fprintf(f, "label_%04X:\n", rel_addr / 4);
			next_label++;
		}


		/* Dump the instruction */
		int line_size = MAX_STRING_SIZE;
		char line[line_size];

		SIInstDump(&inst, rel_addr, ptr, line, line_size);
		fprintf(f, " %s", line);


		/* Break at end of program. */
		if (inst.info->fmt == SIInstFormatSOPP && 
			inst.bytes.sopp.op == 1)
		{
			break;
		}

		/* Increment instruction pointer */
		ptr += inst.size;
		rel_addr += inst.size;
	}

	/* Free instruction */
	delete_static(&inst);
}


void SIAsmDisassembleBinary(SIAsm *self, char *path)
{
	struct elf_file_t *elf_file;
	struct elf_symbol_t *symbol;
	struct elf_section_t *section;
	struct si_bin_file_t *amd_bin;

	char kernel_name[MAX_STRING_SIZE];

	void *ptr;

	int size;
	int i;

	/* Decode external ELF */
	elf_file = elf_file_create_from_path(path);
	for (i = 0; i < list_count(elf_file->symbol_table); i++)
	{
		/* Get symbol and section */
		symbol = list_get(elf_file->symbol_table, i);
		section = list_get(elf_file->section_list, symbol->section);
		if (!section)
			continue;

		/* If symbol is '__OpenCL_XXX_kernel', it points 
		 * to internal ELF */
		if (str_prefix(symbol->name, "__OpenCL_") && 
			str_suffix(symbol->name, "_kernel"))
		{
			/* Decode internal ELF */
			str_substr(kernel_name, sizeof(kernel_name), 
				symbol->name, 9, strlen(symbol->name) - 16);
			amd_bin = si_bin_file_create(
				section->buffer.ptr + symbol->value, 
				symbol->size, kernel_name);

			/* Get kernel name */
			printf("**\n** Disassembly for '__kernel %s'\n**\n\n",
					kernel_name);
			ptr = amd_bin->enc_dict_entry_southern_islands->sec_text_buffer.ptr;
			size = amd_bin->enc_dict_entry_southern_islands->sec_text_buffer.size;
			SIAsmDisassembleBuffer(self, ptr, size, stdout);
			printf("\n\n\n");

			/* Free internal ELF */
			si_bin_file_free(amd_bin);
		}
	}

	/* Free external ELF */
	elf_file_free(elf_file);
}


void SIAsmDisassembleOpenGLBinary(SIAsm *self, char *path, int shader_index)
{
	struct opengl_si_program_binary_t *si_program_bin;
	struct opengl_si_shader_binary_t *si_shader;
	void *file_buffer;
	int file_size;

	/* Load file into memory buffer */
	file_buffer = read_buffer(path, &file_size);
	if(!file_buffer)
		fatal("%s:Invalid file!", path);

	/* Analyze the file and initialize structure */	
	si_program_bin = opengl_si_program_binary_create(file_buffer, file_size, path);
	free_buffer(file_buffer);

	/* Basic info of the shader binary */
	printf("This program binary contains %d shaders\n\n", 
		list_count(si_program_bin->shader_bins));
	if (shader_index > list_count(si_program_bin->shader_bins) ||
			shader_index <= 0 )
	{
		fatal("Shader index out of range! Please choose <index> "
			"from 1 ~ %d", list_count(si_program_bin->shader_bins));
	}

	/* Disassemble */
	si_shader = list_get(si_program_bin->shader_bins, 
			shader_index - 1);
	printf("**\n** Disassembly for shader %d\n**\n\n", shader_index);
	SIAsmDisassembleBuffer(self, si_shader->isa->ptr,
			si_shader->isa->size, stdout);
	printf("\n\n\n");

	/* Free */
	opengl_si_program_binary_free(si_program_bin);
}

