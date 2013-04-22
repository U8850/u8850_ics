/*
 * kernel/power/suspend.c - Suspend to RAM and standby functionality.
 *
 * Copyright (c) 2003 Patrick Mochel
 * Copyright (c) 2003 Open Source Development Lab
 * Copyright (c) 2009 Rafael J. Wysocki <rjw@sisk.pl>, Novell Inc.
 *
 * This file is released under the GPLv2.
 */

#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/syscalls.h>
#include <linux/gfp.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/suspend.h>

#include "power.h"

/* FIHTDC, Div2-SW2-BSP, Penho, packet filter { */
#ifdef CONFIG_FIH_PACKET_FILTER
/* FIH;Tiger;2009/12/10 { */
#include <linux/fs.h>
#include "../../../arch/arm/mach-msm/proc_comm.h"

void dis_packet_filter(void)
{
	unsigned char oem_cmd_buf[6] = {
		0x00, 0x04,
		0x01, 0x04};

	msm_proc_comm_oem_tcp_filter(oem_cmd_buf, 4);
}

void set_packet_filter(void)
{
	struct file *tcpFs = NULL;
	struct file *tcpFs6 = NULL;
	char tcpState[5];
	unsigned short tcpDestPort = 0;

	if(tcpFs==NULL)
	{
		tcpFs = filp_open("/proc/net/tcpFilter", O_RDONLY, 0);
		if(IS_ERR(tcpFs))
		{
			tcpFs = NULL;
			printk(KERN_ERR "Tiger> can't open /proc/net/tcpFilter\n");
		}
		else
		{
			tcpFs6 = filp_open("/proc/net/tcpFilter6", O_RDONLY, 0);
			if(IS_ERR(tcpFs6))
			{
				filp_close(tcpFs, NULL);
				tcpFs = NULL;
				tcpFs6 = NULL;
				printk(KERN_ERR "Tiger> can't open /proc/net/tcpFilter6\n");
			}
			else
			{
				unsigned char oem_cmd_buf[128];
				unsigned short *oem_ptr = (unsigned short *)oem_cmd_buf;
				unsigned short *port_start = NULL;
				int portCount = 0, cmdSize = 0;

				*oem_ptr = 0;
				oem_ptr ++;
				cmdSize ++;

				*oem_ptr = CLEAR_TABLE;
				oem_ptr ++;
				cmdSize ++;

				/* IPv4 */
				//tcpFs->f_op->llseek(tcpFs, 0, 0);
				while(tcpFs->f_op->read(tcpFs, (char *)&tcpState, 4, &tcpFs->f_pos) == 4)
				{
					tcpState[4] = 0;
					tcpDestPort = (tcpState[0]<='9') ? (tcpState[0]-'0')<<12 : (tcpState[0]-'A'+10)<<12;
					tcpDestPort += (tcpState[1]<='9') ? (tcpState[1]-'0')<<8 : (tcpState[1]-'A'+10)<<8;
					tcpDestPort += (tcpState[2]<='9') ? (tcpState[2]-'0')<<4 : (tcpState[2]-'A'+10)<<4;
					tcpDestPort += (tcpState[3]<='9') ? (tcpState[3]-'0') : (tcpState[3]-'A'+10);

					//printk(KERN_INFO "filter tcp destination port [%s] -> (%d)\n", tcpState, tcpDestPort);
					if(port_start == NULL)
					{
						port_start = oem_ptr;
						*oem_ptr = ADD_DEST_PORT;
						oem_ptr += 2;
						cmdSize += 2;
						*oem_ptr = tcpDestPort;
						oem_ptr ++;
						cmdSize ++;
						portCount = 1;
					}
					else
					{
						int i;
						// search dummy port
						for(i=0; i<portCount; i++)
						{
							if(*(port_start+i+2) == tcpDestPort)
							{
								break;
							}
						}

						if(i == portCount)
						{
							*oem_ptr = tcpDestPort;
							oem_ptr ++;
							cmdSize ++;
							portCount ++;
							if(portCount == 59) {
								oem_ptr = (unsigned short *)oem_cmd_buf;
								*oem_ptr = cmdSize<<1;
								*(port_start+1) = portCount;
								msm_proc_comm_oem_tcp_filter(oem_ptr, cmdSize<<1);

								*oem_ptr = 0;
								oem_ptr ++;
								cmdSize = 1;
								port_start = NULL;
								portCount = 0;
							}
						}
					}
				}

				/* IPv6 */
				//tcpFs6->f_op->llseek(tcpFs6, 0, 0);
				while(tcpFs6->f_op->read(tcpFs6, (char *)&tcpState, 4, &tcpFs6->f_pos) == 4)
				{
					tcpState[4] = 0;
					tcpDestPort = (tcpState[0]<='9') ? (tcpState[0]-'0')<<12 : (tcpState[0]-'A'+10)<<12;
					tcpDestPort += (tcpState[1]<='9') ? (tcpState[1]-'0')<<8 : (tcpState[1]-'A'+10)<<8;
					tcpDestPort += (tcpState[2]<='9') ? (tcpState[2]-'0')<<4 : (tcpState[2]-'A'+10)<<4;
					tcpDestPort += (tcpState[3]<='9') ? (tcpState[3]-'0') : (tcpState[3]-'A'+10);

					//printk(KERN_INFO "filter tcp6 destination port [%s] -> (%d)\n", tcpState, tcpDestPort);
					if(port_start == NULL)
					{
						port_start = oem_ptr;
						*oem_ptr = ADD_DEST_PORT;
						oem_ptr += 2;
						cmdSize += 2;
						*oem_ptr = tcpDestPort;
						oem_ptr ++;
						cmdSize ++;
						portCount = 1;
					}
					else
					{
						int i;
						// search dummy port
						for(i=0; i<portCount; i++)
						{
							if(*(port_start+i+2) == tcpDestPort)
							{
								break;
							}
						}

						if(i == portCount)
						{
							*oem_ptr = tcpDestPort;
							oem_ptr ++;
							cmdSize ++;
							portCount ++;
							if(portCount == 59) {
								oem_ptr = (unsigned short *)oem_cmd_buf;
								*oem_ptr = cmdSize<<1;
								*(port_start+1) = portCount;
								msm_proc_comm_oem_tcp_filter(oem_ptr, cmdSize<<1);

								*oem_ptr = 0;
								oem_ptr ++;
								cmdSize = 1;
								port_start = NULL;
								portCount = 0;
							}
						}
					}
				}

				*oem_ptr = UPDATE_COMPLETE;
				cmdSize ++;
				oem_ptr = (unsigned short *)oem_cmd_buf;
				*oem_ptr = cmdSize<<1;
				if(port_start != NULL)
				{
					*(port_start+1) = portCount;
				}

				msm_proc_comm_oem_tcp_filter(oem_ptr, cmdSize<<1);

				filp_close(tcpFs, NULL);
				tcpFs = NULL;
				filp_close(tcpFs6, NULL);
				tcpFs6 = NULL;
			}
		}
	}
}
/* } FIH;Tiger;2009/12/10 */
#endif	// CONFIG_FIH_PACKET_FILTER
/* } FIHTDC, Div2-SW2-BSP, Penho, packet filter */

const char *const pm_states[PM_SUSPEND_MAX] = {
#ifdef CONFIG_EARLYSUSPEND
	[PM_SUSPEND_ON]		= "on",
#endif
	[PM_SUSPEND_STANDBY]	= "standby",
	[PM_SUSPEND_MEM]	= "mem",
};

static struct platform_suspend_ops *suspend_ops;

/**
 *	suspend_set_ops - Set the global suspend method table.
 *	@ops:	Pointer to ops structure.
 */
void suspend_set_ops(struct platform_suspend_ops *ops)
{
	mutex_lock(&pm_mutex);
	suspend_ops = ops;
	mutex_unlock(&pm_mutex);
}

bool valid_state(suspend_state_t state)
{
	/*
	 * All states need lowlevel support and need to be valid to the lowlevel
	 * implementation, no valid callback implies that none are valid.
	 */
	return suspend_ops && suspend_ops->valid && suspend_ops->valid(state);
}

/**
 * suspend_valid_only_mem - generic memory-only valid callback
 *
 * Platform drivers that implement mem suspend only and only need
 * to check for that in their .valid callback can use this instead
 * of rolling their own .valid callback.
 */
int suspend_valid_only_mem(suspend_state_t state)
{
	return state == PM_SUSPEND_MEM;
}

static int suspend_test(int level)
{
#ifdef CONFIG_PM_DEBUG
	if (pm_test_level == level) {
		printk(KERN_INFO "suspend debug: Waiting for 5 seconds.\n");
		mdelay(5000);
		return 1;
	}
#endif /* !CONFIG_PM_DEBUG */
	return 0;
}

/**
 *	suspend_prepare - Do prep work before entering low-power state.
 *
 *	This is common code that is called for each state that we're entering.
 *	Run suspend notifiers, allocate a console and stop all processes.
 */
static int suspend_prepare(void)
{
	int error;

	if (!suspend_ops || !suspend_ops->enter)
		return -EPERM;

	pm_prepare_console();

	error = pm_notifier_call_chain(PM_SUSPEND_PREPARE);
	if (error)
		goto Finish;

	error = usermodehelper_disable();
	if (error)
		goto Finish;

	error = suspend_freeze_processes();
	if (!error)
		return 0;

	suspend_thaw_processes();
	usermodehelper_enable();
 Finish:
	pm_notifier_call_chain(PM_POST_SUSPEND);
	pm_restore_console();
	return error;
}

/* default implementation */
void __attribute__ ((weak)) arch_suspend_disable_irqs(void)
{
	local_irq_disable();
}

/* default implementation */
void __attribute__ ((weak)) arch_suspend_enable_irqs(void)
{
	local_irq_enable();
}

/**
 *	suspend_enter - enter the desired system sleep state.
 *	@state:		state to enter
 *
 *	This function should be called after devices have been suspended.
 */
static int suspend_enter(suspend_state_t state)
{
	int error;

	if (suspend_ops->prepare) {
		error = suspend_ops->prepare();
		if (error)
			return error;
	}

	error = dpm_suspend_noirq(PMSG_SUSPEND);
	if (error) {
		printk(KERN_ERR "PM: Some devices failed to power down\n");
		goto Platfrom_finish;
	}

	if (suspend_ops->prepare_late) {
		error = suspend_ops->prepare_late();
		if (error)
			goto Power_up_devices;
	}

	if (suspend_test(TEST_PLATFORM))
		goto Platform_wake;

	error = disable_nonboot_cpus();
	if (error || suspend_test(TEST_CPUS))
		goto Enable_cpus;

	arch_suspend_disable_irqs();
	BUG_ON(!irqs_disabled());

	error = sysdev_suspend(PMSG_SUSPEND);
	if (!error) {
		if (!suspend_test(TEST_CORE) && pm_check_wakeup_events()) {
			error = suspend_ops->enter(state);
	   		events_check_enabled = false;
                }
		sysdev_resume();
	}

	arch_suspend_enable_irqs();
	BUG_ON(irqs_disabled());

 Enable_cpus:
	enable_nonboot_cpus();

 Platform_wake:
	if (suspend_ops->wake)
		suspend_ops->wake();

 Power_up_devices:
	dpm_resume_noirq(PMSG_RESUME);

 Platfrom_finish:
	if (suspend_ops->finish)
		suspend_ops->finish();

	return error;
}

/**
 *	suspend_devices_and_enter - suspend devices and enter the desired system
 *				    sleep state.
 *	@state:		  state to enter
 */
/* FIHTDC, Div2-SW2-BSP, Penho, SKIP_SR_UARTMSG { */
#ifdef CONFIG_FIH_SUSPEND_RESUME_LOG
int g_suspend_success = 0;
#endif	// CONFIG_FIH_SUSPEND_RESUME_LOG
/* } FIHTDC, Div2-SW2-BSP, Penho, SKIP_SR_UARTMSG */
int suspend_devices_and_enter(suspend_state_t state)
{
	int error;
	gfp_t saved_mask;

	if (!suspend_ops)
		return -ENOSYS;

	if (suspend_ops->begin) {
		error = suspend_ops->begin(state);
		if (error)
			goto Close;
	}
	suspend_console();
	saved_mask = clear_gfp_allowed_mask(GFP_IOFS);
	suspend_test_start();
	error = dpm_suspend_start(PMSG_SUSPEND);
	if (error) {
		printk(KERN_ERR "PM: Some devices failed to suspend\n");
		goto Recover_platform;
	}
	suspend_test_finish("suspend devices");
	if (suspend_test(TEST_DEVICES))
		goto Recover_platform;

/* FIHTDC, Div2-SW2-BSP, Penho, SKIP_SR_UARTMSG { */
#ifdef CONFIG_FIH_SUSPEND_RESUME_LOG
	if (!suspend_enter(state)) g_suspend_success = 1;
#else	// CONFIG_FIH_SUSPEND_RESUME_LOG
	suspend_enter(state);
#endif	// CONFIG_FIH_SUSPEND_RESUME_LOG
/* } FIHTDC, Div2-SW2-BSP, Penho, SKIP_SR_UARTMSG */

 Resume_devices:
	suspend_test_start();
	dpm_resume_end(PMSG_RESUME);
	suspend_test_finish("resume devices");
	set_gfp_allowed_mask(saved_mask);
	resume_console();
/* FIHTDC, Div2-SW2-BSP, Penho, SKIP_SR_UARTMSG { */
#ifdef CONFIG_FIH_SUSPEND_RESUME_LOG
	g_suspend_success = 0;
#endif	// CONFIG_FIH_SUSPEND_RESUME_LOG
/* } FIHTDC, Div2-SW2-BSP, Penho, SKIP_SR_UARTMSG */
 Close:
	if (suspend_ops->end)
		suspend_ops->end();
	return error;

 Recover_platform:
	if (suspend_ops->recover)
		suspend_ops->recover();
	goto Resume_devices;
}

/**
 *	suspend_finish - Do final work before exiting suspend sequence.
 *
 *	Call platform code to clean up, restart processes, and free the
 *	console that we've allocated. This is not called for suspend-to-disk.
 */
static void suspend_finish(void)
{
	suspend_thaw_processes();
	usermodehelper_enable();
	pm_notifier_call_chain(PM_POST_SUSPEND);
	pm_restore_console();
}

/**
 *	enter_state - Do common work of entering low-power state.
 *	@state:		pm_state structure for state we're entering.
 *
 *	Make sure we're the only ones trying to enter a sleep state. Fail
 *	if someone has beat us to it, since we don't want anything weird to
 *	happen when we wake up.
 *	Then, do the setup for suspend, enter the state, and cleaup (after
 *	we've woken up).
 */
int enter_state(suspend_state_t state)
{
	int error;

	if (!valid_state(state))
		return -ENODEV;

	if (!mutex_trylock(&pm_mutex))
		return -EBUSY;

	suspend_sys_sync_queue();

	pr_debug("PM: Preparing system for %s sleep\n", pm_states[state]);
	error = suspend_prepare();
	if (error)
		goto Unlock;

/* FIHTDC, Div2-SW2-BSP, Penho, packet filter { */
#ifdef CONFIG_FIH_PACKET_FILTER
	set_packet_filter();
#endif	// CONFIG_FIH_PACKET_FILTER
/* } FIHTDC, Div2-SW2-BSP, Penho, packet filter */

	if (suspend_test(TEST_FREEZER))
		goto Finish;

	pr_debug("PM: Entering %s sleep\n", pm_states[state]);
	error = suspend_devices_and_enter(state);

 Finish:
	pr_debug("PM: Finishing wakeup.\n");
	suspend_finish();
 Unlock:
	mutex_unlock(&pm_mutex);
	return error;
}

/**
 *	pm_suspend - Externally visible function for suspending system.
 *	@state:		Enumerated value of state to enter.
 *
 *	Determine whether or not value is within range, get state
 *	structure, and enter (above).
 */
int pm_suspend(suspend_state_t state)
{
	if (state > PM_SUSPEND_ON && state <= PM_SUSPEND_MAX)
		return enter_state(state);
	return -EINVAL;
}
EXPORT_SYMBOL(pm_suspend);
