// Changes the color of your Nintendo Switch Controller. Usable via wired or Bluetooth, just change the bluetooth boolean.
// g++ -o recolor -lhidapi-hidraw ns_colorchange.cpp
#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <chrono>
#include <ctime>
#include <cstring>
#include <hidapi/hidapi.h>

#define JOYCON_L_BT (0x2006)
#define JOYCON_R_BT (0x2007)
#define PRO_CONTROLLER (0x2009)
#define JOYCON_CHARGING_GRIP (0x200e)
unsigned short product_ids_size = 4;
unsigned short product_ids[] = {JOYCON_L_BT, JOYCON_R_BT, PRO_CONTROLLER, JOYCON_CHARGING_GRIP};

// replace the values after 0x with the proper ones for your color.
// #404997 would be 0x40, 0x49, 0x97
// #cecccf would be 0xCE, 0xCC, 0xCF
unsigned char body[] = {
  0x40, 0x49, 0x97
};
unsigned char sticks[] = {
  0xCE, 0xCC, 0xCF
};

// IMPORTANT BOOLEAN
// true means your controller will be connected via bluetooth when you run the script
// false means it will be connected via a wired connection. to use joycon with a wired
// connecton, you will need a charging grip.
bool bluetooth = false;


uint8_t global_count = 0;

void hex_dump(unsigned char *buf, int len)
{
    for (int i = 0; i < len; i++)
        printf("%02x ", buf[i]);
    printf("\n");
}

void hid_exchange(hid_device *handle, unsigned char *buf, int len)
{
    if(!handle) return;

    hid_write(handle, buf, len);

    int res = hid_read(handle, buf, 0x400);
}

int hid_dual_exchange(hid_device *handle_l, hid_device *handle_r, unsigned char *buf_l, unsigned char *buf_r, int len)
{
    int res = -1;

    if(handle_l && buf_l)
    {
        hid_set_nonblocking(handle_l, 1);
        res = hid_write(handle_l, buf_l, len);
        res = hid_read(handle_l, buf_l, 0x400);
        hid_set_nonblocking(handle_l, 0);
    }

    if(handle_r && buf_r)
    {
        hid_set_nonblocking(handle_r, 1);
        res = hid_write(handle_r, buf_r, len);
        do
        {
            res = hid_read(handle_r, buf_r, 0x400);
        }
        while(!res);
        hid_set_nonblocking(handle_r, 0);
    }

    return res;
}

void joycon_send_command(hid_device *handle, int command, uint8_t *data, int len)
{
    unsigned char buf[0x400];
    memset(buf, 0, 0x400);

    if(!bluetooth)
    {
        buf[0x00] = 0x80;
        buf[0x01] = 0x92;
        buf[0x03] = 0x31;
    }

    buf[bluetooth ? 0x0 : 0x8] = command;
    if(data != NULL && len != 0)
        memcpy(buf + (bluetooth ? 0x1 : 0x9), data, len);

    hid_exchange(handle, buf, len + (bluetooth ? 0x1 : 0x9));

    if(data)
        memcpy(data, buf, 0x40);
}

void joycon_send_subcommand(hid_device *handle, int command, int subcommand, uint8_t *data, int len)
{
    unsigned char buf[0x400];
    memset(buf, 0, 0x400);

    uint8_t rumble_base[9] = {(++global_count) & 0xF, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};
    memcpy(buf, rumble_base, 9);

    buf[9] = subcommand;
    if(data && len != 0)
        memcpy(buf + 10, data, len);

    joycon_send_command(handle, command, buf, 10 + len);

    if(data)
        memcpy(data, buf, 0x40);
}

void spi_write(hid_device *handle, uint32_t offs, uint8_t *data, uint8_t len)
{
    unsigned char buf[0x400];
    uint8_t *spi_write = (uint8_t*)calloc(1, 0x26 * sizeof(uint8_t));
    uint32_t* offset = (uint32_t*)(&spi_write[0]);
    uint8_t* length = (uint8_t*)(&spi_write[4]);

    *length = len;
    *offset = offs;
    memcpy(&spi_write[0x5], data, len);

    int max_write_count = 2000;
    int write_count = 0;
    do
    {
        write_count += 1;
        memcpy(buf, spi_write, 0x39);
        joycon_send_subcommand(handle, 0x1, 0x11, buf, 0x26);
    }
    while((buf[0x10 + (bluetooth ? 0 : 10)] != 0x11 && buf[0] != (bluetooth ? 0x21 : 0x81))
        && write_count < max_write_count);
	if(write_count > max_write_count)
        printf("ERROR: Write error or timeout\nSkipped writing of %dBytes at address 0x%05X...\n",
            *length, *offset);
}

void spi_read(hid_device *handle, uint32_t offs, uint8_t *data, uint8_t len)
{
    unsigned char buf[0x400];
    uint8_t *spi_read_cmd = (uint8_t*)calloc(1, 0x26 * sizeof(uint8_t));
    uint32_t* offset = (uint32_t*)(&spi_read_cmd[0]);
    uint8_t* length = (uint8_t*)(&spi_read_cmd[4]);

    *length = len;
    *offset = offs;

    int max_read_count = 2000;
	int read_count = 0;
    do
    {
        //usleep(300000);
		read_count += 1;
        memcpy(buf, spi_read_cmd, 0x36);
        joycon_send_subcommand(handle, 0x1, 0x10, buf, 0x26);
    }
    while(*(uint32_t*)&buf[0xF + (bluetooth ? 0 : 10)] != *offset && read_count < max_read_count);
	if(read_count > max_read_count)
        printf("ERROR: Read error or timeout\nSkipped reading of %dBytes at address 0x%05X...\n",
            *length, *offset);


    memcpy(data, &buf[0x14 + (bluetooth ? 0 : 10)], len);
}

int joycon_init(hid_device *handle, const wchar_t *name)
{
    unsigned char buf[0x400];
    unsigned char sn_buffer[14] = {0x00};
    memset(buf, 0, 0x400);


    if(!bluetooth)
    {
        // Get MAC Left
        memset(buf, 0x00, 0x400);
        buf[0] = 0x80;
        buf[1] = 0x01;
        hid_exchange(handle, buf, 0x2);

        if(buf[2] == 0x3)
        {
            printf("%ls disconnected!\n", name);
            return -1;
        }
        else
        {
            printf("Found %ls, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", name,
                   buf[9], buf[8], buf[7], buf[6], buf[5], buf[4]);
        }

        // Do handshaking
        memset(buf, 0x00, 0x400);
        buf[0] = 0x80;
        buf[1] = 0x02;
        hid_exchange(handle, buf, 0x2);

        printf("Switching baudrate...\n");

        // Switch baudrate to 3Mbit
        memset(buf, 0x00, 0x400);
        buf[0] = 0x80;
        buf[1] = 0x03;
        hid_exchange(handle, buf, 0x2);

        // Do handshaking again at new baudrate so the firmware pulls pin 3 low?
        memset(buf, 0x00, 0x400);
        buf[0] = 0x80;
        buf[1] = 0x02;
        hid_exchange(handle, buf, 0x2);

        // Only talk HID from now on
        memset(buf, 0x00, 0x400);
        buf[0] = 0x80;
        buf[1] = 0x04;
        hid_exchange(handle, buf, 0x2);
    }

    // Enable vibration
    memset(buf, 0x00, 0x400);
    buf[0] = 0x01; // Enabled
    joycon_send_subcommand(handle, 0x1, 0x48, buf, 1);

    // Enable IMU data
    memset(buf, 0x00, 0x400);
    buf[0] = 0x01; // Enabled
    joycon_send_subcommand(handle, 0x1, 0x40, buf, 1);

    // Increase data rate for Bluetooth
    if (bluetooth)
    {
       memset(buf, 0x00, 0x400);
       buf[0] = 0x31; // Enabled
       joycon_send_subcommand(handle, 0x1, 0x3, buf, 1);
    }

    //Read device's S/N
    spi_read(handle, 0x6002, sn_buffer, 0xE);

    printf("Successfully initialized %ls with S/N: %c%c%c%c%c%c%c%c%c%c%c%c%c%c!\n",
        name, sn_buffer[0], sn_buffer[1], sn_buffer[2], sn_buffer[3],
        sn_buffer[4], sn_buffer[5], sn_buffer[6], sn_buffer[7], sn_buffer[8],
        sn_buffer[9], sn_buffer[10], sn_buffer[11], sn_buffer[12],
        sn_buffer[13]);

    return 0;
}

void joycon_deinit(hid_device *handle, const wchar_t *name)
{
    unsigned char buf[0x400];
    memset(buf, 0x00, 0x400);

    //Let the Joy-Con talk BT again
    if(!bluetooth)
    {
        buf[0] = 0x80;
        buf[1] = 0x05;
        hid_exchange(handle, buf, 0x2);
    }

    printf("Deinitialized %ls\n", name);
}

void device_print(struct hid_device_info *dev)
{
    printf("USB device info:\n  vid: 0x%04hX pid: 0x%04hX\n  path: %s\n  MAC: %ls\n  interface_number: %d\n",
        dev->vendor_id, dev->product_id, dev->path, dev->serial_number, dev->interface_number);
    printf("  Manufacturer: %ls\n", dev->manufacturer_string);
    printf("  Product:      %ls\n\n", dev->product_string);
}

int main(int argc, char* argv[])
{
    int res;
    unsigned char buf[2][0x400] = {0};
    hid_device *handle_l = 0, *handle_r = 0;
    const wchar_t *device_name = L"none";
    struct hid_device_info *devs, *dev_iter;
    bool charging_grip = false;

    setbuf(stdout, NULL); // turn off stdout buffering for test reasons

    res = hid_init();
    if(res)
    {
        printf("Failed to open hid library! Exiting...\n");
        return -1;
    }

    // iterate thru all the valid product ids and try and initialize controllers
    for(int i = 0; i < product_ids_size; i++)
    {
        devs = hid_enumerate(0x057E, product_ids[i]);
        dev_iter = devs;
        while(dev_iter)
        {
            // Sometimes hid_enumerate still returns other product IDs
            if (dev_iter->product_id != product_ids[i]) break;

            // break out if the current handle is already used
            if((dev_iter->product_id == JOYCON_R_BT || dev_iter->interface_number == 0) && handle_r)
                break;
            else if((dev_iter->product_id == JOYCON_L_BT || dev_iter->interface_number == 1) && handle_l)
                break;

            device_print(dev_iter);

            if(!wcscmp(dev_iter->serial_number, L"000000000001"))
            {
                bluetooth = false;
            }
            else if(!bluetooth)
            {
                printf("Can't mix USB HID with Bluetooth HID, exiting...\n");
                return -1;
            }

            // on windows this will be -1 for devices with one interface
            if(dev_iter->interface_number == 0 || dev_iter->interface_number == -1)
            {
                hid_device *handle = hid_open_path(dev_iter->path);
                if(handle == NULL)
                {
                    printf("Failed to open controller at %ls, continuing...\n", dev_iter->path);
                    dev_iter = dev_iter->next;
                    continue;
                }

                switch(dev_iter->product_id)
                {
                    case JOYCON_CHARGING_GRIP:
                        device_name = L"Joy-Con (R)";
                        charging_grip = true;

                        handle_r = handle;
                        break;
                    case PRO_CONTROLLER:
                        device_name = L"Pro Controller";

                        handle_r = handle;
                        break;
                    case JOYCON_L_BT:
                        device_name = L"Joy-Con (L)";

                        handle_l = handle;
                        break;
                    case JOYCON_R_BT:
                        device_name = L"Joy-Con (R)";

                        handle_r = handle;
                        break;
                }

                if(joycon_init(handle, device_name))
                {
                    hid_close(handle);
                    if(dev_iter->product_id != JOYCON_L_BT)
                        handle_r = NULL;
                    else
                        handle_l = NULL;
                }
            }
            // Only exists for left Joy-Con in the charging grip
            else if(dev_iter->interface_number == 1)
            {
                handle_l = hid_open_path(dev_iter->path);
                if(handle_l == NULL)
                {
                    printf("Failed to open controller at %ls, continuing...\n", dev_iter->path);
                    dev_iter = dev_iter->next;
                    continue;
                }

                if(joycon_init(handle_l, L"Joy-Con (L)"))
                    handle_l = NULL;
            }
            dev_iter = dev_iter->next;
        }
        hid_free_enumeration(devs);
    }

    if(!handle_r)
    {
        printf("Failed to get handle for right Joy-Con or Pro Controller, exiting...\n");
        return -1;
    }

    // Only missing one half by this point
    if(!handle_l && charging_grip)
    {
        printf("Could not get handles for both Joy-Con in grip! Exiting...\n");
        return -1;
    }

    // controller init is complete at this point



    // Joy-Con color data, body RGB #E8B31C and button RGB #1C1100
    unsigned char color_buffer[6] = {body[0], body[1], body[2], sticks[0], sticks[1], sticks[2]};

    printf("Changing body color to #%02x%02x%02x, buttons to #%02x%02x%02x\n",
           color_buffer[0], color_buffer[1], color_buffer[2],
           color_buffer[3], color_buffer[4], color_buffer[5]);
    printf("It's probably safe to exit while this is going, but please wait while it writes...\n");

    spi_write(handle_r, 0x6050, color_buffer, 6);
    if(handle_l)
        spi_write(handle_l, 0x6050, color_buffer, 6);
    printf("Writes completed.\n");

    unsigned long last = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    int input_count = 0;

    if(handle_l)
    {
        joycon_deinit(handle_l, L"Joy-Con (L)");
        hid_close(handle_l);
    }

    if(handle_r)
    {
        joycon_deinit(handle_r, device_name);
        hid_close(handle_r);
    }

    // Finalize the hidapi library
    res = hid_exit();

    return 0;
}
