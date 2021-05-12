#include <linux/module.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/ftrace.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/string.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilyasov Idris");
MODULE_DESCRIPTION("Control mouse pointer by gamepad");


struct gamepad 
{
    struct input_handler *handler;
    struct input_handle *handle;
    struct input_dev *dev;
};

typedef struct gamepad gamepad_t;


struct event_work 
{
    struct input_event event;
    struct work_struct work;
};


struct workqueue_struct *queue;
struct input_dev *mouse;
gamepad_t *gamepad;	


void gamepad_mouse_events(struct work_struct *work) 
{
    struct event_work *event = container_of(work, struct event_work, work);
    // printk(KERN_INFO "Type: %d, Code: %d, Value: %d", event->event.type, event->event.code, event->event.value);
    
    if (event->event.type == EV_ABS)
    {
    	if (event->event.code == ABS_X)
    	{
            input_report_rel(mouse, REL_X, (event->event.value) / 7500);
            printk(KERN_INFO "mouse_gamepad: Mouse pointer moving along the OX axis");
    	}
    	
    	if (event->event.code == ABS_Y)
    	{
            input_report_rel(mouse, REL_Y, (event->event.value) / 7500);
            printk(KERN_INFO "mouse_gamepad: Mouse pointer moving along the OY axis");
    	}
    	
    	if (event->event.code == ABS_HAT0X)
    	{
            input_report_rel(mouse, REL_HWHEEL, (event->event.value) * 2);
            printk(KERN_INFO "mouse_gamepad: Horizontal scrolling by dpad");
    	}
    	
    	if (event->event.code == ABS_HAT0Y)
    	{
            input_report_rel(mouse, REL_WHEEL, -(event->event.value));
            printk(KERN_INFO "mouse_gamepad: Vertical scrolling by dpad");
    	}

    	input_sync(mouse);
    }
    
    else if (event->event.type == EV_KEY) 
    {
        if (event->event.code == BTN_A)
        {
            if (event->event.value == 1)
            {
                input_report_key(mouse, BTN_LEFT, 1);
                printk(KERN_INFO "mouse_gamepad: Button A (left mouse button) was pressed");
            }
            else if (event->event.value == 0)
            {
                input_report_key(mouse, BTN_LEFT, 0);
                printk(KERN_INFO "mouse_gamepad: Button A (left mouse button) was released");
            }
        }
	    
        if (event->event.code == BTN_B)
        {
            if (event->event.value == 1)
            {
                input_report_key(mouse, BTN_RIGHT, 1);
                printk(KERN_INFO "mouse_gamepad: Button B (right mouse button) was pressed");
	    	}
            else if (event->event.value == 0)
            {
	            input_report_key(mouse, BTN_RIGHT, 0);
	            printk(KERN_INFO "mouse_gamepad: Button B (right mouse button) was released");
            }
        }
	    
        input_sync(mouse);
    }
	
    kfree(event);
}


static void gamepad_event(struct input_handle *handle, unsigned int type, unsigned int code, int value) 
{
    struct event_work *work = kzalloc(sizeof(struct event_work), GFP_ATOMIC);
    work->event.code = code;
    work->event.type = type;
    work->event.value = value;

    INIT_WORK(&work->work, gamepad_mouse_events);
    queue_work(queue, &work->work);
}


static int gamepad_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id) 
{
    struct input_handle *handle;
    int result;
    
    if (strcmp(dev->name, "Microsoft X-Box One S pad"))
    {
    	return 0;
    }
	 		
    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);

    if (!handle)
    {
        return -ENOMEM;
    }
        
    gamepad->handle = handle;
    handle->dev = dev;
    handle->handler = handler;

    result = input_register_handle(handle);
    if (result) 
    {
        kfree(handle);
        return result;
    }
       
    result = input_open_device(handle);
    if (result) 
    {
        input_unregister_handle(handle);
        kfree(handle);
        return result;
    }
    
    printk(KERN_INFO "%s", dev->name);
    return 0;
}


static void gamepad_disconnect(struct input_handle *handle) 
{
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}


static const struct input_device_id gamepad_ids[] = 
{
    {
    .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
    .evbit = { BIT_MASK(EV_KEY) },
    },
    
    {
    .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
    .evbit = { BIT_MASK(EV_REL) },
    },

    {
    .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
    .evbit = { BIT_MASK(EV_ABS) },
    },
    
    {
    .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
    .evbit = { BIT_MASK(EV_MSC) },
    },
};


static struct input_handler gamepad_handler = 
{
    .event = gamepad_event,
    .connect = gamepad_connect,
    .disconnect = gamepad_disconnect,
    .name = "gamepad",
    .id_table = gamepad_ids,
};


int __init gamepad_init(void) 
{
    int result;
    queue = create_workqueue("mouse_gamepad");
    if (!queue) 
    {
        printk(KERN_ERR "mouse_gamepad: workqueue wasn't allocated\n");
        return -1;
    }

    mouse = input_allocate_device();
    mouse->name = "virtual_mouse";
	
	
    gamepad = kzalloc(sizeof(struct gamepad), GFP_KERNEL);
    gamepad->handler = &gamepad_handler;
	
	
    result = input_register_handler(&gamepad_handler);
    if (result)
    {
        return result;
    }

    gamepad->dev = input_allocate_device();
    
    if (!gamepad->dev) 
    {
        printk(KERN_ALERT "mouse_gamepad: Bad input_allocate_device()\n");
        return -1;
    }
	
    set_bit(EV_REL, mouse->evbit);
    set_bit(REL_X, mouse->relbit);
    set_bit(REL_Y, mouse->relbit);
    set_bit(REL_WHEEL, mouse->relbit);
    set_bit(REL_HWHEEL, mouse->relbit);
	
    set_bit(EV_KEY, mouse->evbit);
    set_bit(BTN_LEFT, mouse->keybit);
    set_bit(BTN_RIGHT, mouse->keybit);
	
    input_register_device(gamepad->dev);
    input_register_device(mouse);

    printk("mouse_gamepad: Module was loaded\n");
    return 0;
}


void __exit gamepad_exit(void) 
{
    if (gamepad->dev)
    {
        input_unregister_device(gamepad->dev);
    }
    
    if (mouse)
    {
        input_unregister_device(mouse);
    }
    
    input_unregister_handler(gamepad->handler);
    flush_workqueue(queue);
    destroy_workqueue(queue);
    kfree(gamepad);
    printk("mouse_gamepad: Module was unloaded\n");
}


module_init(gamepad_init);
module_exit(gamepad_exit);
