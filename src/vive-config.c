/*
 * HTC Vive configuration data readout
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <errno.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include <zlib.h>

#include "device.h"
#include "hidraw.h"

/*
 * Downloads configuration data stored in the Vive headset and controller.
 */
char *ouvrt_vive_get_config(OuvrtDevice *dev)
{
	unsigned char buf[64];
	unsigned char *config_json;
	void *config_z;
	z_stream strm;
	int count = 0;
	int ret;

	buf[0] = 0x10;
	ret = hid_get_feature_report_timeout(dev->fd, buf, sizeof(buf), 100);
	if (ret < 0) {
		g_print("%s: Read error 0x10: %d\n", dev->name, errno);
		return NULL;
	}

	config_z = g_malloc(4096);

	buf[0] = 0x11;
	do {
		ret = hid_get_feature_report_timeout(dev->fd, buf, sizeof(buf), 100);
		if (ret < 0) {
			g_print("%s: Read error after %d bytes: %d\n",
				dev->name, count, errno);
			g_free(config_z);
			return NULL;
		}

		if (buf[1] > 62) {
			g_print("%s: Invalid configuration data at %d\n",
				dev->name, count);
			g_free(config_z);
			return NULL;
		}

		if (count + buf[1] > 4096) {
			g_print("%s: Configuration data too large\n",
				dev->name);
			g_free(config_z);
			return NULL;
		}

		memcpy(config_z + count, buf + 2, buf[1]);
		count += buf[1];
	} while (buf[1]);

	g_debug("%s: Read configuration data: %d bytes\n", dev->name,
		count);

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK) {
		g_print("inflate_init failed: %d\n", ret);
		g_free(config_z);
		return NULL;
	}

	config_json = g_malloc(32768);

	strm.avail_in = count;
	strm.next_in = config_z;
	strm.avail_out = 32768;
	strm.next_out = config_json;

	ret = inflate(&strm, Z_FINISH);
	g_free(config_z);
	if (ret != Z_STREAM_END) {
		g_print("%s: Failed to inflate configuration data: %d\n",
			dev->name, ret);
		g_free(config_json);
		return NULL;
	}

	g_debug("%s: Inflated configuration data: %lu bytes\n",
		dev->name, strm.total_out);

	config_json[strm.total_out] = '\0';

	return g_realloc(config_json, strm.total_out + 1);
}
