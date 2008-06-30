/*
 * Copyright © 2006-2007 Daniel Stone
 * Copyright © 2008 Novell, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Daniel Stone <daniel@fooishbar.org>
 *         David Reveman <davidr@novell.com>
 */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "config-backends.h"
#include "dixstruct.h"
#include "opaque.h" /* for 'display': there should be a better way. */
#include "dmxdbus.h"
#include "dmxextension.h"

#define API_VERSION 1

#define MATCH_RULE "type='method_call',interface='org.x.config.dmx'"
#define MALFORMED_MSG "[dmx/dbus] malformed message, dropping"

static const unsigned int hexvalues[256] = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 9 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 19 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 29 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 39 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, /* 49 */
    0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0, 0x0, /* 59 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0xa, 0xb, 0xb, 0xd, 0xe, /* 69 */
    0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 79 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 89 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xa, 0xb, 0xc, /* 99 */
    0xd, 0xe, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};

struct connection_info {
    char           busobject[32];
    char           busname[64];
    DBusConnection *connection;
};

static void
reset_info (struct connection_info *info)
{
    info->connection = NULL;
    info->busname[0] = '\0';
    info->busobject[0] = '\0';
}

static int
handle_attach_screen (DBusMessage *message,
		      DBusMessage *reply,
		      DBusError   *error,
		      int         addInput)
{
    DMXScreenAttributesRec attr;
    DBusMessageIter        reply_iter;
    int                    screen, ret, size, i;
    char                   *display, *auth_type, *auth_data, *name;
    char                   *data, *ptr;

    dbus_message_iter_init_append (reply, &reply_iter);

    if (!dbus_message_get_args (message,
				error,
				DBUS_TYPE_UINT32,
				&screen,
				DBUS_TYPE_STRING,
				&display,
				DBUS_TYPE_STRING,
				&auth_type,
				DBUS_TYPE_STRING,
				&auth_data,
				DBUS_TYPE_STRING,
				&name,
				DBUS_TYPE_INVALID))
    {
	DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
	return BadValue;
    }

    memset (&attr, 0, sizeof (attr));

    attr.displayName = display;

    if (screen >= dmxGetNumScreens ())
    {
	DMXScreenAttributesRec attribs;

	for (screen = 0; screen < dmxGetNumScreens (); screen++)
	{
	    dmxGetScreenAttributes (screen, &attribs);

	    if (!attribs.displayName || !*attribs.displayName)
		break;
	}
    }

    size = strlen (auth_data) / 2;
    data = ptr = malloc (size);
    if (!data)
    {
	ErrorF ("[dmx/dbus] couldn't translate auth data\n");
	return BadAlloc;
    }

    for (i = 0; i < size; i++)
    {
	*ptr++ = (char) ((hexvalues[(int) auth_data[0]] * 16) +
			 (hexvalues[(int) auth_data[1]]));
	auth_data += 2;
    }

    ret = dmxAttachScreen (screen,
			   &attr,
			   auth_type,
			   data,
			   size,
			   (dmxErrorSetProcPtr) dbus_set_error,
			   error,
			   DBUS_ERROR_FAILED);

    free (data);

    if (ret != Success)
    {
        DebugF ("[dmx/dbus] dmxAttachScreen failed\n");
        return ret;
    }

    if (addInput)
    {
	DMXInputAttributesRec attrib;
	int                   id;

	memset (&attrib, 0, sizeof (attrib));

	attrib.physicalScreen = screen;
	attrib.inputType      = 2;

	ret = dmxAddInput (&attrib, &id);
	if (ret != Success)
	{
	    DebugF ("[dmx/dbus] dmxAddInput failed\n");
	    dmxDetachScreen (screen);
	    return ret;
	}
    }

    if (!dbus_message_iter_append_basic (&reply_iter,
					 DBUS_TYPE_INT32,
					 &screen))
    {
	ErrorF ("[dmx/dbus] couldn't append to iterator\n");
	return BadAlloc;
    }

    return Success;
}

static int
attach_screen (DBusMessage *message,
	       DBusMessage *reply,
	       DBusError   *error)
{
    return handle_attach_screen (message, reply, error, 1);
}

static int
attach_screen_output (DBusMessage *message,
		      DBusMessage *reply,
		      DBusError   *error)
{
    return handle_attach_screen (message, reply, error, 0);
}

static int
detach_screen (DBusMessage *message,
	       DBusMessage *reply,
	       DBusError   *error)
{
    DBusMessageIter reply_iter;
    int             screen;
    char	    *name;

    dbus_message_iter_init_append (reply, &reply_iter);

    if (!dbus_message_get_args (message,
				error,
				DBUS_TYPE_STRING,
				&name,
				DBUS_TYPE_INVALID))
    {
	DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
	return BadValue;
    }

    for (screen = 0; screen < dmxGetNumScreens (); screen++)
    {
	DMXScreenAttributesRec attribs;

	dmxGetScreenAttributes (screen, &attribs);

	if (!attribs.displayName || !*attribs.displayName)
	    continue;

	if (strcmp (attribs.displayName, name) == 0)
	    break;
    }

    if (screen == dmxGetNumScreens ())
    {
        DebugF ("[dmx/dbus] screen '%s' is not attached\n", name);
        return BadValue;
    }

    if (dmxDetachScreen (screen) != 0)
    {
        DebugF ("[dmx/dbus] failed to detach screen %d\n", screen);
        return BadMatch;
    }

    DebugF ("[dmx/dbus] detaching screen %d\n", screen);

    return Success;
}

static int
list_screens (DBusMessage *message,
	      DBusMessage *reply,
	      DBusError   *error)
{
    DBusMessageIter iter, subiter;
    int		    screen;

    dbus_message_iter_init_append (reply, &iter);

    for (screen = 0; screen < dmxGetNumScreens (); screen++)
    {
	DMXScreenAttributesRec attribs;

	dmxGetScreenAttributes (screen, &attribs);

	if (!attribs.displayName || !*attribs.displayName)
	    continue;

        if (!dbus_message_iter_open_container (&iter,
					       DBUS_TYPE_STRUCT,
					       NULL,
					       &subiter))
	{
            ErrorF ("[dmx/dbus] couldn't init container\n");
            return BadAlloc;
        }

        if (!dbus_message_iter_append_basic (&subiter,
					     DBUS_TYPE_UINT32,
					     &screen))
	{
            ErrorF ("[dmx/dbus] couldn't append to iterator\n");
            return BadAlloc;
        }

        if (!dbus_message_iter_append_basic (&subiter,
					     DBUS_TYPE_STRING,
					     &attribs.displayName))
	{
            ErrorF("[dmx/dbus] couldn't append to iterator\n");
            return BadAlloc;
        }

        if (!dbus_message_iter_close_container (&iter, &subiter))
	{
            ErrorF ("[dmx/dbus] couldn't close container\n");
            return BadAlloc;
        }
    }

    return Success;
}

static int
get_version (DBusMessage *message,
	     DBusMessage *reply,
	     DBusError   *error)
{
    DBusMessageIter iter;
    unsigned int    version = API_VERSION;

    dbus_message_iter_init_append (reply, &iter);

    if (!dbus_message_iter_append_basic (&iter, DBUS_TYPE_UINT32, &version))
    {
        ErrorF ("[dmx/dbus] couldn't append version\n");
        return BadAlloc;
    }

    return Success;
}

static DBusHandlerResult
message_handler (DBusConnection *connection,
		 DBusMessage    *message,
		 void           *data)
{
    struct connection_info *info = data;
    DBusMessage            *reply;
    DBusError              error;

    /* ret is the overall D-Bus handler result, whereas err is the internal
     * X error from our individual functions. */
    int                    err, ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DebugF ("[dmx/dbus] received a message for %s\n",
	    dbus_message_get_interface (message));

    dbus_error_init (&error);

    reply = dbus_message_new_method_return (message);
    if (!reply)
    {
        ErrorF ("[dmx/dbus] failed to create reply\n");
        ret = DBUS_HANDLER_RESULT_NEED_MEMORY;
        goto err_start;
    }

    if (strcmp (dbus_message_get_member (message), "attachScreen") == 0)
        err = attach_screen (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message),
		     "attachScreenOutput") == 0)
        err = attach_screen_output (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "detachScreen") == 0)
        err = detach_screen (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "listScreens") == 0)
        err = list_screens (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "version") == 0)
        err = get_version (message, reply, &error);
    else
        goto err_reply;

    /* Failure to allocate is a special case. */
    if (err == BadAlloc)
    {
        ret = DBUS_HANDLER_RESULT_NEED_MEMORY;
        goto err_reply;
    }

    if (err != Success)
    {
	dbus_message_unref (reply);

	reply = dbus_message_new_error_printf (message,
					       error.name,
					       error.message);

	if (!reply)
	{
	    ErrorF ("[dmx/dbus] failed to create reply\n");
	    ret = DBUS_HANDLER_RESULT_NEED_MEMORY;
	    goto err_start;
	}
    }

    /* While failure here is always an OOM, we don't return that,
     * since that would result in devices being double-added/removed. */
    if (dbus_connection_send (info->connection, reply, NULL))
	dbus_connection_flush (info->connection);
    else
        ErrorF ("[dmx/dbus] failed to send reply\n");

    ret = DBUS_HANDLER_RESULT_HANDLED;

err_reply:
    dbus_message_unref (reply);
err_start:
    dbus_error_free (&error);

    return ret;
}

static void
connect_hook (DBusConnection *connection,
	      void           *data)
{
    DBusObjectPathVTable   vtable = { .message_function = message_handler };
    DBusError              error;
    struct connection_info *info = data;

    info->connection = connection;

    dbus_error_init (&error);

    /* blocks until we get a reply. */
    dbus_bus_add_match (info->connection, MATCH_RULE, &error);
    if (!dbus_error_is_set (&error))
    {
	if (dbus_connection_register_object_path (info->connection,
						  info->busobject,
						  &vtable,
						  info))
	{
	    DebugF ("[dbus] registered %s, %s\n", info->busname,
		    info->busobject);
	}
	else
	{
	    ErrorF ("[dmx/dbus] couldn't register object path\n");
	    dbus_bus_remove_match (info->connection, MATCH_RULE, &error);
	    reset_info (info);
	}
    }
    else
    {
        ErrorF ("[dmx/dbus] couldn't add match: %s (%s)\n", error.name,
		error.message);
	reset_info (info);
    }

    dbus_error_free (&error);
}

static void
disconnect_hook (void *data)
{
}

static struct connection_info connection_data;
static struct config_dbus_core_hook core_hook = {
    .connect    = connect_hook,
    .disconnect = disconnect_hook,
    .data       = &connection_data
};

int
dmx_dbus_init (void)
{
    snprintf (connection_data.busobject, sizeof (connection_data.busobject),
	      "/org/x/config/dmx/%d", atoi (display));

    return config_dbus_core_add_hook (&core_hook);
}

void
dmx_dbus_fini (void)
{
    config_dbus_core_remove_hook (&core_hook);
    reset_info (&connection_data);
}
