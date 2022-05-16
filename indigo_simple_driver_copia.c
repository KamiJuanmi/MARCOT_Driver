/** Prueba de desarollo de un driver
 \file indigo_simple_driver.c
 */

#define DRIVER_VERSION 0x0002
#define DRIVER_NAME "Juanmi_driver_hucha"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>

#include <indigo/indigo_driver_xml.h>

#include "indigo_simple_driver.h"

// No se si esto lo utilizare
#define PRIVATE_DATA ((hucha_private_data *)device->private_data)

#define HUCHA_DINERO_PROPERTY (PRIVATE_DATA->dinero_property)
#define HUCHA_DINERO_ITEM (HUCHA_DINERO_PROPERTY->items + 0)

#define AHORRA_SPEED 50

typedef struct
{
    int handle;
    double target_dinerico, current_dinerico;
    indigo_property *dinero_property;
    indigo_timer *hucha_timer;
} hucha_private_data;

static indigo_result hucha_enumerate_properties(indigo_device *device, indigo_client *client, indigo_property *property)
{
    if (IS_CONNECTED)
    {
        if (indigo_property_match(HUCHA_DINERO_PROPERTY, property))
            indigo_define_property(device, HUCHA_DINERO_PROPERTY, NULL);
    }
    return indigo_rotator_enumerate_properties(device, NULL, NULL);
}

static void hucha_timer_callback(indigo_device *device)
{
    if (HUCHA_DINERO_PROPERTY->state == INDIGO_ALERT_STATE)
    {
        HUCHA_DINERO_ITEM->number.value = PRIVATE_DATA->target_dinerico = PRIVATE_DATA->current_dinerico;
        indigo_update_property(device, HUCHA_DINERO_PROPERTY, NULL);
    }
    else
    {
        if (PRIVATE_DATA->current_dinerico < PRIVATE_DATA->target_dinerico)
        {
            HUCHA_DINERO_PROPERTY->state = INDIGO_BUSY_STATE;
            if (PRIVATE_DATA->target_dinerico - PRIVATE_DATA->current_dinerico > AHORRA_SPEED)
                HUCHA_DINERO_ITEM->number.value = PRIVATE_DATA->current_dinerico = (PRIVATE_DATA->current_dinerico + AHORRA_SPEED);
            else
                HUCHA_DINERO_ITEM->number.value = PRIVATE_DATA->current_dinerico = PRIVATE_DATA->target_dinerico;
            indigo_update_property(device, HUCHA_DINERO_PROPERTY, NULL);
            indigo_reschedule_timer(device, 1, &PRIVATE_DATA->hucha_timer);
        }
        else
        {
            HUCHA_DINERO_PROPERTY->state = INDIGO_OK_STATE;
            HUCHA_DINERO_ITEM->number.value = PRIVATE_DATA->current_dinerico;
            indigo_update_property(device, HUCHA_DINERO_PROPERTY, NULL);
        }
    }
}

static indigo_result hucha_attach(indigo_device *device)
{
    assert(device != NULL);
    assert(PRIVATE_DATA != NULL);
    if (indigo_rotator_attach(device, DRIVER_NAME, DRIVER_VERSION) == INDIGO_OK)
    {
        // -------------------------------------------------------------------------------- Inicio la propiedad inventada
        HUCHA_DINERO_PROPERTY = indigo_init_number_property(NULL, device->name, "HUCHA_DINERO", NULL, "Ahorra dinerico", INDIGO_OK_STATE, INDIGO_RW_PERM, 1);
        if (HUCHA_DINERO_PROPERTY == NULL)
            return INDIGO_FAILED;
        indigo_init_number_item(HUCHA_DINERO_ITEM, "DINERO", "Ahorra", 0, 10000, AHORRA_SPEED, 10);
        strcpy(HUCHA_DINERO_ITEM->number.format, "%g");
        // --------------------------------------------------------------------------------
        INDIGO_DEVICE_ATTACH_LOG(DRIVER_NAME, device->name);
        return indigo_rotator_enumerate_properties(device, NULL, NULL);
    }
}

static void hucha_connect_callback(indigo_device *device)
{
    CONNECTION_PROPERTY->state = INDIGO_OK_STATE;
    if (CONNECTION_CONNECTED_ITEM->sw.value)
    {
        INDIGO_DRIVER_LOG(DRIVER_NAME, "Conectado");
        indigo_define_property(device, HUCHA_DINERO_PROPERTY, NULL);
        CONNECTION_PROPERTY->state = INDIGO_OK_STATE;
    }
    else
    {
        indigo_delete_property(device, HUCHA_DINERO_PROPERTY, NULL);
        INDIGO_DRIVER_LOG(DRIVER_NAME, "Desconectado");
        CONNECTION_PROPERTY->state = INDIGO_OK_STATE;
    }
    indigo_aux_change_property(device, NULL, CONNECTION_PROPERTY);
}

static void hucha_dinero_handler(indigo_device *device)
{
    indigo_set_timer(device, HUCHA_DINERO_ITEM->number.value < 1 ? HUCHA_DINERO_ITEM->number.value : 1, hucha_timer_callback, &PRIVATE_DATA->hucha_timer);
    indigo_update_property(device, CONNECTION_PROPERTY, NULL);
}

static indigo_result hucha_change_property(indigo_device *device, indigo_client *client, indigo_property *property)
{
    assert(device != NULL);
    assert(DEVICE_CONTEXT != NULL);
    assert(property != NULL);
    if (indigo_property_match(CONNECTION_PROPERTY, property))
    {
        // -------------------------------------------------------------------------------- CONNECTION
        if (indigo_ignore_connection_change(device, property))
            return INDIGO_OK;
        indigo_property_copy_values(CONNECTION_PROPERTY, property, false);
        CONNECTION_PROPERTY->state = INDIGO_BUSY_STATE;
        indigo_update_property(device, CONNECTION_PROPERTY, NULL);
        indigo_set_timer(device, 0, hucha_connect_callback, NULL);
        return INDIGO_OK;
    }
    else if (indigo_property_match(HUCHA_DINERO_PROPERTY, property))
    {
        indigo_property_copy_values(HUCHA_DINERO_PROPERTY, property, false);
        HUCHA_DINERO_PROPERTY->state = INDIGO_BUSY_STATE;
        indigo_update_property(device, HUCHA_DINERO_PROPERTY, NULL);
        indigo_set_timer(device, 0, hucha_dinero_handler, NULL);
        return INDIGO_OK;
    }
    return indigo_rotator_change_property(device, client, property);
}

static indigo_result hucha_detach(indigo_device *device)
{
    assert(device != NULL);
    if (IS_CONNECTED)
    {
        indigo_set_switch(CONNECTION_PROPERTY, CONNECTION_DISCONNECTED_ITEM, true);
        hucha_connect_callback(device);
    }
    indigo_release_property(HUCHA_DINERO_PROPERTY);
    INDIGO_DEVICE_DETACH_LOG(DRIVER_NAME, device->name);
    return indigo_rotator_detach(device);
}

static hucha_private_data *private_data = NULL;
static indigo_device *hucha = NULL;

indigo_result indigo_simple_driver(indigo_driver_action action, indigo_driver_info *info)
{
    static indigo_device hucha_template = INDIGO_DEVICE_INITIALIZER(
        "CHEMA",
        hucha_attach,
        hucha_enumerate_properties,
        hucha_change_property,
        NULL,
        hucha_detach);

    static indigo_driver_action last_action = INDIGO_DRIVER_SHUTDOWN;

    SET_DRIVER_INFO(info, "BAD BUNNY", __FUNCTION__, DRIVER_VERSION, true, last_action);

    if (action == last_action)
        return INDIGO_OK;

    switch (action)
    {
    case INDIGO_DRIVER_INIT:
        last_action = action;
        private_data = indigo_safe_malloc(sizeof(hucha_private_data));
        hucha = indigo_safe_malloc_copy(sizeof(indigo_device), &hucha_template);
        hucha->private_data = private_data;
        indigo_attach_device(hucha);
        break;

    case INDIGO_DRIVER_SHUTDOWN:
        VERIFY_NOT_CONNECTED(hucha);
        last_action = action;
        if (hucha != NULL)
        {
            indigo_detach_device(hucha);
            free(hucha);
            hucha = NULL;
        }
        if (private_data != NULL)
        {
            free(private_data);
            private_data = NULL;
        }
        break;

    case INDIGO_DRIVER_INFO:
        break;
    }
    return INDIGO_OK;
}
