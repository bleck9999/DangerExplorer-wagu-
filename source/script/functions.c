#include "types.h"
#include "args.h"
#include "variables.h"
#include <string.h>
#include "../gfx/gfx.h"
#include <mem/heap.h>
#include "lexer.h"

#include <storage/nx_sd.h>
#include "../fs/fsutils.h"
#include "../gfx/gfxutils.h"
#include "../hid/hid.h"
#include <utils/util.h>
#include "../fs/fscopy.h"
#include "../storage/mountmanager.h"
#include "../storage/emummc.h"
#include "../fs/readers/folderReader.h"
#include "../utils/utils.h"
#include "../keys/keys.h"
#include "../storage/emmcfile.h"
#include "../keys/nca.h"
#include "../keys/save.h"
#include "../tegraexplorer/tconf.h"

#define scriptFunction(name) Variable_t name(scriptCtx_t *ctx, Variable_t *vars, u32 varLen)

#define varInt(i) newVar(IntType, 0, i)
#define varStr(s) newVar(StringType, 1, .stringType = s)

scriptFunction(funcIf) {
	setCurIndentInstruction(ctx, (vars[0].integerType == 0), 0, -1);
	return NullVar;
}

scriptFunction(funcPrint) {
	for (u32 i = 0; i < varLen; i++) {
		if (vars[i].varType == IntType)
			gfx_printf("%d", vars[i].integerType);
		else if (vars[i].varType == StringType)
			gfx_printf("%s", vars[i].stringType);
		else if (vars[i].varType == IntArrayType) {
			gfx_printf("[");
			int* v = vecGetArray(int*, vars[i].vectorType);
			for (u32 j = 0; j < vars[i].vectorType.count; j++)
				gfx_printf((j + 1 == vars[i].vectorType.count) ? "%d" : "%d, ", v[j]);
			gfx_printf("]");
		}
	}

	return NullVar;
}

scriptFunction(funcPrintln) {
	funcPrint(ctx, vars, varLen);
	gfx_printf("\n");
	return NullVar;
}

scriptFunction(funcWhile) {
	setCurIndentInstruction(ctx, (vars[0].integerType == 0), 0, ctx->startEquation);
	
	return NullVar;
}

scriptFunction(funcElse) {
	indentInstructor_t* curInstruction = getCurIndentInstruction(ctx);
	setCurIndentInstruction(ctx, !curInstruction->skip, 0, -1);
	return NullVar;
}

scriptFunction(funcLen) {
	if (vars[0].varType >= IntArrayType && vars[0].varType <= ByteArrayType)
		return IntVal(vars[0].vectorType.count);
	else if (vars[0].varType == StringType) {
		if (vars[0].stringType != NULL)
			return IntVal(strlen(vars[0].stringType));
	}

	return ErrVar(ERRINVALIDTYPE);
}

scriptFunction(funcMakeByteArray){
	u8 *buff = malloc(vars[0].vectorType.count);
	vecDefArray(int*, entries, vars[0].vectorType);
	for (int i = 0; i < vars[0].vectorType.count; i++)
		buff[i] = (u8)(entries[i] & 0xFF);

	Vector_t v = vecFromArray(buff, vars[0].vectorType.count, sizeof(u8));
	return newVar(ByteArrayType, 1, .vectorType = v);
}

scriptFunction(funcSetPixel){
	u32 color = 0xFF000000 | ((vars[2].integerType & 0xFF) << 16) | ((vars[3].integerType & 0xFF) << 8) | (vars[4].integerType & 0xFF);
	gfx_set_pixel_horz(vars[0].integerType, vars[1].integerType, color);
	return NullVar;
}

scriptFunction(funcFileExists){
	return newVar(IntType, 0, FileExists(vars[0].stringType));
}

// Args: Str (Path)
scriptFunction(funcReadFile){
	u32 fSize = 0;
	u8 *buff = sd_file_read(vars[0].stringType, &fSize);
	if (buff == NULL)
		return ErrVar(ERRFATALFUNCFAIL);
	
	Vector_t vec = vecFromArray(buff, fSize, sizeof(u8));
	return newVar(ByteArrayType, 1, .vectorType = vec);
}

// Args: Str (Path), vector(Byte) (toWrite) 
scriptFunction(funcWriteFile){
	return newVar(IntType, 0, sd_save_to_file(vars[1].vectorType.data, vars[1].vectorType.count, vars[0].stringType));
}

// Args: vector(Byte)
scriptFunction(funcByteToStr){
	char *str = calloc(vars[0].vectorType.count + 1, 1);
	memcpy(str, vars[0].vectorType.data, vars[0].vectorType.count);
	return newVar(StringType, 1, .stringType = str);
}

scriptFunction(funcReturn){
	if (ctx->indentIndex > 0){
		vecDefArray(indentInstructor_t*, instructors, ctx->indentInstructors);
		for (int i = ctx->indentIndex - 1; i >= 0; i--){
			indentInstructor_t ins = instructors[i];
			if (ins.active && ins.jump && ins.function){
				ctx->curPos = ins.jumpLoc - 1;
				ins.active = 0;
				ctx->indentIndex = i;
				break;
			}
		}
	}

	return NullVar;
}

scriptFunction(funcExit){
	return ErrVar(ERRESCSCRIPT);
}

/*
scriptFunction(funcContinue){
	if (ctx->indentIndex > 0){
		vecDefArray(indentInstructor_t*, instructors, ctx->indentInstructors);
		for (int i = ctx->indentIndex - 1; i >= 0; i--){
			indentInstructor_t ins = instructors[i];
			if (ins.active && ins.jump && !ins.function){
				ctx->curPos = ins.jumpLoc - 1;
				ins.active = 0;
				ctx->indentIndex = i;
				break;
			}
		}
	}

	return NullVar;
}
*/

// Args: Int, Int
scriptFunction(funcSetPrintPos){
	if (vars[0].integerType > 78 || vars[0].integerType < 0 || vars[1].integerType > 42 || vars[1].integerType < 0)
		return ErrVar(ERRFATALFUNCFAIL);
	
	gfx_con_setpos(vars[0].integerType * 16, vars[1].integerType * 16);
	return NullVar;
}

scriptFunction(funcClearScreen){
	gfx_clearscreen();
	return NullVar;
}

int validRanges[] = {
	1279,
	719,
	1279,
	719
};

// Args: Int, Int, Int, Int, Int
scriptFunction(funcDrawBox){
	for (int i = 0; i < ARR_LEN(validRanges); i++){
		if (vars[i].integerType > validRanges[i] || vars[i].integerType < 0)
			return ErrVar(ERRFATALFUNCFAIL);
	}

	gfx_box(vars[0].integerType, vars[1].integerType, vars[2].integerType, vars[3].integerType, vars[4].integerType);
	return NullVar;
}

typedef struct {
	char *name;
	u32 color;
} ColorCombo_t;

ColorCombo_t combos[] = {
	{"RED", COLOR_RED},
	{"ORANGE", COLOR_ORANGE},
	{"YELLOW", COLOR_YELLOW},
	{"GREEN", COLOR_GREEN},
	{"BLUE", COLOR_BLUE},
	{"VIOLET", COLOR_VIOLET},
	{"GREY", COLOR_GREY},
};

u32 GetColor(char *color){
	for (int i = 0; i < ARR_LEN(combos); i++){
		if (!strcmp(combos[i].name, color)){
			return combos[i].color;
		}
	}

	return COLOR_WHITE;
}

// Args: Str
scriptFunction(funcSetColor){
	SETCOLOR(GetColor(vars[0].stringType), COLOR_DEFAULT);
	return NullVar;
}

scriptFunction(funcPause){
	return newVar(IntType, 0, hidWait()->buttons);
}

// Args: Int
scriptFunction(funcWait){
	/*u32 timer = get_tmr_ms();
	while (timer + vars[0].integerType > get_tmr_ms()){
		gfx_printf("<Wait %d seconds> \r", (vars[0].integerType - (get_tmr_ms() - timer)) / 1000);
		hidRead();
	}*/
	return NullVar;
}

scriptFunction(funcGetVer){
	int *arr = malloc(3 * sizeof(int));
	arr[0] = LP_VER_MJ;
	arr[1] = LP_VER_MN;
	arr[2] = LP_VER_BF;
	Vector_t res = vecFromArray(arr, 3, sizeof(int));
	return newVar(IntArrayType, 1, .vectorType = res);
}


// Args: vec(str), int, (optional) vec(str)
scriptFunction(funcMakeMenu){
	if (varLen == 3 && vars[2].vectorType.count != vars[0].vectorType.count)
		return ErrVar(ERRSYNTAX);

	Vector_t menuEntries = newVec(sizeof(MenuEntry_t), vars[0].vectorType.count);
	vecDefArray(char**, names, vars[0].vectorType);
	char **colors;
	if (varLen >= 3)
		colors = vecGetArray(char**, vars[2].vectorType);

	int *options;
	if (varLen == 4)
		options = vecGetArray(int*, vars[3].vectorType);
	
	for (int i = 0; i < vars[0].vectorType.count; i++){
		u32 color = COLORTORGB(((varLen >= 3) ? GetColor(colors[i]) : COLOR_WHITE));
		MenuEntry_t a = {.optionUnion = color, .name = names[i]};

		if (varLen == 4){
			a.skip = (options[i] & 1) ? 1 : 0;
			a.hide = (options[i] & 2) ? 1 : 0;
		}

		vecAddElem(&menuEntries, a);
	}

	int res = newMenu(&menuEntries, vars[1].integerType, 78, 10, ENABLEB | ALWAYSREDRAW, menuEntries.count);
	vecFree(menuEntries);
	return varInt(res);
}

//  Args: Str, Str
scriptFunction(funcCombinePath){
	if (varLen <= 1)
		return NullVar;
	
	for (int i = 0; i < varLen; i++){
		if (vars[i].varType != StringType)
			return ErrVar(ERRINVALIDTYPE);
	}

	char *res = CpyStr(vars[0].stringType);

	for (int i = 1; i < varLen; i++){
		char *temp = CombinePaths(res, vars[i].stringType);
		free(res);
		res = temp;
	}

	return varStr(res);
}

// Args: Str
scriptFunction(funcEscFolder){
	return newVar(StringType, 1, .stringType = EscapeFolder(vars[0].stringType));
}

//  Args: Str, Str
scriptFunction(funcFileMove){
	return varInt(f_rename(vars[0].stringType, vars[1].stringType));
}

//  Args: Str, Str
scriptFunction(funcFileCopy){
	return varInt(FileCopy(vars[0].stringType, vars[1].stringType, COPY_MODE_PRINT).err);
}

// Args: Str
scriptFunction(funcMmcConnect){
	int res = 0;
	if (!strcmp(vars[0].stringType, "SYSMMC"))
		res = connectMMC(MMC_CONN_EMMC);
	else if (!strcmp(vars[0].stringType, "EMUMMC") && emu_cfg.enabled)
		res = connectMMC(MMC_CONN_EMUMMC);
	else
		return ErrVar(ERRFATALFUNCFAIL);
	
	return varInt(res);
}

// Args: Str
scriptFunction(funcMmcMount){
	if (!TConf.keysDumped)
		return ErrVar(ERRFATALFUNCFAIL);

	return varInt((mountMMCPart(vars[0].stringType).err));
}

// Args: Str
scriptFunction(funcMkdir){
	return varInt((f_mkdir(vars[0].stringType)));
}

// Args: Str
scriptFunction(funcReadDir){
	int res = 0;
	Vector_t files = ReadFolder(vars[0].stringType, &res);
	if (res){
		clearFileVector(&files);
		return ErrVar(ERRFATALFUNCFAIL);
	}

	vecDefArray(FSEntry_t*, fsEntries, files);
	
	Vector_t fileNames = newVec(sizeof(char*), files.count);
	Vector_t fileProperties = newVec(sizeof(int), files.count);

	for (int i = 0; i < files.count; i++){
		vecAddElem(&fileNames, fsEntries[i].name);
		int isFolder = fsEntries[i].isDir;
		vecAddElem(&fileProperties, isFolder);
	}

	vecFree(files);

	dictVectorAdd(&ctx->varDict, newDict(CpyStr("fileProperties"), (newVar(IntArrayType, 1, .vectorType = fileProperties))));

	return newVar(StringArrayType, 1, .vectorType = fileNames);
}

// Args: Str, Str
scriptFunction(funcCopyDir){
	return varInt((FolderCopy(vars[0].stringType, vars[1].stringType).err));
}

// Args: Str
scriptFunction(funcDelDir){
	return varInt((FolderDelete(vars[0].stringType).err));
}

// Args: Str
scriptFunction(funcDelFile){
	return varInt((f_unlink(vars[0].stringType)));
}

// Args: Str, Str
scriptFunction(funcMmcDump){
	return varInt((DumpOrWriteEmmcPart(vars[0].stringType, vars[1].stringType, 0, 1).err));
}

// Args: Str, Str, Int
scriptFunction(funcMmcRestore){
	return varInt((DumpOrWriteEmmcPart(vars[0].stringType, vars[1].stringType, 1, vars[2].integerType).err));
}

// Args: Str
scriptFunction(funcGetNcaType){
	if (!TConf.keysDumped)
		return ErrVar(ERRFATALFUNCFAIL);

	return varInt((GetNcaType(vars[0].stringType)));
}

// Args: Str
scriptFunction(funcSignSave){
	if (!TConf.keysDumped)
		return ErrVar(ERRFATALFUNCFAIL);

	return varInt((saveCommit(vars[0].stringType).err));
}

scriptFunction(funcGetMs){
	return varInt(get_tmr_ms());
}

extern int launch_payload(char *path);

// Args: Str
scriptFunction(funcLaunchPayload){
	return varInt(launch_payload(vars[0].stringType));
}

u8 fiveInts[] = {IntType, IntType, IntType, IntType, IntType};
u8 singleIntArray[] = { IntArrayType };
u8 singleInt[] = { IntType };
u8 singleAny[] = { varArgs };
u8 singleStr[] = { StringType };
u8 singleByteArr[] = { ByteArrayType };
u8 StrByteVec[] = { StringType, ByteArrayType};
u8 MenuArgs[] = { StringArrayType, IntType, StringArrayType, IntArrayType};
u8 twoStrings[] = { StringType, StringType };
u8 mmcReadWrite[] = { StringType, StringType, IntType};

functionStruct_t scriptFunctions[] = {
	{"if", funcIf, 1, singleInt},
	{"print", funcPrint, varArgs, NULL},
	{"println", funcPrintln, varArgs, NULL},
	{"while", funcWhile, 1, singleInt},
	{"else", funcElse, 0, NULL},
	{"len", funcLen, 1, singleAny},
	{"byte", funcMakeByteArray, 1, singleIntArray},
	{"setPixel", funcSetPixel, 5, fiveInts},
	{"fileRead", funcReadFile, 1, singleStr},
	{"fileWrite", funcWriteFile, 2, StrByteVec},
	{"fileExists", funcFileExists, 1, singleStr},
	{"bytesToStr", funcByteToStr, 1, singleByteArr},
	{"return", funcReturn, 0, NULL},
	{"exit", funcExit, 0, NULL},
	//{"continue", funcContinue, 0, NULL},
	{"printPos", funcSetPrintPos, 2, fiveInts},
	{"clearscreen", funcClearScreen, 0, NULL},
	{"drawBox", funcDrawBox, 5, fiveInts},
	{"color", funcSetColor, 1, singleStr},
	{"pause", funcPause, 0, NULL},
	{"wait", funcWait, 1, singleInt},
	{"version", funcGetVer, 0, NULL},
	{"menu", funcMakeMenu, 2, MenuArgs}, // for the optional arg
	{"menu", funcMakeMenu, 3, MenuArgs},
	{"menu", funcMakeMenu, 4, MenuArgs},
	{"pathCombine", funcCombinePath, varArgs, NULL},
	{"pathEscFolder", funcEscFolder, 1, singleStr},
	{"fileMove", funcFileMove, 2, twoStrings},
	{"fileCopy", funcFileCopy, 2, twoStrings},
	{"fileDel", funcDelFile, 1, singleStr},
	{"mmcConnect", funcMmcConnect, 1, singleStr},
	{"mmcMount", funcMmcMount, 1, singleStr},
	{"mkdir", funcMkdir, 1, singleStr},
	{"dirRead", funcReadDir, 1, singleStr},
	{"dirCopy", funcCopyDir, 2, twoStrings},
	{"dirDel", funcDelDir, 1, singleStr},
	{"mmcDump", funcMmcDump, 2, mmcReadWrite},
	{"mmcRestore", funcMmcRestore, 3, mmcReadWrite},
	{"ncaGetType", funcGetNcaType, 1, singleStr},
	{"saveSign", funcSignSave, 1, singleStr},
	{"timerMs", funcGetMs, 0, NULL},
	{"launchPayload", funcLaunchPayload, 1, singleStr},
	// Left from old: keyboard(?)
};

Variable_t executeFunction(scriptCtx_t* ctx, char* func_name, lexarToken_t *start, u32 len) {
	Vector_t args = { 0 };

	if (len > 0) {
		args = extractVars(ctx, start, len);
		Variable_t* vars = vecGetArray(Variable_t*, args);
		for (int i = 0; i < args.count; i++) {
			if (vars[i].varType == ErrType)
				return vars[i];
		}
	}

	Variable_t* vars = vecGetArray(Variable_t*, args);

	for (u32 i = 0; i < ARRAY_SIZE(scriptFunctions); i++) {
		if (scriptFunctions[i].argCount == args.count || scriptFunctions[i].argCount == varArgs) {
			if (!strcmp(scriptFunctions[i].key, func_name)) {
				if (scriptFunctions[i].argCount != varArgs && scriptFunctions[i].argCount != 0) {
					u8 argsMatch = 1;
					for (u32 j = 0; j < args.count; j++) {
						if (vars[j].varType != scriptFunctions[i].typeArray[j] && scriptFunctions[i].typeArray[j] != varArgs) {
							argsMatch = 0;
							break;
						}
					}

					if (!argsMatch)
						continue;
				}

				Variable_t ret = scriptFunctions[i].value(ctx, vars, args.count);
				freeVariableVector(&args);
				return ret;
			}
		}
	}

	Variable_t* var = dictVectorFind(&ctx->varDict, func_name);
	if (var != NULL) {
		if (var->varType == JumpType) {
			setCurIndentInstruction(ctx, 0, 1, ctx->curPos);
			ctx->curPos = var->integerType - 1;
			return NullVar;
		}
	}

	return ErrVar(ERRNOFUNC);
}
