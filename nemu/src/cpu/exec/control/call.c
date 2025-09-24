#include "cpu/exec/helper.h"
 
// 定义DATA_BYTE为1，生成处理1字节的数据的call指令函数
#define DATA_BYTE 1
#include "call-template.h"
#undef DATA_BYTE
 
// 定义DATA_BYTE为2，生成处理2字节的数据的call指令函数
#define DATA_BYTE 2
#include "call-template.h"
#undef DATA_BYTE
 
// 定义DATA_BYTE为4，生成处理4字节的数据的call指令函数
#define DATA_BYTE 4
#include "call-template.h"
#undef DATA_BYTE
 
//定义重载函数_v来处理不同数据长度的指令
make_helper_v(call_i)
make_helper_v(call_rm)