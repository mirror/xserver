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

#define API_VERSION 3

#define MATCH_RULE "type='method_call',interface='org.x.config.dmx'"
#define MALFORMED_MSG "[dmx/dbus] malformed message, dropping"

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
enable_screen (DBusMessage *message,
	       DBusMessage *reply,
	       DBusError   *error)
{
    DMXScreenAttributesRec attr;
    uint32_t               screen;

    if (!dbus_message_get_args (message, error,
				DBUS_TYPE_UINT32,
				&screen,
				DBUS_TYPE_INVALID))
    {
	DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
	return BadValue;
    }

    if (screen >= dmxGetNumScreens ())
    {
	dbus_set_error (error,
			DMX_ERROR_INVALID_SCREEN,
			"Screen %d does not exist", screen);
	return BadValue;
    }

    dmxGetScreenAttributes (screen, &attr);

    if (!attr.name || !*attr.name)
    {
	dbus_set_error (error,
			DBUS_ERROR_FAILED,
			"No back-end server attached to screen %d",
			screen);
	return BadValue;
    }

    dmxEnableScreen(screen);

    return Success;
}

static int
disable_screen (DBusMessage *message,
		DBusMessage *reply,
		DBusError   *error)
{
    DMXScreenAttributesRec attr;
    uint32_t               screen;

    if (!dbus_message_get_args (message, error,
				DBUS_TYPE_UINT32,
				&screen,
				DBUS_TYPE_INVALID))
    {
	DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
	return BadValue;
    }

    if (screen >= dmxGetNumScreens ())
    {
	dbus_set_error (error,
			DMX_ERROR_INVALID_SCREEN,
			"Screen %d does not exist", screen);
	return BadValue;
    }

    dmxGetScreenAttributes (screen, &attr);

    if (!attr.name || !*attr.name)
    {
	dbus_set_error (error,
			DBUS_ERROR_FAILED,
			"No back-end server attached to screen %d",
			screen);
	return BadValue;
    }

    dmxDisableScreen(screen);

    return Success;
}

static int
attach_screen (uint32_t    window,
	       uint32_t    screen,
	       uint32_t    auth_type_len,
	       uint32_t    auth_data_len,
	       const char  *display,
	       const char  *auth_type,
	       const char  *auth_data,
	       const char  *name,
	       int32_t     x,
	       int32_t     y,
	       DBusMessage *reply,
	       DBusError   *error)
{
    DMXScreenAttributesRec attr;
    int                    ret;

    if (!*name)
    {
	dbus_set_error (error,
			DBUS_ERROR_FAILED,
			"Cannot use empty string for screen name");
	return BadValue;
    }

    if (screen >= dmxGetNumScreens ())
    {
	dbus_set_error (error,
			DMX_ERROR_INVALID_SCREEN,
			"Screen %d does not exist", screen);
	return BadValue;
    }

    dmxGetScreenAttributes (screen, &attr);

    if (attr.name && *attr.name)
    {
	dbus_set_error (error,
			DMX_ERROR_SCREEN_IN_USE,
			"Back-end server already attached to screen %d",
			screen);
	return BadValue;
    }

    memset (&attr, 0, sizeof (attr));

    attr.name              = name;
    attr.displayName       = display;
    attr.rootWindowXoffset = x;
    attr.rootWindowYoffset = y;

    ret = dmxAttachScreen (screen,
			   &attr,
			   window,
			   auth_type,
			   auth_type_len,
			   auth_data,
			   auth_data_len,
			   (dmxErrorSetProcPtr) dbus_set_error,
			   error,
			   DBUS_ERROR_FAILED);

    if (ret != Success)
    {
        DebugF ("[dmx/dbus] dmxAttachScreen failed\n");
        return ret;
    }

    return Success;
}

static int
attach_screen_without_offset (DBusMessage *message,
			      DBusMessage *reply,
			      DBusError   *error)
{
    uint32_t window, screen, auth_type_len, auth_data_len;
    char     *display, *auth_type, *auth_data, *name;

    if (!dbus_message_get_args (message, error,
				DBUS_TYPE_UINT32,
				&screen,
				DBUS_TYPE_STRING,
				&display,
				DBUS_TYPE_STRING,
				&name,
				DBUS_TYPE_UINT32,
				&window,
				DBUS_TYPE_ARRAY,  DBUS_TYPE_BYTE,
				&auth_type, &auth_type_len,
				DBUS_TYPE_ARRAY,  DBUS_TYPE_BYTE,
				&auth_data, &auth_data_len,
				DBUS_TYPE_INVALID))
    {
	DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
	return BadValue;
    }

    return attach_screen (window, screen,
			  auth_type_len, auth_data_len,
			  display,
			  auth_type, auth_data,
			  name,
			  0, 0,
			  reply, error);
}

static int
attach_screen_with_offset (DBusMessage *message,
			   DBusMessage *reply,
			   DBusError   *error)
{
    uint32_t window, screen, auth_type_len, auth_data_len;
    int32_t  x, y;
    char     *display, *auth_type, *auth_data, *name;

    if (!dbus_message_get_args (message, error,
				DBUS_TYPE_UINT32,
				&screen,
				DBUS_TYPE_STRING,
				&display,
				DBUS_TYPE_STRING,
				&name,
				DBUS_TYPE_UINT32,
				&window,
				DBUS_TYPE_INT32,
				&x,
				DBUS_TYPE_INT32,
				&y,
				DBUS_TYPE_ARRAY,  DBUS_TYPE_BYTE,
				&auth_type, &auth_type_len,
				DBUS_TYPE_ARRAY,  DBUS_TYPE_BYTE,
				&auth_data, &auth_data_len,
				DBUS_TYPE_INVALID))
    {
	DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
	return BadValue;
    }

    return attach_screen (window, screen,
			  auth_type_len, auth_data_len,
			  display,
			  auth_type, auth_data,
			  name,
			  x, y,
			  reply, error);
}

static int
detach_screen (DBusMessage *message,
	       DBusMessage *reply,
	       DBusError   *error)
{
    uint32_t screen;
    int      ret;

    if (!dbus_message_get_args (message, error,
				DBUS_TYPE_UINT32,
				&screen,
				DBUS_TYPE_INVALID))
   { 
       DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
       return BadValue;
    }

    ret = dmxDetachScreen (screen);
    if (ret != Success)
    {
        DebugF ("[dmx/dbus] dmxDetachScreen failed\n");
        return ret;
    }

    return Success;
}

static int
add_input (DBusMessage *message,
	   DBusMessage *reply,
	   DBusError   *error)
{
    DMXInputAttributesRec attr;
    DBusMessageIter       iter;
    uint32_t              screen, id;
    dbus_bool_t           core;
    int                   input_id, ret;

    dbus_message_iter_init_append (reply, &iter);

    if (!dbus_message_get_args (message, error,
				DBUS_TYPE_UINT32,
				&screen,
				DBUS_TYPE_BOOLEAN,
				&core,
				DBUS_TYPE_INVALID))
    {
	DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
	return BadValue;
    }

    memset (&attr, 0, sizeof (attr));

    attr.physicalScreen = screen;
    attr.inputType      = 2;

    ret = dmxAddInput (&attr, &input_id);
    if (ret != Success)
    {
	DebugF ("[dmx/dbus] dmxAddInput failed\n");
	return ret;
    }

    id = input_id;

    if (!dbus_message_iter_append_basic (&iter,
					 DBUS_TYPE_UINT32,
					 &id))
    {
	ErrorF ("[dmx/dbus] couldn't append to iterator\n");
	dmxRemoveInput (id);
	return BadAlloc;
    }

    return Success;
}

static int
remove_input (DBusMessage *message,
	      DBusMessage *reply,
	      DBusError   *error)
{
    uint32_t id;
    int      ret;

    if (!dbus_message_get_args (message, error,
				DBUS_TYPE_UINT32,
				&id,
				DBUS_TYPE_INVALID))
    {
	DebugF (MALFORMED_MSG ": %s, %s", error->name, error->message);
	return BadValue;
    }

    ret = dmxRemoveInput (id);
    if (ret != Success)
    {
	DebugF ("[dmx/dbus] dmxRemoveInput failed\n");
	return ret;
    }

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

	if (!attribs.name || !*attribs.name)
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
					     &attribs.name))
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

    if (strcmp (dbus_message_get_member (message), "enableScreen") == 0)
        err = enable_screen (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "disableScreen") == 0)
        err = disable_screen (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "attachScreen") == 0)
        err = attach_screen_without_offset (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "attachScreenAt") == 0)
        err = attach_screen_with_offset (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "detachScreen") == 0)
        err = detach_screen (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "addInput") == 0)
        err = add_input (message, reply, &error);
    else if (strcmp (dbus_message_get_member (message), "removeInput") == 0)
        err = remove_input (message, reply, &error);
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
