/*
 * dvb_hdhomerun_init.c, skeleton driver for the HDHomeRun devices
 *
 * Copyright (C) 2010 Villy Thomsen <tfylliv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/string.h>

#include <linux/platform_device.h>

#include <media/dvb_demux.h>
#include <media/dvb_frontend.h>
#include <media/dvb_net.h>
#include <media/dvbdev.h>
#include <media/dmxdev.h>

#include "dvb_hdhomerun_compat.h"
#include "dvb_hdhomerun_control.h"
#include "dvb_hdhomerun_core.h"
#include "dvb_hdhomerun_data.h"
#include "dvb_hdhomerun_debug.h"
#include "dvb_hdhomerun_fe.h"

#include "dvb_hdhomerun_init.h"

MODULE_AUTHOR("Villy Thomsen");
MODULE_DESCRIPTION("HDHomeRun Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(HDHOMERUN_VERSION);
MODULE_SOFTDEP("pre: dvb_hdhomerun_core dvb_hdhomerun_fe");

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

#ifndef __devinit
#define __devinit
#define __devinitdata
#endif

extern int hdhomerun_debug_mask;

struct dvb_hdhomerun {
	struct platform_device *plat_dev;
	int instance;

	struct dmx_frontend hw_frontend;
	struct dmx_frontend mem_frontend;
	struct dmxdev dmxdev;
	struct dvb_adapter dvb_adapter;
	struct dvb_demux demux;
	struct dvb_frontend *fe;

	struct mutex feedlock;

	struct hdhomerun_register_tuner_data tuner_data;
};

struct platform_device *platform_device[HDHOMERUN_MAX_TUNERS];

static int hdhomerun_num_of_devices = 0;

/*
 * Demux setup
 */
static int dvb_hdhomerun_start_feed(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct dvb_demux *demux = feed->demux;
	struct dvb_hdhomerun *hdhomerun = (struct dvb_hdhomerun *) demux->priv;
	
	DEBUG_FUNC(1);

	if (hdhomerun == NULL)
		return -EINVAL;
	
	DEBUG_OUT(HDHOMERUN_STREAM, "hdhomerun%d: Tuner: %s "
		  "start feed: pid = 0x%x index = %d\n",
		  hdhomerun->instance, hdhomerun->tuner_data.name, feed->pid, feed->index);

	
	
	if (!demux->dmx.frontend)
		return -EINVAL;

	mutex_lock(&hdhomerun->feedlock);
	{
		struct dvbhdhomerun_control_mesg mesg;
		struct hdhomerun_dvb_demux_feed my_feed = {
			.pid = feed->pid,
			.index = feed->index,
		};
		mesg.type = DVB_HDHOMERUN_START_FEED;
		mesg.id = hdhomerun->plat_dev->id;
		mesg.u.demux_feed = my_feed;
		ret = hdhomerun_control_post_and_wait(&mesg);
	}
	
	mutex_unlock(&hdhomerun->feedlock);
	return ret;
}

static int dvb_hdhomerun_stop_feed(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct dvb_demux *demux = feed->demux;
	struct dvb_hdhomerun *hdhomerun = (struct dvb_hdhomerun *) demux->priv;

	DEBUG_FUNC(1);

	if (hdhomerun == NULL)
		return -EINVAL;

	DEBUG_OUT(HDHOMERUN_STREAM, "hdhomerun%d: "
		  "Stop feed: pid = 0x%x index = %d\n",
		  hdhomerun->instance, feed->pid, feed->index);
	
	mutex_lock(&hdhomerun->feedlock);
	{
		struct dvbhdhomerun_control_mesg mesg;
		struct hdhomerun_dvb_demux_feed my_feed = {
			.pid = feed->pid,
			.index = feed->index,
		};
		mesg.type = DVB_HDHOMERUN_STOP_FEED;
		mesg.id = hdhomerun->plat_dev->id;
		mesg.u.demux_feed = my_feed;
		ret = hdhomerun_control_post_and_wait(&mesg);
	}


	mutex_unlock(&hdhomerun->feedlock);

	return ret;
}

static int __devinit dvb_hdhomerun_register(struct dvb_hdhomerun *hdhomerun)
{
	struct dvb_adapter *dvb_adapter;
	struct dvb_demux *dvbdemux;
	struct dmx_demux *dmx;
	int ret;
   int len;

	DEBUG_FUNC(1);

	ret = dvb_register_adapter(&hdhomerun->dvb_adapter, "HDHomeRun",
				   THIS_MODULE, &hdhomerun->plat_dev->dev,
				   adapter_nr);
	if (ret < 0)
		goto err_out;

	dvb_adapter = &hdhomerun->dvb_adapter;

	dvbdemux = &hdhomerun->demux;

	dvbdemux->priv = (void *) hdhomerun;

	dvbdemux->filternum = 256;
	dvbdemux->feednum = 256;
	dvbdemux->start_feed = dvb_hdhomerun_start_feed;
	dvbdemux->stop_feed = dvb_hdhomerun_stop_feed;
	dvbdemux->dmx.capabilities = (DMX_TS_FILTERING | DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING);
	ret = dvb_dmx_init(dvbdemux);
	if (ret < 0)
		goto err_dvb_unregister_adapter;

	dmx = &dvbdemux->dmx;

	hdhomerun->hw_frontend.source = DMX_FRONTEND_0;
	hdhomerun->mem_frontend.source = DMX_MEMORY_FE;
	hdhomerun->dmxdev.filternum = 256;
	hdhomerun->dmxdev.demux = dmx;

	ret = dvb_dmxdev_init(&hdhomerun->dmxdev, dvb_adapter);
	if (ret < 0)
		goto err_dvb_dmx_release;

	ret = dmx->add_frontend(dmx, &hdhomerun->hw_frontend);
	if (ret < 0)
		goto err_dvb_dmxdev_release;

	ret = dmx->add_frontend(dmx, &hdhomerun->mem_frontend);
	if (ret < 0)
		goto err_remove_hw_frontend;

	ret = dmx->connect_frontend(dmx, &hdhomerun->hw_frontend);
	if (ret < 0)
		goto err_remove_mem_frontend;

	if(hdhomerun->tuner_data.type == 1) {
	  hdhomerun->fe = dvb_attach(dvb_hdhomerun_fe_attach_dvbc, hdhomerun->plat_dev->id);
	} else if(hdhomerun->tuner_data.type == 2) {
	  hdhomerun->fe = dvb_attach(dvb_hdhomerun_fe_attach_dvbt, hdhomerun->plat_dev->id);
	} else if(hdhomerun->tuner_data.type == 3) {
	  hdhomerun->fe = dvb_attach(dvb_hdhomerun_fe_attach_atsc, hdhomerun->plat_dev->id);
	} else {
	  hdhomerun->fe = dvb_attach(dvb_hdhomerun_fe_attach_atsc, hdhomerun->plat_dev->id);
	}

	if (hdhomerun->fe == NULL) {
		printk(KERN_ERR "HDHomeRun%d: frontend attach failed (is dvb_hdhomerun_fe loaded?)\n",
		       hdhomerun->instance);
		ret = -ENODEV;
		goto err_disconnect_frontend;
	}

   if(hdhomerun->tuner_data.use_full_name) {
      len = strlen((const char*)hdhomerun->fe->ops.info.name);
      hdhomerun->fe->ops.info.name[len] = 32; // Insert space char
      strncpy( &hdhomerun->fe->ops.info.name[len+1],
               hdhomerun->tuner_data.name,
               sizeof(hdhomerun->fe->ops.info.name) - len - 1);
      hdhomerun->fe->ops.info.name[ sizeof(hdhomerun->fe->ops.info.name) - 1 ] = '\0';
   }

	ret = dvb_register_frontend(dvb_adapter, hdhomerun->fe);
	if (ret < 0)
		goto err_release_frontend;

	printk(KERN_INFO "HDHomeRun%d: DVB Frontend registered\n",
	       hdhomerun->instance);
	printk(KERN_INFO "HDHomeRun%d: Registered DVB adapter%d\n",
	       hdhomerun->instance, hdhomerun->dvb_adapter.num);

	mutex_init(&hdhomerun->feedlock);

	ret = dvb_hdhomerun_data_create_device(dvbdemux, hdhomerun->plat_dev->id);
	if (ret < 0)
		goto err_unregister_frontend;

	return 0;

err_unregister_frontend:
	dvb_unregister_frontend(hdhomerun->fe);
err_release_frontend:
	dvb_frontend_detach(hdhomerun->fe);
	hdhomerun->fe = NULL;
err_disconnect_frontend:
	dmx->disconnect_frontend(dmx);
err_remove_mem_frontend:
	dmx->remove_frontend(dmx, &hdhomerun->mem_frontend);
err_remove_hw_frontend:
	dmx->remove_frontend(dmx, &hdhomerun->hw_frontend);
err_dvb_dmxdev_release:
	dvb_dmxdev_release(&hdhomerun->dmxdev);
err_dvb_dmx_release:
	dvb_dmx_release(dvbdemux);
err_dvb_unregister_adapter:
	dvbdemux->priv = NULL;
	dvb_unregister_adapter(dvb_adapter);
err_out:
	return ret;
}

static void dvb_hdhomerun_unregister(struct dvb_hdhomerun *hdhomerun)
{
	struct dvb_adapter *dvb_adapter;
	struct dvb_demux *dvbdemux;
	struct dmx_demux *dmx;
	
	DEBUG_FUNC(1);

	printk(KERN_INFO "HDHomeRun%d: DVB Frontend unregister\n",
	       hdhomerun->instance);
	printk(KERN_INFO "HDHomeRun%d: Unregister DVB adapter%d\n",
	       hdhomerun->instance, hdhomerun->dvb_adapter.num);

	dvb_adapter = &hdhomerun->dvb_adapter;
	dvbdemux = &hdhomerun->demux;
	dmx = &dvbdemux->dmx;

	dmx->close(dmx);
	dmx->remove_frontend(dmx, &hdhomerun->mem_frontend);
	dmx->remove_frontend(dmx, &hdhomerun->hw_frontend);
	dvb_dmxdev_release(&hdhomerun->dmxdev);
	dvb_dmx_release(dvbdemux);
	dvbdemux->priv = NULL;
	if (hdhomerun->fe) {
		dvb_unregister_frontend(hdhomerun->fe);
		dvb_frontend_detach(hdhomerun->fe);
		hdhomerun->fe = NULL;
	}
	dvb_unregister_adapter(dvb_adapter);
}


static int __devinit dvb_hdhomerun_probe(struct platform_device *plat_dev)
{
	int ret;
	struct dvb_hdhomerun *hdhomerun;

	DEBUG_FUNC(1);

	hdhomerun = kzalloc(sizeof(struct dvb_hdhomerun), GFP_KERNEL);
	if (hdhomerun == NULL) {
		printk(KERN_ERR
		       "HDHomeRun: out of memory for adapter %d\n",
		       plat_dev->id);
		return -ENOMEM;
	}

	hdhomerun->plat_dev = plat_dev;
	hdhomerun->instance = plat_dev->id;

	platform_set_drvdata(plat_dev, hdhomerun);

	/* ret = dvb_hdhomerun_register(hdhomerun); */
	/* if (ret < 0) { */
	/* 	platform_set_drvdata(plat_dev, NULL); */
	/* 	kfree(hdhomerun); */
	/* } */

	return 0;
	return ret;
}

static HDHR_REMOVE_RET dvb_hdhomerun_remove(struct platform_device *plat_dev)
{
	struct dvb_hdhomerun *hdhomerun;

	DEBUG_FUNC(1);

	hdhomerun = platform_get_drvdata(plat_dev);
	if (hdhomerun == NULL)
		HDHR_REMOVE_RETURN;

	dvb_hdhomerun_unregister(hdhomerun);

	platform_set_drvdata(plat_dev, NULL);
	kfree(hdhomerun);
	HDHR_REMOVE_RETURN;
}


int dvb_hdhomerun_register_hdhomerun(struct hdhomerun_register_tuner_data *tuner_data) 
{
	int i;
	int ret;
	int id;
	struct dvb_hdhomerun *hdhomerun;
	struct hdhomerun_register_tuner_data *tmp;
	struct platform_device *pdev;

	DEBUG_FUNC(1);

	/* Check if we already have this tuner registered, handles the
	   case where userhdhomerun has been stopped/started. */
	if(hdhomerun_num_of_devices > 0) {
		for(i = 0; i < hdhomerun_num_of_devices; ++i) {
			if (!platform_device[i])
				continue;
			hdhomerun = platform_get_drvdata(platform_device[i]);
			if (!hdhomerun)
				continue;
			tmp = &hdhomerun->tuner_data;
			if(strncmp(tuner_data->name, tmp->name, sizeof(tmp->name)) == 0) {
				/* Already have that tuner */
				printk("hdhomerun: dvb device for this tuner already exists, ignore request %s\n", tuner_data->name);
				tuner_data->id = tmp->id;
				return 0;
			}
		}
	}

	if(hdhomerun_num_of_devices >= HDHOMERUN_MAX_TUNERS)
		return -ENODEV;

	id = hdhomerun_num_of_devices;
	pdev = platform_device_register_simple("HDHomeRun", id, NULL, 0);
	if (IS_ERR(pdev)) {
		printk(KERN_ERR "HDHomeRun: could not allocate and register instance %d\n", id);
		return PTR_ERR(pdev);
	}
	platform_device[id] = pdev;
	tuner_data->id = pdev->id;

	hdhomerun = platform_get_drvdata(pdev);
	if (!hdhomerun) {
		printk(KERN_ERR "HDHomeRun: missing driver data for instance %d\n", id);
		platform_device_unregister(pdev);
		platform_device[id] = NULL;
		return -ENODEV;
	}
	hdhomerun->tuner_data = *tuner_data;

	ret = dvb_hdhomerun_register(hdhomerun);
	if (ret < 0) {
		printk(KERN_ERR "HDHomeRun: failed to register DVB adapter for %s (%d)\n",
		       tuner_data->name, ret);
		/*
		 * dvb_hdhomerun_register() already rolled back DVB
		 * objects. Drop drvdata so .remove is a no-op, then
		 * unregister the platform device. Do not count this
		 * instance: a later tuner used to oops in strncmp()
		 * on the freed/NULL hdhomerun (GitHub issue #7).
		 */
		platform_set_drvdata(pdev, NULL);
		kfree(hdhomerun);
		platform_device_unregister(pdev);
		platform_device[id] = NULL;
		return ret;
	}

	hdhomerun_num_of_devices++;
	return 0;
}
EXPORT_SYMBOL_GPL(dvb_hdhomerun_register_hdhomerun);


/*
 * Init/Exit
 */

static struct platform_driver dvb_hdhomerun_platform_driver = {
	.probe = dvb_hdhomerun_probe,
	.HDHR_PLATFORM_REMOVE_CB = dvb_hdhomerun_remove,
	.driver = {
		.name = "HDHomeRun",
	},
};


static int __init dvb_hdhomerun_init(void)
{
	int ret = 0;

	DEBUG_FUNC(1);

	printk(KERN_INFO
	       "HDHomeRun: Begin init, version %s\n",
	       HDHOMERUN_VERSION);

	ret = platform_driver_register(&dvb_hdhomerun_platform_driver);
	if (ret) {
		printk(KERN_ERR "HDHomeRun: "
		       "Error %d from platform_driver_register()\n", ret);
		goto init_exit;
	}

	ret = dvb_hdhomerun_control_init();

	printk(KERN_INFO "HDHomeRun: Waiting for userspace to connect\n");

	/* Hmmm, need a bit more error checking in the above */

init_exit:
	printk(KERN_INFO "HDHomeRun: End init\n");
	return ret;
}

static void __exit dvb_hdhomerun_exit(void)
{
	int i;

	printk(KERN_INFO "HDHomeRun: Begin exit\n");

	DEBUG_FUNC(1);

	dvb_hdhomerun_control_exit();

	if(hdhomerun_num_of_devices) {
		for(i = 0; i < hdhomerun_num_of_devices; ++i) {
			dvb_hdhomerun_data_delete_device(i);
			if (platform_device[i]) {
				platform_device_unregister(platform_device[i]);
				platform_device[i] = NULL;
			}
		}
		hdhomerun_num_of_devices = 0;
	}

	dvb_hdhomerun_data_exit();

	platform_driver_unregister(&dvb_hdhomerun_platform_driver);
	printk(KERN_INFO "HDHomeRun: End exit\n");
}

module_init(dvb_hdhomerun_init);
module_exit(dvb_hdhomerun_exit);
