/*
 * Copyright (C) 2013 Alexey Galakhov <agalakhov@gmail.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "dmi.h"

#include <glib.h>

static const gchar * const
sysfs_path = "/sys/class/dmi/id/";

static const gchar * const
vendor_table[] = {
	"sys_vendor",
	"chassis_vendor",
	"board_vendor",
	NULL
};

static const gchar * const
product_table[] = {
	"product_name",
	"board_name",
	NULL
};

static const gchar *
probe_sysfs (const gchar * const table[])
{
	const gchar * const *name = table;
	gchar *retval = NULL;
	for (; name; ++name) {
		gsize len = 0;
		gchar *path = g_strconcat (sysfs_path, *name, NULL);
		g_file_get_contents (path, &retval, &len, NULL);
		g_free (path);
		if (retval) {
			retval[len - 1] = '\0';
			break;
		}
	}
	if (retval) {
		g_strdelimit (retval, "\n", '\0');
		g_strstrip (retval);
	}
	return retval;
}

const gchar *
dmi_query_vendor (void)
{
	return probe_sysfs (vendor_table);
}

const gchar *
dmi_query_product (void)
{
	return probe_sysfs (product_table);
}

/* vim: set ts=8 sw=8 tw=0 : */
