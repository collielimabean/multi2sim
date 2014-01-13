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

#include <arch/kepler/emu/Emu.h>
#include <arch/kepler/emu/Grid.h>

#include <arch/fermi/emu/emu.h>
#include <arch/fermi/emu/grid.h>
#include <arch/x86/emu/context.h>
#include <arch/x86/emu/emu.h>
#include <arch/x86/emu/regs.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/list.h>
#include <lib/util/string.h>
#include <mem-system/memory.h>

#include "cuda.h"
#include "function.h"
#include "function-arg.h"
#include "module.h"


/*
 * Global Variables
 */

/* Debug */
int cuda_debug_category;

/* Error messages */
char *cuda_err_code = "\tAn invalid function code was generated by your\n"
		"\tapplication in a CUDA system call. Probably, this means that your\n"
		"\tapplication is using an incompatible version of the Multi2Sim CUDA\n"
		"\truntime/driver library ('libm2s-cuda'). Please recompile your\n"
		"\tapplication and try again.\n";

/* ABI call names */
char *cuda_call_name[cuda_call_count + 1] =
{
		NULL,
#define CUDA_DEFINE_CALL(name) #name,
#include "cuda.dat"
#undef CUDA_DEFINE_CALL
		NULL
};

/* Forward declarations of ABI calls */
#define CUDA_DEFINE_CALL(name) int cuda_func_##name(X86Context *context);
#include "cuda.dat"
#undef CUDA_DEFINE_CALL

/* ABI call table */
typedef int (*cuda_func_t)(X86Context *context);
cuda_func_t cuda_func_table[cuda_call_count + 1] =
{
		NULL,
#define CUDA_DEFINE_CALL(name) cuda_func_##name,
#include "cuda.dat"
#undef CUDA_DEFINE_CALL
		NULL
};

/* For CUDA launch */
struct cuda_abi_frm_kernel_launch_info_t
{
	struct cuda_function_t *function;
	X86Context *context;
	FrmGrid *grid;
	int finished;
};

struct cuda_abi_kpl_kernel_launch_info_t
{
	struct cuda_function_t *function;
	X86Context *context;
	KplGrid *grid;
	int finished;
};




/*
 * Class 'CudaDriver'
 */


//void CudaDriverCreate(CudaDriver *self, X86Emu *x86_emu, FrmEmu *frm_emu, KplEmu *kpl_emu)
void CudaDriverCreate(CudaDriver *self, X86Emu *x86_emu, FrmEmu *frm_emu)
{
	/* Parent */
	DriverCreate(asDriver(self), x86_emu);

	/* Initialize */
	self->frm_emu = frm_emu;
	//self->kpl_emu = kpl_emu;

	/* Assign driver to host emulator */
	x86_emu->cuda_driver = self;
}

void CudaDriverDestroy(CudaDriver *self)
{
	int i;
	struct cuda_module_t *module;
	struct cuda_function_t *function;

	if (! module_list)
		return;

	LIST_FOR_EACH(module_list, i)
	{
		module = list_get(module_list, i);
		if (module)
			cuda_module_free(module);
	}
	list_free(module_list);

	if (!function_list)
		return;

	LIST_FOR_EACH(function_list, i)
	{
		function = list_get(function_list, i);
		if (function)
			cuda_function_free(function);
	}
	list_free(function_list);
}




/*
 * Public
 */

int CudaDriverCall(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;

	int code;
	int ret;

	/* Function code */
	code = regs->ebx;
	if (code <= cuda_call_invalid || code >= cuda_call_count)
		fatal("%s: invalid CUDA function (code %d).\n%s",
				__FUNCTION__, code, cuda_err_code);

	/* Debug */
	cuda_debug("CUDA call '%s' (code %d)\n", cuda_call_name[code], code);

	/* Call */
	assert(cuda_func_table[code]);
	ret = cuda_func_table[code](ctx);

	return ret;
}




/*
 * CUDA call - versionCheck
 *
 * @param struct cuda_version_t *version;
 *	Structure where the version of the CUDA driver implementation will be
 *	dumped. To succeed, the major version should match in the driver library
 *	(guest) and driver implementation (host), whereas the minor version should
 *	be equal or higher in the implementation (host).
 *
 *	Features should be added to the CUDA driver (guest and host) using the
 *	following rules:
 *	1) If the guest library requires a new feature from the host
 *	   implementation, the feature is added to the host, and the minor
 *	   version is updated to the current Multi2Sim SVN revision both in
 *	   host and guest.
 *     All previous services provided by the host should remain available
 *     and backward-compatible. Executing a newer library on the older
 *     simulator will fail, but an older library on the newer simulator
 *     will succeed.
 *  2) If a new feature is added that affects older services of the host
 *     implementation breaking backward compatibility, the major version is
 *     increased by 1 in the host and guest code.
 *     Executing a library with a different (lower or higher) major version
 *     than the host implementation will fail.
 *
 * @return
 *	The return value is always 0.
 */

int cuda_func_versionCheck(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	struct cuda_version_t version;

	version.major = CUDA_VERSION_MAJOR;
	version.minor = CUDA_VERSION_MINOR;

	cuda_debug("\tout: version.major=%d\n", version.major);
	cuda_debug("\tout: version.minor=%d\n", version.minor);

	mem_write(mem, regs->ecx, sizeof version, &version);

	return 0;
}




/*
 * CUDA call - cuInit
 *
 * @return
 * 	The return value is always 0 on success.
 */

int cuda_func_cuInit(X86Context *ctx)
{
	/* Create module list */
	module_list = list_create();

	/* Create function list */
	function_list = list_create();

	return 0;
}




/*
 * CUDA call - cuDeviceTotalMem
 *
 * @param unsigned total_global_mem_size;
 *  Total global memory size of the emulator.
 *
 * @return
 *	The return value is always 0 on success.
 */

int cuda_func_cuDeviceTotalMem(X86Context *ctx)
{
	struct mem_t *mem = ctx->mem;

	unsigned total_global_mem_size;
	FrmEmu *frm_emu = ctx->emu->cuda_driver->frm_emu;

	total_global_mem_size = ctx->regs->ecx;

	cuda_debug("\tout: total=%u\n", frm_emu->total_global_mem_size);

	mem_write(mem, total_global_mem_size, sizeof total_global_mem_size,
			&(frm_emu->total_global_mem_size));

	return 0;
}




/*
 * CUDA call - cuModuleLoad
 *
 * @param char cubin_path[];
 *  Path of the CUBIN to load.
 *
 * @return
 *	The return value is always 0 on success.
 */

int cuda_func_cuModuleLoad(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	char cubin_path[MAX_STRING_SIZE];

	/* Get kernel binary */
	mem_read(mem, regs->ecx, sizeof cubin_path, cubin_path);

	/* Create module */
	cuda_module_create(cubin_path);

	return 0;
}




/*
 * CUDA call - cuModuleUnload
 *
 * @param unsigned module_id;
 *  ID of the module to unload.
 *
 * @return
 *	The return value is always 0 on success.
 */

int cuda_func_cuModuleUnload(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;

	unsigned module_id;
	struct cuda_module_t *module;

	/* Get module */
	module_id = regs->ecx;
	module = list_get(module_list, module_id);

	/* Free module */
	cuda_module_free(module);

	return 0;
}




/*
 * CUDA call - cuModuleGetFunction
 *
 * @param unsigned module_id;
 *  ID of the module to retrieve function from.
 *
 * @param char func_name[];
 *  Name of function to retrieve.
 *
 * @return
 *	The return value is always 0 on success.
 */

int cuda_func_cuModuleGetFunction(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned module_id;
	char func_name[MAX_STRING_SIZE];
	struct cuda_module_t *module;

	module_id = regs->ecx;
	mem_read(mem, regs->edx, sizeof func_name, func_name);

	cuda_debug("\tin: module.id = 0x%08x\n", module_id);
	cuda_debug("\tin: function_name = %s\n", func_name);

	/* Get module */
	module = list_get(module_list, module_id);

	/* Create function */
	cuda_function_create(module, func_name);

	return 0;
}




/*
 * CUDA call - cuMemGetInfo
 *
 * @param unsigned free;
 *  Returned free global memory in bytes.
 *
 * @param unsigned total;
 *  Returned total global memory in bytes.
 *
 * @return
 *	The return value is always 0 on success.
 */

int cuda_func_cuMemGetInfo(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned free;
	unsigned total;
	FrmEmu *frm_emu = ctx->emu->cuda_driver->frm_emu;

	free = regs->ecx;
	total = regs->edx;

	cuda_debug("\tout: free=%u\n", frm_emu->free_global_mem_size);
	cuda_debug("\tout: total=%u\n", frm_emu->total_global_mem_size);

	mem_write(mem, free, sizeof free, &(frm_emu->free_global_mem_size));
	mem_write(mem, total, sizeof total, &(frm_emu->total_global_mem_size));

	return 0;
}




/*
 * CUDA call - cuFrmMemAlloc
 *
 * @param unsigned dev_mem_ptr;
 *  Returned device pointer.
 *
 * @param unsigned dev_mem_size;
 *  Requested allocation size in bytes.
 *
 * @return
 *	The return value is always 0 on success.
 */
int cuda_func_cuFrmMemAlloc(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned dev_mem_ptr;
	unsigned dev_mem_size;
	FrmEmu *frm_emu = ctx->emu->cuda_driver->frm_emu;

	dev_mem_ptr = regs->ecx;
	dev_mem_size = regs->edx;

	cuda_debug("\tin: dev_mem_size=%u\n", dev_mem_size);

	mem_write(mem, dev_mem_ptr, sizeof(unsigned), &(frm_emu->global_mem_top));
	frm_emu->free_global_mem_size -= dev_mem_size;

	cuda_debug("\tout: dev_mem_ptr=0x%08x\n", frm_emu->global_mem_top);

	frm_emu->global_mem_top += dev_mem_size;

	return 0;
}




/*
 * CUDA call - cuKplMemAlloc
 *
 * @param unsigned dev_mem_ptr;
 *  Returned device pointer.
 *
 * @param unsigned dev_mem_size;
 *  Requested allocation size in bytes.
 *
 * @return
 *	The return value is always 0 on success.
 */
int cuda_func_cuKplMemAlloc(X86Context *ctx)
{

	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned dev_mem_ptr;
	unsigned dev_mem_size;
	KplEmu *kpl_emu = ctx->emu->cuda_driver->kpl_emu;

	dev_mem_ptr = regs->ecx;
	dev_mem_size = regs->edx;

	cuda_debug("\tin: dev_mem_size=%u\n", dev_mem_size);

	mem_write(mem, dev_mem_ptr, sizeof(unsigned), &(kpl_emu->global_mem_top));
	kpl_emu->free_global_mem_size -= dev_mem_size;

	cuda_debug("\tout: dev_mem_ptr=0x%08x\n", kpl_emu->global_mem_top);

	kpl_emu->global_mem_top += dev_mem_size;

	return 0;
}




/*
 * CUDA call - cuMemFree
 *
 * @param unsigned dev_mem_ptr;
 *  Pointer to global memory to free.
 *
 * @return
 *	The return value is always 0 on success.
 */

int cuda_func_cuMemFree(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;

	unsigned dev_mem_ptr;

	dev_mem_ptr = regs->ecx;

	cuda_debug("\tin: dev_mem_ptr=0x%08x\n", dev_mem_ptr);

	return 0;
}




/*
 * CUDA call - cuFrmMemcpyHtoD
 *
 * @param unsigned dev_mem_ptr;
 *  Pointer of destination device memory.
 *
 * @param unsigned host_mem_ptr;
 *  Pointer of source host memory.
 *
 * @param unsigned size;
 *  Size of memory copy in bytes.
 *
 * @return
 *	The return value is always 0 on success.
 */
int cuda_func_cuFrmMemcpyHtoD(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned dev_mem_ptr;
	unsigned host_mem_ptr;
	unsigned size;
	void *buf;
	FrmEmu *frm_emu = ctx->emu->cuda_driver->frm_emu;

	dev_mem_ptr = regs->ecx;
	host_mem_ptr = regs->edx;
	size = regs->esi;

	cuda_debug("\tin: dev_mem_ptr=0x%08x\n", dev_mem_ptr);
	cuda_debug("\tin: host_mem_ptr=0x%08x\n", host_mem_ptr);
	cuda_debug("\tin: size=%u\n", size);

	/* Copy */
	buf = xcalloc(1, size);
	mem_read(mem, host_mem_ptr, size, buf);
	mem_write(frm_emu->global_mem, dev_mem_ptr, size, buf);
	free(buf);

	return 0;
}




/*
 * CUDA call - cuKplMemcpyHtoD
 *
 * @param unsigned dev_mem_ptr;
 *  Pointer of destination device memory.
 *
 * @param unsigned host_mem_ptr;
 *  Pointer of source host memory.
 *
 * @param unsigned size;
 *  Size of memory copy in bytes.
 *
 * @return
 *	The return value is always 0 on success.
 */
int cuda_func_cuKplMemcpyHtoD(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned dev_mem_ptr;
	unsigned host_mem_ptr;
	unsigned size;
	void *buf;
	KplEmu *kpl_emu = ctx->emu->cuda_driver->kpl_emu;

	dev_mem_ptr = regs->ecx;
	host_mem_ptr = regs->edx;
	size = regs->esi;

	cuda_debug("\tin: dev_mem_ptr=0x%08x\n", dev_mem_ptr);
	cuda_debug("\tin: host_mem_ptr=0x%08x\n", host_mem_ptr);
	cuda_debug("\tin: size=%u\n", size);

	/* Copy */
	buf = xcalloc(1, size);
	mem_read(mem, host_mem_ptr, size, buf);
	mem_write(kpl_emu->global_mem, dev_mem_ptr, size, buf);
	free(buf);

	return 0;
}




/*
 * CUDA call - cuFrmMemcpyDtoH
 *
 * @param unsigned host_mem_ptr;
 *  Destination host pointer.
 *
 * @param unsigned dev_mem_ptr;
 *  Source device pointer.
 *
 * @param unsigned size;
 *  Size of memory copy in bytes.
 *
 * @return
 *	The return value is always 0 on success.
 */
int cuda_func_cuFrmMemcpyDtoH(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned host_mem_ptr;
	unsigned dev_mem_ptr;
	unsigned size;
	void *buf;
	FrmEmu *frm_emu = ctx->emu->cuda_driver->frm_emu;

	host_mem_ptr = regs->ecx;
	dev_mem_ptr = regs->edx;
	size = regs->esi;

	cuda_debug("\tin: host_mem_ptr=0x%08x\n", host_mem_ptr);
	cuda_debug("\tin: dev_mem_ptr=0x%08x\n", dev_mem_ptr);
	cuda_debug("\tin: size=%u\n", size);

	/* Copy */
	buf = xcalloc(1, size);
	mem_read(frm_emu->global_mem, dev_mem_ptr, size, buf);
	mem_write(mem, host_mem_ptr, size, buf);
	free(buf);

	return 0;
}




/*
 * CUDA call - cuKplMemcpyDtoH
 *
 * @param unsigned host_mem_ptr;
 *  Destination host pointer.
 *
 * @param unsigned dev_mem_ptr;
 *  Source device pointer.
 *
 * @param unsigned size;
 *  Size of memory copy in bytes.
 *
 * @return
 *	The return value is always 0 on success.
 */
int cuda_func_cuKplMemcpyDtoH(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned host_mem_ptr;
	unsigned dev_mem_ptr;
	unsigned size;
	void *buf;
	KplEmu *kpl_emu = ctx->emu->cuda_driver->kpl_emu;

	host_mem_ptr = regs->ecx;
	dev_mem_ptr = regs->edx;
	size = regs->esi;

	cuda_debug("\tin: host_mem_ptr=0x%08x\n", host_mem_ptr);
	cuda_debug("\tin: dev_mem_ptr=0x%08x\n", dev_mem_ptr);
	cuda_debug("\tin: size=%u\n", size);

	/* Copy */
	buf = xcalloc(1, size);
	mem_read(kpl_emu->global_mem, dev_mem_ptr, size, buf);
	mem_write(mem, host_mem_ptr, size, buf);
	free(buf);

	return 0;
}




/*
 * CUDA call - cuFrmLaunchKernel
 *
 * @param unsigned function_id;
 *  ID of the function to launch.
 *
 * @param unsigned grid_dim[3];
 *  Dimensions of the grid in blocks.
 *
 * @param unsigned block_dim[3];
 *  Dimensions of the thread-block.
 *
 * @param unsigned shared_mem_usage;
 *  Size of the shared memory per thread-block in bytes.
 *
 * @param unsigned stream_handle;
 *  Handle of the stream.
 *
 * @param unsigned kernel_args;
 *  Array of pointers to kernel parameters.
 *
 * @param unsigned extra;
 *  Extra kernel parameters.
 *
 * @return
 *	The return value is always 0 on success.
 */

void frm_grid_set_free_notify_func(FrmGrid *grid, void (*func)(void *),
		void *user_data)
{
	grid->free_notify_func = func;
	grid->free_notify_data = user_data;
}

static void cuda_abi_frm_kernel_launch_finish(void *user_data)
{
	struct cuda_abi_frm_kernel_launch_info_t *info = user_data;
	struct cuda_function_t *kernel = info->function;

	X86Context *ctx = info->context;
	FrmGrid *grid = info->grid;

	/* Debug */
	cuda_debug("Grid %d running kernel '%s' finished\n",
			grid->id, kernel->name);

	/* Set 'finished' flag in launch info */
	info->finished = 1;

	/* Force the x86 emulator to check which suspended contexts can wakeup,
	 * based on their new state. */
	X86EmuProcessEventsSchedule(ctx->emu);
}

static int cuda_abi_frm_kernel_launch_can_wakeup(X86Context *ctx,
		void *user_data)
{
	struct cuda_abi_frm_kernel_launch_info_t *info = user_data;

	/* NOTE: the grid has been freed at this point if it finished
	 * execution, so field 'info->grid' should not be accessed. We
	 * use flag 'info->finished' instead. */
	return info->finished;
}

static void cuda_abi_frm_kernel_launch_wakeup(X86Context *ctx,
		void *user_data)
{
	struct cuda_abi_frm_kernel_launch_info_t *info = user_data;

	/* Free info object */
	free(info);
}

int cuda_func_cuFrmLaunchKernel(X86Context *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned args[11];
	unsigned function_id;
	unsigned grid_dim[3];
	unsigned block_dim[3];
	unsigned shared_mem_usage;
	unsigned stream_handle;
	unsigned kernel_args;
	unsigned extra;

	struct cuda_function_t *function;
	int i;
	struct cuda_function_arg_t *arg;
	unsigned arg_ptr;
	int offset = 0x20;
	FrmGrid *grid;
	FrmEmu *frm_emu = ctx->emu->cuda_driver->frm_emu;
	struct cuda_abi_frm_kernel_launch_info_t *info;

	/* Read arguments */
	mem_read(mem, regs->ecx, 11 * sizeof *args, args);
	function_id = args[0];
	grid_dim[0] = args[1];
	grid_dim[1] = args[2];
	grid_dim[2] = args[3];
	block_dim[0] = args[4];
	block_dim[1] = args[5];
	block_dim[2] = args[6];
	shared_mem_usage = args[7];
	stream_handle = args[8];
	kernel_args = args[9];
	extra = args[10];

	/* Debug */
	cuda_debug("\tfunction_id = 0x%08x\n", function_id);
	cuda_debug("\tgrid_dimX = %u\n", grid_dim[0]);
	cuda_debug("\tgrid_dimY = %u\n", grid_dim[1]);
	cuda_debug("\tgrid_dimZ = %u\n", grid_dim[2]);
	cuda_debug("\tblock_dimX = %u\n", block_dim[0]);
	cuda_debug("\tblock_dimY = %u\n", block_dim[1]);
	cuda_debug("\tblock_dimZ = %u\n", block_dim[2]);
	cuda_debug("\tshared_mem_usage = %u\n", shared_mem_usage);
	cuda_debug("\tstream_handle = 0x%08x\n", stream_handle);
	cuda_debug("\tkernel_args = 0x%08x\n", kernel_args);
	cuda_debug("\textra = 0x%08x\n", extra);

	/* Get function */
	function = list_get(function_list, function_id);

	/* Set up arguments */
	for (i = 0; i < function->arg_count; ++i)
	{
		arg = function->arg_array[i];
		mem_read(mem, kernel_args + i * 4, sizeof(unsigned), &arg_ptr);
		mem_read(mem, arg_ptr, sizeof(unsigned), &(arg->value));
		FrmEmuConstMemWrite(frm_emu, offset, &(arg->value));
		offset += 0x4;
	}

	/* Create grid */
	grid = new(FrmGrid, frm_emu, function);

	/* Set up grid */
	FrmGridSetupSize(grid, grid_dim, block_dim);
	FrmGridSetupConstantMemory(grid);

	/* Add to pending list */
	list_add(frm_emu->pending_grids, grid);

	/* Set up call-back function to be run when grid finishes */
	info = xcalloc(1, sizeof(struct cuda_abi_frm_kernel_launch_info_t));
	info->function= function;
	info->context = ctx;
	info->grid = grid;
	frm_grid_set_free_notify_func(grid, cuda_abi_frm_kernel_launch_finish,
			info);

	/* Suspend x86 context until grid finishes */
	X86ContextSuspend(ctx, cuda_abi_frm_kernel_launch_can_wakeup, info,
			cuda_abi_frm_kernel_launch_wakeup, info);

	return 0;
}




/*
 * CUDA call - cuKplLaunchKernel
 *
 * @param unsigned function_id;
 *  ID of the function to launch.
 *
 * @param unsigned grid_dim[3];
 *  Dimensions of the grid in blocks.
 *
 * @param unsigned block_dim[3];
 *  Dimensions of the thread-block.
 *
 * @param unsigned shared_mem_usage;
 *  Size of the shared memory per thread-block in bytes.
 *
 * @param unsigned stream_handle;
 *  Handle of the stream.
 *
 * @param unsigned kernel_args;
 *  Array of pointers to kernel parameters.
 *
 * @param unsigned extra;
 *  Extra kernel parameters.
 *
 * @return
 *	The return value is always 0 on success.
 */

//void kpl_grid_set_free_notify_func(KplGrid *grid, void (*func)(void *),
//		void *user_data)
//{
//	grid->free_notify_func = func;
//	grid->free_notify_data = user_data;
//}

//static void cuda_abi_kpl_kernel_launch_finish(void *user_data)
//{
//	struct cuda_abi_kpl_kernel_launch_info_t *info = user_data;
//	struct cuda_function_t *kernel = info->function;
//
//	X86Context *ctx = info->context;
//	KplGrid *grid = info->grid;
//
	/* Debug */
//	cuda_debug("Grid %d running kernel '%s' finished\n",
//			grid->id, kernel->name);

	/* Set 'finished' flag in launch info */
//	info->finished = 1;

	/* Force the x86 emulator to check which suspended contexts can wakeup,
	 * based on their new state. */
//	X86EmuProcessEventsSchedule(ctx->emu);
//}

//static int cuda_abi_kpl_kernel_launch_can_wakeup(X86Context *ctx,
//		void *user_data)
//{
//	struct cuda_abi_kpl_kernel_launch_info_t *info = user_data;

	/* NOTE: the grid has been freed at this point if it finished
	 * execution, so field 'info->grid' should not be accessed. We
	 * use flag 'info->finished' instead. */
//	return info->finished;
//}

//static void cuda_abi_kpl_kernel_launch_wakeup(X86Context *ctx,
//		void *user_data)
//{
//	struct cuda_abi_kpl_kernel_launch_info_t *info = user_data;

	/* Free info object */
//	free(info);
//}

int cuda_func_cuKplLaunchKernel(X86Context *ctx)
{
//	struct x86_regs_t *regs = ctx->regs;
//	struct mem_t *mem = ctx->mem;

//	unsigned args[11];
//	unsigned function_id;
//	unsigned grid_dim[3];
//	unsigned block_dim[3];
//	unsigned shared_mem_usage;
//	unsigned stream_handle;
//	unsigned kernel_args;
//	unsigned extra;

//	struct cuda_function_t *function;
//	int i;
//	struct cuda_function_arg_t *arg;
//	unsigned arg_ptr;
//	int offset = 0x20;
//	KplGrid *grid;
//	KplEmu *kpl_emu = ctx->emu->cuda_driver->kpl_emu;
//	struct cuda_abi_kpl_kernel_launch_info_t *info;

	/* Read arguments */
//	mem_read(mem, regs->ecx, 11 * sizeof *args, args);
//	function_id = args[0];
//	grid_dim[0] = args[1];
//	grid_dim[1] = args[2];
//	grid_dim[2] = args[3];
//	block_dim[0] = args[4];
//	block_dim[1] = args[5];
//	block_dim[2] = args[6];
//	shared_mem_usage = args[7];
//	stream_handle = args[8];
//	kernel_args = args[9];
//	extra = args[10];

	/* Debug */
//	cuda_debug("\tfunction_id = 0x%08x\n", function_id);
//	cuda_debug("\tgrid_dimX = %u\n", grid_dim[0]);
//	cuda_debug("\tgrid_dimY = %u\n", grid_dim[1]);
//	cuda_debug("\tgrid_dimZ = %u\n", grid_dim[2]);
//	cuda_debug("\tblock_dimX = %u\n", block_dim[0]);
//	cuda_debug("\tblock_dimY = %u\n", block_dim[1]);
//	cuda_debug("\tblock_dimZ = %u\n", block_dim[2]);
//	cuda_debug("\tshared_mem_usage = %u\n", shared_mem_usage);
//	cuda_debug("\tstream_handle = 0x%08x\n", stream_handle);
//	cuda_debug("\tkernel_args = 0x%08x\n", kernel_args);
//	cuda_debug("\textra = 0x%08x\n", extra);

	/* Get function */
//	function = list_get(function_list, function_id);

	/* Set up arguments */
//	for (i = 0; i < function->arg_count; ++i)
//	{
//		arg = function->arg_array[i];
//		mem_read(mem, kernel_args + i * 4, sizeof(unsigned), &arg_ptr);
//		mem_read(mem, arg_ptr, sizeof(unsigned), &(arg->value));
//		KplEmuConstMemWrite(kpl_emu, offset, &(arg->value));
//		offset += 0x4;
//	}

	/* Create grid */
//	grid = new(KplGrid, kpl_emu, function);

	/* Set up grid */
//	KplGridSetupSize(grid, grid_dim, block_dim);
//	KplGridSetupConstantMemory(grid);

	/* Add to pending list */
	//list_add(kpl_emu->pending_grids, grid);

	/* Set up call-back function to be run when grid finishes */
//	info = xcalloc(1, sizeof(struct cuda_abi_kpl_kernel_launch_info_t));
//	info->function= function;
//	info->context = ctx;
//	info->grid = grid;
//	kpl_grid_set_free_notify_func(grid, cuda_abi_kpl_kernel_launch_finish,
//		info);

	/* Suspend x86 context until grid finishes */
//	X86ContextSuspend(ctx, cuda_abi_kpl_kernel_launch_can_wakeup, info,
//			cuda_abi_kpl_kernel_launch_wakeup, info);

	return 0;
}

