/*
 * Author: Park Ju Hyung aka arter97 <qkrwngud825@gmail.com>
 *
 * Copyright 2015 Park Ju Hyung
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Adreno idler - Idling algorithm,
 * an efficient workaround for msm-adreno-tz's overheads.
 *
 * Main goal is to lower the power consumptions while maintaining high-performance.
 *
 * Since msm-adreno-tz tends to *not* use the lowest frequency even on idle,
 * Adreno idler replaces msm-adreno-tz's algorithm when it comes to
 * calculating idle frequency(mostly by ondemand's method).
 * The higher frequencies are not touched with this algorithm, so high-demanding
 * games will (most likely) not suffer from worsened performance.
 *
 * The additional idle_lasttime detects if last 500ms was idle before
 * ramping down the frequency to prevent micro-lags on scrolling or playing games.
 */

#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/msm_adreno_devfreq.h>

/* stats.busy_time threshold for determining if the given workload is idle.
   Any workload higher than this will be treated as non-idle workload,
   meaning the higher it gets, the slower & low-power it would get. */
static int idleworkload = 5000;
module_param_named(adreno_idler_idleworkload, idleworkload, int, 0664);

/* Numbers to wait before entering idle.
   The idlewait'th events before must be all idle before Adreno idler ramps
   down the frequency.
   This implementation is to prevent micro-lags on scrolling or playing games,
   meaning the lower it gets, the slower & low-power it would get. */
static int idlewait = 20;
module_param_named(adreno_idler_idlewait, idlewait, int, 0664);

/* Taken from ondemand */
static int downdifferenctial = 20;
module_param_named(adreno_idler_downdifferenctial, downdifferenctial, int, 0664);

/* Master switch to activate whole routine */
static int adreno_idler_active = 1;
module_param_named(adreno_idler_active, adreno_idler_active, int, 0664);

static unsigned int idlecount = 0;

int adreno_idler(struct devfreq_dev_status stats, struct devfreq *devfreq,
		 unsigned long *freq)
{
	if (!adreno_idler_active)
		return 0;

	if (stats.busy_time < idleworkload) {
		/* busy_time >= idleworkload should be considered as a non-idle workload. */
		idlecount++;
		if (*freq == devfreq->profile->freq_table[devfreq->profile->max_state]) {
			/* frequency is already at its lowest.
			   No need to calculate things, so bail out. */
			return 1;
		}
		if (idlecount >= idlewait &&
		    stats.busy_time * 100 < stats.total_time * downdifferenctial) {
			/* We are idle for idlewaitms! Ramp down the frequency now. */
			*freq = devfreq->profile->freq_table[devfreq->profile->max_state];
			idlecount--;
			return 1;
		}
	} else {
		/* This is the case where msm-adreno-tz don't use the lowest frequency.
		   Mimic this behavior by bumping up the frequency. */
		idlecount = 0;
		*freq = devfreq->profile->freq_table[devfreq->profile->max_state - 1];
		/* Do not return 1 here and allow rest of the algorithm to
		   figure out the appropriate frequency for current workload.
		   It can even set it back to lowest frequency. */
	}
	return 0;
}
EXPORT_SYMBOL(adreno_idler);

static int __init adreno_idler_init(void)
{
	return 0;
}
subsys_initcall(adreno_idler_init);

static void __exit adreno_idler_exit(void)
{
	return;
}
module_exit(adreno_idler_exit);

MODULE_AUTHOR("Park Ju Hyung <qkrwngud825@gmail.com>");
MODULE_DESCRIPTION("'adreno_idler - A powersaver for Adreno TZ"
	"Control idle algorithm for Adreno GPU series");
MODULE_LICENSE("GPL");
