#ifndef  __APP_CFG_H__
#define  __APP_CFG_H__


#define DAC_MUX      2
#define INDATA_MUX   3

#define  TASK_STK_SIZE                0x300       /* Size of each task's stacks (# of WORDs)            */
#define  N_TASKS                        5       /* Number of identical tasks                          */



//保留2-10号优先级给客户开发
///叠加测试使用的， 叠加测试，在不冲突的情况下，可以使用以下三个线程优先级
// #define TaskScriptPrio    					3//模式控制线程
#define TaskMusicPrio					5
#define TaskMusicPrio1					6



/*模式任务*/
#define TaskIdlePrio                    12  //Idle任务

#define TaskLineinEqPrio                11  //LINE IN sofe eq任务
#define TaskLineinDigPrio               17  //LINE IN digital任务

#define TaskRTCPrio                     12  //RTC任务

#define TaskPcCardPrio                  8  //PC读卡器

#define TaskFMsrcPrio                   12	//FM_DAC
#define TaskPCsrcPrio                   12	//PC_DAC

#define TaskEFOutPrio                   13

#define TaskMsPrio                      8 //魔音测试


//外挂flash录音先关线程
#define TaskEncodePrio                  6  //ENCODE APP
#define TaskEncRunPrio                  5  //ENCODE RUN
#define TaskEncWFPrio                   4  //ENCODE WRITE FILE


//混响先关处理线程
#define TaskEchoMicPrio                 8  //混响任务
#define TaskEchoRunPrio                 7  //混响处理


//解码相关的优先级
#define TaskMidiKeyboardPrio   			10//midi keyboard
#define TaskMidiAudioPrio   			11//midi解码线程
#define TaskMusicPhyDecPrio             12//解码线程



#define TaskRfPrio  					17
#define TaskMidiUpdate					17
#define TaskInteractPrio  				17


//叠加线程优先级
#define TaskMIXScriptPrio				9//在使能全局叠加的情况下，模式控制线程优先级必须要比所有的解码线程低

/*不可相同优先级的 高优先级任务*/
typedef enum {
    SecondPrio   	 = 14,
    TaskUIPrio   	 = 15,
    TaskScriptPrio   = 16,//模式控制线程
    TaskRcspPrio     = 17,
    TaskDevMountPrio = 18,
    TaskSbcPrio,
    TaskBtEscoPrio,
    TaskBtAecPrio,
    TaskBtStackPrio,
    TaskBTMSGPrio,
    TaskBtLmpPrio,
    TaskResoursrPrio,
    TaskPselPhyDecPrio,
    TaskPselPrio,
    TaskBtAecSDPrio,
    TaskMixPrio,
    TaskMainPrio,
    TaskDevCheckPrio,  //31   31为最大值
} SYS_TASK_PRIO;

/************ÉèÖÃÕ»´óÐ¡£¨µ¥Î»Îª OS_STK £©************/
#define START_TASK_STK_SIZE   1024
#define SECOND_TASK_STK_SIZE   512
#define MP3_TASK_STK_SIZE   0x4000
#define DAC_TASK_STK_SIZE   0x400

#define FILL_TASK_STK_SIZE   0x2000

#define THIRD_TASK_STK_SIZE   0x200
#define UI_TASK_STK_SIZE     0x1000
/*
struct task_req;

struct task_info {
	const char *name;
	void (*init)(void *priv);
	void (*exit)();
};

struct task_req {
	void (*exit_req)(const struct task_info *task);
	int (*switch_req)(const struct task_info *task, const char *name);
};

#define  TASK_REGISTER(task) \
	const struct task_info task __attribute__((section(".task_info")))
*/


#endif

