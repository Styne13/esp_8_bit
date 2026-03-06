
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <vector>
#include <string>
using namespace std;

#include "hci_server.h"
#include "hid_server.h"

enum {
    HID_SDP_PSM = 0x0001,
    HID_CONTROL_PSM = 0x0011,
    HID_INTERRUPT_PSM = 0x0013,
};

class SDPParse;
class InputDevice {
public:

    enum State {
        CLOSED,
        CONNECT,
        READING_NAME,
        READING_SDP,
        AUTHENTICATING,
        OPENING,
        OPEN
    };

    InputDevice();
    ~InputDevice();

    State _state;
    bdaddr_t _bdaddr;
    int _dev_class;
    string _name;

    SDPParse* _sdp_handler;
    string _hid_descriptor;

    bool _reconnect;
    int _sdp;
    int _control;
    int _interrupt;

    int _wii_index;

    const char* name();
    void connect();
    void authentication_complete(int status);
    void connection_request();
    void connection_complete(int status);
    void remote_name_response(const char* name);
    void disconnection_complete();
    void socket_changed(int socket, int state);

    void start_sdp();
    void update_sdp();
};

//==================================================================
//==================================================================
// clunky sdp

/* AttrID name lookup table */
typedef struct {
    int   attr_id;
    const char* name;
} sdp_attr_id_nam_lookup_table_t;

#define SDP_ATTR_ID_SERVICE_RECORD_HANDLE              0x0000
#define SDP_ATTR_ID_SERVICE_CLASS_ID_LIST              0x0001
#define SDP_ATTR_ID_SERVICE_RECORD_STATE               0x0002
#define SDP_ATTR_ID_SERVICE_SERVICE_ID                 0x0003
#define SDP_ATTR_ID_PROTOCOL_DESCRIPTOR_LIST           0x0004
#define SDP_ATTR_ID_BROWSE_GROUP_LIST                  0x0005
#define SDP_ATTR_ID_LANGUAGE_BASE_ATTRIBUTE_ID_LIST    0x0006
#define SDP_ATTR_ID_SERVICE_INFO_TIME_TO_LIVE          0x0007
#define SDP_ATTR_ID_SERVICE_AVAILABILITY               0x0008
#define SDP_ATTR_ID_BLUETOOTH_PROFILE_DESCRIPTOR_LIST  0x0009
#define SDP_ATTR_ID_DOCUMENTATION_URL                  0x000A
#define SDP_ATTR_ID_CLIENT_EXECUTABLE_URL              0x000B
#define SDP_ATTR_ID_ICON_URL                           0x000C
#define SDP_ATTR_ID_ADDITIONAL_PROTOCOL_DESC_LISTS     0x000D
#define SDP_ATTR_ID_SERVICE_NAME                       0x0100
#define SDP_ATTR_ID_SERVICE_DESCRIPTION                0x0101
#define SDP_ATTR_ID_PROVIDER_NAME                      0x0102
#define SDP_ATTR_ID_VERSION_NUMBER_LIST                0x0200
#define SDP_ATTR_ID_GROUP_ID                           0x0200
#define SDP_ATTR_ID_SERVICE_DATABASE_STATE             0x0201
#define SDP_ATTR_ID_SERVICE_VERSION                    0x0300

#define SDP_ATTR_ID_EXTERNAL_NETWORK                   0x0301 /* Cordless Telephony */
#define SDP_ATTR_ID_SUPPORTED_DATA_STORES_LIST         0x0301 /* Synchronization */
#define SDP_ATTR_ID_REMOTE_AUDIO_VOLUME_CONTROL        0x0302 /* GAP */
#define SDP_ATTR_ID_SUPPORTED_FORMATS_LIST             0x0303 /* OBEX Object Push */
#define SDP_ATTR_ID_FAX_CLASS_1_SUPPORT                0x0302 /* Fax */
#define SDP_ATTR_ID_FAX_CLASS_2_0_SUPPORT              0x0303
#define SDP_ATTR_ID_FAX_CLASS_2_SUPPORT                0x0304
#define SDP_ATTR_ID_AUDIO_FEEDBACK_SUPPORT             0x0305
#define SDP_ATTR_ID_SECURITY_DESCRIPTION               0x030a /* PAN */
#define SDP_ATTR_ID_NET_ACCESS_TYPE                    0x030b /* PAN */
#define SDP_ATTR_ID_MAX_NET_ACCESS_RATE                0x030c /* PAN */
#define SDP_ATTR_ID_IPV4_SUBNET                        0x030d /* PAN */
#define SDP_ATTR_ID_IPV6_SUBNET                        0x030e /* PAN */

#define SDP_ATTR_ID_SUPPORTED_CAPABILITIES             0x0310 /* Imaging */
#define SDP_ATTR_ID_SUPPORTED_FEATURES                 0x0311 /* Imaging and Hansfree */
#define SDP_ATTR_ID_SUPPORTED_FUNCTIONS                0x0312 /* Imaging */
#define SDP_ATTR_ID_TOTAL_IMAGING_DATA_CAPACITY        0x0313 /* Imaging */
#define SDP_ATTR_ID_SUPPORTED_REPOSITORIES             0x0314 /* PBAP */

#define SDP_ATTR_HID_DEVICE_RELEASE_NUMBER    0x0200
#define SDP_ATTR_HID_PARSER_VERSION        0x0201
#define SDP_ATTR_HID_DEVICE_SUBCLASS        0x0202
#define SDP_ATTR_HID_COUNTRY_CODE        0x0203
#define SDP_ATTR_HID_VIRTUAL_CABLE        0x0204
#define SDP_ATTR_HID_RECONNECT_INITIATE        0x0205
#define SDP_ATTR_HID_DESCRIPTOR_LIST        0x0206
#define SDP_ATTR_HID_LANG_ID_BASE_LIST        0x0207

#define SDP_ATTR_HID_SDP_DISABLE        0x0208
#define SDP_ATTR_HID_BATTERY_POWER        0x0209
#define SDP_ATTR_HID_REMOTE_WAKEUP        0x020a
#define SDP_ATTR_HID_PROFILE_VERSION        0x020b
#define SDP_ATTR_HID_SUPERVISION_TIMEOUT    0x020c
#define SDP_ATTR_HID_NORMALLY_CONNECTABLE    0x020d
#define SDP_ATTR_HID_BOOT_DEVICE        0x020e


static sdp_attr_id_nam_lookup_table_t sdp_attr_id_nam_lookup_table[] = {
    { SDP_ATTR_HID_DEVICE_RELEASE_NUMBER, "SDP_ATTR_HID_DEVICE_RELEASE_NUMBER"},
    { SDP_ATTR_HID_PARSER_VERSION, "SDP_ATTR_HID_PARSER_VERSION"},
    { SDP_ATTR_HID_DEVICE_SUBCLASS, "SDP_ATTR_HID_DEVICE_SUBCLASS"},
    { SDP_ATTR_HID_COUNTRY_CODE, "SDP_ATTR_HID_COUNTRY_CODE"},
    { SDP_ATTR_HID_VIRTUAL_CABLE, "SDP_ATTR_HID_VIRTUAL_CABLE"},
    { SDP_ATTR_HID_RECONNECT_INITIATE, "SDP_ATTR_HID_RECONNECT_INITIATE"},
    { SDP_ATTR_HID_DESCRIPTOR_LIST, "SDP_ATTR_HID_DESCRIPTOR_LIST"},
    { SDP_ATTR_HID_LANG_ID_BASE_LIST, "SDP_ATTR_HID_LANG_ID_BASE_LIST"},

    { SDP_ATTR_HID_SDP_DISABLE, "SDP_ATTR_HID_SDP_DISABLE"},
    { SDP_ATTR_HID_BATTERY_POWER, "SDP_ATTR_HID_BATTERY_POWER"},
    { SDP_ATTR_HID_REMOTE_WAKEUP, "SDP_ATTR_HID_REMOTE_WAKEUP"},
    { SDP_ATTR_HID_SDP_DISABLE, "SDP_ATTR_HID_SDP_DISABLE"},
    { SDP_ATTR_HID_PROFILE_VERSION, "SDP_ATTR_HID_PROFILE_VERSION"},
    { SDP_ATTR_HID_SUPERVISION_TIMEOUT, "SDP_ATTR_HID_SUPERVISION_TIMEOUT"},
    { SDP_ATTR_HID_NORMALLY_CONNECTABLE, "SDP_ATTR_HID_NORMALLY_CONNECTABLE"},
    { SDP_ATTR_HID_BOOT_DEVICE, "SDP_ATTR_HID_BOOT_DEVICE"},

    { SDP_ATTR_ID_SERVICE_RECORD_HANDLE,             "SrvRecHndl"         },
    { SDP_ATTR_ID_SERVICE_CLASS_ID_LIST,             "SrvClassIDList"     },
    { SDP_ATTR_ID_SERVICE_RECORD_STATE,              "SrvRecState"        },
    { SDP_ATTR_ID_SERVICE_SERVICE_ID,                "SrvID"              },
    { SDP_ATTR_ID_PROTOCOL_DESCRIPTOR_LIST,          "ProtocolDescList"   },
    { SDP_ATTR_ID_BROWSE_GROUP_LIST,                 "BrwGrpList"         },
    { SDP_ATTR_ID_LANGUAGE_BASE_ATTRIBUTE_ID_LIST,   "LangBaseAttrIDList" },
    { SDP_ATTR_ID_SERVICE_INFO_TIME_TO_LIVE,         "SrvInfoTimeToLive"  },
    { SDP_ATTR_ID_SERVICE_AVAILABILITY,              "SrvAvail"           },
    { SDP_ATTR_ID_BLUETOOTH_PROFILE_DESCRIPTOR_LIST, "BTProfileDescList"  },
    { SDP_ATTR_ID_DOCUMENTATION_URL,                 "DocURL"             },
    { SDP_ATTR_ID_CLIENT_EXECUTABLE_URL,             "ClientExeURL"       },
    { SDP_ATTR_ID_ICON_URL,                          "IconURL"            },
    { SDP_ATTR_ID_ADDITIONAL_PROTOCOL_DESC_LISTS,    "AdditionalProtocolDescLists" },
    { SDP_ATTR_ID_SERVICE_NAME,                      "SrvName"            },
    { SDP_ATTR_ID_SERVICE_DESCRIPTION,               "SrvDesc"            },
    { SDP_ATTR_ID_PROVIDER_NAME,                     "ProviderName"       },
    { SDP_ATTR_ID_VERSION_NUMBER_LIST,               "VersionNumList"     },
    { SDP_ATTR_ID_GROUP_ID,                          "GrpID"              },
    { SDP_ATTR_ID_SERVICE_DATABASE_STATE,            "SrvDBState"         },
    { SDP_ATTR_ID_SERVICE_VERSION,                   "SrvVersion"         },
    { SDP_ATTR_ID_SECURITY_DESCRIPTION,              "SecurityDescription"}, /* PAN */
    { SDP_ATTR_ID_SUPPORTED_DATA_STORES_LIST,        "SuppDataStoresList" }, /* Synchronization */
    { SDP_ATTR_ID_SUPPORTED_FORMATS_LIST,            "SuppFormatsList"    }, /* OBEX Object Push */
    { SDP_ATTR_ID_NET_ACCESS_TYPE,                   "NetAccessType"      }, /* PAN */
    { SDP_ATTR_ID_MAX_NET_ACCESS_RATE,               "MaxNetAccessRate"   }, /* PAN */
    { SDP_ATTR_ID_IPV4_SUBNET,                       "IPv4Subnet"         }, /* PAN */
    { SDP_ATTR_ID_IPV6_SUBNET,                       "IPv6Subnet"         }, /* PAN */
    { SDP_ATTR_ID_SUPPORTED_CAPABILITIES,            "SuppCapabilities"   }, /* Imaging */
    { SDP_ATTR_ID_SUPPORTED_FEATURES,                "SuppFeatures"       }, /* Imaging and Hansfree */
    { SDP_ATTR_ID_SUPPORTED_FUNCTIONS,               "SuppFunctions"      }, /* Imaging */
    { SDP_ATTR_ID_TOTAL_IMAGING_DATA_CAPACITY,       "SuppTotalCapacity"  }, /* Imaging */
    { SDP_ATTR_ID_SUPPORTED_REPOSITORIES,            "SuppRepositories"   }, /* PBAP */
    { -1, NULL }
};

static
const char* get_name(int id)
{
    for (int i = 0; sdp_attr_id_nam_lookup_table[i].name; i++)
        if (sdp_attr_id_nam_lookup_table[i].attr_id == id)
            return sdp_attr_id_nam_lookup_table[i].name;
    return "nope?";
}

class SDPParse {
    int _sdp_sock;
    int _current_id;
    const uint8_t *_data;
    vector<uint8_t> _buf;   // accumulate full search results before parsing

    string _SrvName;
    string _SrvDesc;
    string _ProviderName;
    string _HIDDescriptor;
public:
    SDPParse(int sock) : _sdp_sock(sock)
    {
       if (sock)
           service_search();
    }

    int service_search(const uint8_t* cont = 0, int contlen = 0)
    {
        // 4.7.1 SDP_ServiceSearchAttributeRequest PDU
        const uint8_t _sdp_hid_inquiry[] = {
            0x06,                               // SDP_ServiceSearchAttributeRequest
            0x00,0x01,
            0x00,0x0F,
            0x35,0x03,0x19,0x11,0x24,           // search for 1124 hid
            0xFF,0xFF,                          // max return
            0x35,0x05,0x0A,0x00,0x00,0xFF,0xFF, // AttributeIDList, range from 0000-FFFF
        };
        uint8_t buf[sizeof(_sdp_hid_inquiry) + 1 + 16];
        memcpy(buf,_sdp_hid_inquiry,sizeof(_sdp_hid_inquiry));
        buf[sizeof(_sdp_hid_inquiry)] = contlen;
        buf[4] += contlen;
        if (contlen)
            memcpy(buf+sizeof(_sdp_hid_inquiry)+1,cont,contlen);
        return l2_send(_sdp_sock,buf,sizeof(_sdp_hid_inquiry)+1+contlen);
    }

    int update()
    {
        uint8_t buf[512];
        int len = l2_recv(_sdp_sock,buf,sizeof(buf));
        if (len > 0)
            if (add(buf,len) == 0)
                return 0;   // complete record
        return 1;
    }

    int u8()
    {
        return *_data++;
    }
    int u16()
    {
        return (u8() << 8) | u8();
    }
    int u32()
    {
        return (u16() << 16) | u16();
    }

    #define SDP_DE_NULL   0
    #define SDP_DE_UINT   1
    #define SDP_DE_INT    2
    #define SDP_DE_UUID   3
    #define SDP_DE_STRING 4
    #define SDP_DE_BOOL   5
    #define SDP_DE_SEQ    6
    #define SDP_DE_ALT    7
    #define SDP_DE_URL    8

    int header(uint32_t& n)
    {
        uint8_t hdr = u8();
        uint8_t type = hdr >> 3;
        uint8_t i = hdr & 0x07;
        n = 0;
        switch (i) {
            case 5: n = u8(); break;
            case 6: n = u16(); break;
            case 7: n = u32(); break;
            default:
                n = 1 << i;
        }
        if (type == 0)
            n = 0;  // NULL
        return type;
    }

    void PAD(int n)
    {
        while (n--)
            printf("    ");
    }

    int get_id16()
    {
        uint32_t n;
        int t = header(n);
        if (t != SDP_DE_UINT || n != 2) {
            printf("bad sdp id\n");
            return -1;
        }
        return u16();
    }

    void on_uuid(int n)
    {

    }

    void on_string(const string& s)
    {
        switch (_current_id) {
            case SDP_ATTR_ID_SERVICE_NAME:          _SrvName = s; break;
            case SDP_ATTR_ID_SERVICE_DESCRIPTION:   _SrvDesc = s; break;
            case SDP_ATTR_ID_PROVIDER_NAME:         _ProviderName = s; break;
            case SDP_ATTR_HID_DESCRIPTOR_LIST:      _HIDDescriptor = s; break;
        }
    }

    int item(int len, int level = 0, bool want_id = true)
    {
        PAD(level);
        printf("{\n");
        const uint8_t* end = _data + len;
        uint32_t n,id;
        while (_data < end) {
            if (want_id) {
                _current_id = get_id16();
                if (_current_id == -1)
                    return -1;
                printf("%s %d\n",get_name(_current_id),_current_id);
            }
            int t = header(n);
            switch (t) {
                case SDP_DE_NULL:
                    printf("SDP_DE_NULL\n");
                    break;
                case SDP_DE_SEQ:            // it is a sequence
                    item(n,level+1,false);  // just a list
                    break;
                case SDP_DE_INT:
                case SDP_DE_UINT:
                    switch (n) {
                        case 1:
                            id = u8();
                            PAD(level);
                            printf("SDP_DE_UINT8 %d\n",id);
                            break;
                        case 2:
                            id = u16();
                            PAD(level);
                            printf("SDP_DE_UINT16 %d\n",id);
                            break;
                        case 4:
                            id = u32();
                            PAD(level);
                            printf("SDP_DE_UINT32 %d\n",id);
                            break;
                        default:
                            PAD(level);
                            printf("type:%d skipping %d\n",t,n);
                            _data += n;
                    }
                    break;
                case SDP_DE_STRING:
                {
                    string s((const char*)_data,n);
                    _data += n;
                    on_string(s);
                    PAD(level);
                    printf("SDP_DE_STRING: %s\n",s.c_str());
                }
                    break;
                case SDP_DE_UUID:
                    on_uuid(n);
                    id = -1;
                    switch (n) {
                        case 2: id = u16(); break;
                        case 4: id = u32(); break;
                        default: _data += n;
                    }
                    PAD(level);
                    printf("SDP_DE_UUID 0x%08X\n",id);
                    break;
                case SDP_DE_BOOL:
                    PAD(level);
                    printf("SDP_DE_BOOL ");
                    printf("%s\n",*_data++ ? "true" : "false");
                    break;
                case SDP_DE_URL:
                    printf("SDP_DE_URL %d bytes\n",n);
                    _data += n;
                    break;
                default:
                    PAD(level);
                    printf("unhandled type:%d skipping %d\n",t,n);
                    _data += n;
            }
        }
        PAD(level);
        printf("}\n");
        return 0;
    }

    int list(int len, int level = 0)
    {
        const uint8_t* end = _data + len;
        while (_data < end) {
            uint32_t n;
            int t = header(n);  // list
            if (t != SDP_DE_SEQ)
                break;
            if (item(n,level) == -1)
                return -1;
        }
        return 0;
    }

    void dump()
    {
        string s = _SrvName + " " + _SrvDesc + " " + _ProviderName;
        printf("%s\n",s.c_str());
        const uint8_t* d = (const uint8_t*)_HIDDescriptor.c_str();
        int n = (int)_HIDDescriptor.length();
        for (int i = 0; i < n; i++)
            printf("%02X,",d[i]);
    }

    void parse(const uint8_t* data, int len)
    {
        _data = data;
        const uint8_t* end = data + len;
        while (_data < end) {
            uint32_t n;
            int t = header(n);  // list
            if (t != SDP_DE_SEQ)
                break;
            if (list(n) == -1)
                break;
        }
    }

    // return hid descriptor if found
    string parse()
    {
        parse(&_buf[0],(int)_buf.size());
        return _HIDDescriptor;
    }

    // packet from sdp socket, accumulate full report before parsing
    int add(const uint8_t* data, int len)
    {
        _data = data;
        uint8_t pid = u8();
        uint16_t tid = u16();
        uint16_t total = u16();
        switch (pid) {
            case 7:     // search attribute resonse
            {
                int count = u16();
                const uint8_t* cont = _data + count;
                size_t pos = _buf.size();
                _buf.resize(pos + count);
                memcpy(&_buf[pos],_data,count);
                if (!cont[0])
                    return 0;   // go all we are going to get: we now have data in _buf
                service_search(cont+1,cont[0]);
                break;
            }
        }
        return 1;   // more to come?
    }
};

//==================================================================
//==================================================================
//  wiimote is a lovely thing

enum {
    WII_DEVICE_CLASS = 0x042500,       // Nintendo RVL-CNT-01
    WII_TR_DEVICE_CLASS = 0x080500,    // Nintendo RVL-CNT-01-TR, has motion plus
    SWITCH_PRO_DEVICE_CLASS = 0x082500,    // Nintendo Switch Pro Controller
    SWITCH_JOYCON_L_DEVICE_CLASS = 0x082500,   // Nintendo Switch Joy-Con (L)
    SWITCH_JOYCON_R_DEVICE_CLASS = 0x082500,   // Nintendo Switch Joy-Con (R)
};
wii_state wii_states[4] = {0};
switch_state switch_states[4] = {0};

// https://wiibrew.org/wiki/Wiimote
// https://github.com/dvdhrm/xwiimote/blob/master/doc/PROTOCOL
// https://web.archive.org/web/20080630032911/http://wiki.wiimoteproject.com/Reports

// 0x13 LED Enable // P4 P3 P2 P1 ? ? 1 RUMBLE  // turn on specific leds
// 0x13 IR Enable 1 - ? ? ? ? ? IRPCK 1 RUMBLE  // enable pixel clock for ir camera
// 0x1A IR Enable 2  - ? ? ? ? ? IREN 1 RUMBLE  // enable ir camera

const char* _butnames[] = {
    "_","_","_","PLUS","UP","DOWN","RIGHT","LEFT",
    "HOME","_","_","MINUS","A","B","ONE","TWO"
};

class WII {
public:
    void send(int s, const uint8_t* data, int len)
    {
        l2_send(s,data,len);
    }

    void led(int s, uint8_t id = 0x10)     // manipulate wii leds
    {
        uint8_t buf[3] = {0xA2, 0x11, id};  // set led
        send(s,buf,3);
        buf[1] = 0x15;  // status
        send(s,buf,3);
    }

    void read_mem(int s, int addr, int size)     // manipulate wii leds
    {
        uint8_t cmd[8] = {
            0xA2, 0x17,
            0x04,   // registers, not eeprom
            (uint8_t)(addr >> 16),(uint8_t)(addr >> 8),(uint8_t)addr,
            (uint8_t)(size >> 8),(uint8_t)size
        };  // read mem
        send(s,cmd,sizeof(cmd));
    }

    void write_mem(int s, int addr, const uint8_t* data, int size)     // manipulate wii leds
    {
        uint8_t cmd[7+16] = {
            0xA2, 0x16,
            0x04,   // registers, not eeprom
            (uint8_t)(addr >> 16),(uint8_t)(addr >> 8),(uint8_t)addr,(uint8_t)size,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        };  // write mem
        memcpy(cmd+7,data,size);
        send(s,cmd,sizeof(cmd));
    }

    void init_extention(int s)
    {
        uint8_t e = 0x55;
        write_mem(s,0xA400F0,&e,1);   // init
    }

    void read_extention_type(int s)
    {
        read_mem(s,0xA400FA,6);
    }

    // https://wiibrew.org/wiki/Wiimote#0x31:_Core_Buttons_and_Accelerometer
    // ?    X1    X0    PLUS    UP    DOWN    RIGHT    LEFT
    // HOME Z1    Y1    MINUS   A     B       ONE       TWO
    void dump_report(const uint8_t* data, int len)
    {
        const uint8_t* h = data + 2;
        int pad = ((h[0] & 0x9F) << 8) | (h[1] & 0x9F);
        int x = (h[2] << 2) | ((h[0] >> 5) & 0x3);  // 10 bit accelerometer values
        int y = (h[3] << 2) | ((h[1] >> 4) & 0x2);
        int z = (h[4] << 2) | ((h[1] >> 5) & 0x2);
        string n;
        for (int m = 15; m >= 0; m--)
            if ((1<<m) & pad) n += _butnames[15-m] + string(" ");
        printf("WII x:%d y:%d z:%d %s\n",x-512,y-512,z-512,n.c_str());   // Can read real calibration data from 0x16
    }

    // 0x32 BTN + 6 EXT             - button + extension
    // 0x37 BTN + XLR + IR + 6 EXT  - turn on accelerometer, ir and external (nunchuck) device
    void select_input_report(int s, uint8_t report = 0x32)
    {
        u8 hid[4] = { 0xA2, 0x12, 0x00, report };
        send(s,hid,4);
    }

// "Nintendo RVL-CNT-01"
// "Nintendo RVL-CNT-01-TR"
    bool is_wii(InputDevice* d)
    {
        if (strncmp(d->_name.c_str(),"Nintendo",8) == 0)
            return true;
        return (d->_dev_class == WII_DEVICE_CLASS) || (d->_dev_class == WII_TR_DEVICE_CLASS);
    }

    // assign this device to a wii slot
    int find_slot(InputDevice* d)
    {
        if (d->_wii_index == -1)
        {
            for (int i = 0; i < 4; i++) {
                if (wii_states[i].flags == 0) {
                    wii_states[i].flags = wiimote;
                    d->_wii_index = i;
                    led(d->_interrupt,0x10 << i);   // select our id an turn it on
                    break;
                }
            }
        }
        return d->_wii_index;
    }

    void hid(InputDevice* d, const uint8_t* data, int len)
    {
        if (!is_wii(d))
            return;
        int slot = find_slot(d);
        if (slot == -1)
            return;     // does not have a slot yet? odd
        wii_state* state = wii_states + slot;

        auto* h = data+2;
        switch (data[1]) {
            // This report is sent in response to report 0x15 (status, from led setting)
            case 0x20:  // Status
            {
                int stat = h[2];
                int bat = h[5];
                printf("wiimote stat: %02X battery:%d\n",stat,bat);
                //state->battery = bat;
                if (stat & 2)
                    init_extention(d->_interrupt);    // extension plugged?
                else {
                    select_input_report(d->_interrupt);
                    state->flags &= ~0xE;             // unplugged
                }
            }
                break;

            // wii response to memory read
            case 0x21: // Read Data
            {
                int size = (h[2] >> 4) + 1;
                int addr = (h[3] << 8) | h[4];  // low bytes of address
                h += 5;

                int id = (h[4] << 8) | h[5];
                //printf("wiimote %04X:%02X read id is %04X\n",size,addr,id);
                int flags = 0;
                switch (id) {
                    case 0x0000: flags = nunchuk;            gui_msg("nunchuk"); break;              // NUNCHUK
                    case 0x0101: flags = classic_controller; gui_msg("classic controller"); break;   // CLASSIC_CONTROLLER
                    case 0x0103: flags = drums_guitar;       gui_msg("drums/guitar"); break;         // DRUNMS/GUITAR
                        break;
                }
                state->flags = (state->flags & ~0xE) | flags;
                select_input_report(d->_interrupt);
            }
                break;

            case 0x22: // Command Status
                if (!(state->flags & extension_read)) {
                    read_extention_type(d->_interrupt);
                    state->flags |= extension_read;
                } else if (!(state->flags & encryption_disabled)) {
                    uint8_t e = 0;
                    write_mem(d->_interrupt,0xA400FB,&e,1);   // disable encryption
                    state->flags |= encryption_disabled;
                }
                break;

            // default wii report
            case 0x30:
                select_input_report(d->_interrupt);
                break;

            case 0x32:  // core + ext
            case 0x37:  // big report
                memcpy(state->report,data,min((int)sizeof(state->report),len));
                //dump_report(data,len);
                break;

            default:
                printf("unhandled hid %02X\n",data[1]);
        }
    }
};

//==================================================================
//==================================================================
//  Nintendo Switch Controller Support
//  Supports Pro Controller, Joy-Con (L), Joy-Con (R)

// References:
// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
// https://github.com/Dan-Shields/node-switchbot
// https://github.com/ricardoquesada/bluepad32

class SWITCH {
public:
    void send(int s, const uint8_t* data, int len)
    {
        l2_send(s, data, len);
    }

    // Send subcommand with proper packet structure
    void send_subcmd(int s, switch_state* state, uint8_t subcmd, const uint8_t* data, int data_len)
    {
        uint8_t buf[64];
        memset(buf, 0, sizeof(buf));

        buf[0] = 0xA1;  // HID report type (matches receive side)
        buf[1] = 0x01;  // Output report ID: OUTPUT_RUMBLE_AND_SUBCMD
        buf[2] = state->packet_num++;  // packet_num

        // Rumble data (neutral values)
        buf[3] = 0x00;
        buf[4] = 0x01;
        buf[5] = 0x40;
        buf[6] = 0x40;
        buf[7] = 0x00;
        buf[8] = 0x01;
        buf[9] = 0x40;
        buf[10] = 0x40;

        buf[11] = subcmd;  // Subcommand ID

        if (state->packet_num > 0x0F)
            state->packet_num = 0;

        // Copy subcommand data if provided
        if (data && data_len > 0) {
            memcpy(buf + 12, data, min(data_len, 52));
        }

        send(s, buf, 12 + data_len);
    }

    // Request device info (subcommand 0x02)
    void fsm_request_device_info(int s, switch_state* state)
    {
        state->init_state = SWITCH_STATE_REQ_DEV_INFO;
        send_subcmd(s, state, SWITCH_SUBCMD_REQ_DEV_INFO, nullptr, 0);
    }

    // Read factory stick calibration (subcommand 0x10)
    void fsm_read_factory_stick_calibration(int s, switch_state* state)
    {
        state->init_state = SWITCH_STATE_READ_FACTORY_STICK_CAL;

        // Determine address based on controller type
        uint32_t spi_addr = (state->controller_type == SWITCH_CONTROLLER_TYPE_JCR)
                            ? SWITCH_FACTORY_STICK_CAL_ADDR_RIGHT
                            : SWITCH_FACTORY_STICK_CAL_ADDR_LEFT;
        uint8_t bytes_to_read = SWITCH_FACTORY_STICK_CAL_SIZE;

        // For Pro Controller, read both left and right
        if (state->controller_type == SWITCH_CONTROLLER_TYPE_PRO)
            bytes_to_read *= 2;

        uint8_t data[5] = {
            (uint8_t)(spi_addr & 0xFF),
            (uint8_t)((spi_addr >> 8) & 0xFF),
            (uint8_t)((spi_addr >> 16) & 0xFF),
            (uint8_t)((spi_addr >> 24) & 0xFF),
            bytes_to_read
        };

        send_subcmd(s, state, SWITCH_SUBCMD_SPI_FLASH_READ, data, 5);
    }

    // Read user stick calibration (subcommand 0x10)
    void fsm_read_user_stick_calibration(int s, switch_state* state)
    {
        state->init_state = SWITCH_STATE_READ_USER_STICK_CAL;

        uint32_t spi_addr = (state->controller_type == SWITCH_CONTROLLER_TYPE_JCR)
                            ? SWITCH_USER_STICK_CAL_ADDR_RIGHT
                            : SWITCH_USER_STICK_CAL_ADDR_LEFT;
        uint8_t bytes_to_read = SWITCH_USER_STICK_CAL_SIZE;

        if (state->controller_type == SWITCH_CONTROLLER_TYPE_PRO)
            bytes_to_read *= 2;

        uint8_t data[5] = {
            (uint8_t)(spi_addr & 0xFF),
            (uint8_t)((spi_addr >> 8) & 0xFF),
            (uint8_t)((spi_addr >> 16) & 0xFF),
            (uint8_t)((spi_addr >> 24) & 0xFF),
            bytes_to_read
        };

        send_subcmd(s, state, SWITCH_SUBCMD_SPI_FLASH_READ, data, 5);
    }

    // Read factory IMU calibration (subcommand 0x10)
    void fsm_read_factory_imu_calibration(int s, switch_state* state)
    {
        state->init_state = SWITCH_STATE_READ_FACTORY_IMU_CAL;

        uint32_t spi_addr = SWITCH_FACTORY_IMU_CAL_ADDR;
        uint8_t data[5] = {
            (uint8_t)(spi_addr & 0xFF),
            (uint8_t)((spi_addr >> 8) & 0xFF),
            (uint8_t)((spi_addr >> 16) & 0xFF),
            (uint8_t)((spi_addr >> 24) & 0xFF),
            SWITCH_FACTORY_IMU_CAL_SIZE
        };

        send_subcmd(s, state, SWITCH_SUBCMD_SPI_FLASH_READ, data, 5);
    }

    // Set full report mode 0x30 (subcommand 0x03)
    void fsm_set_full_report(int s, switch_state* state)
    {
        state->init_state = SWITCH_STATE_SET_FULL_REPORT;

        uint8_t data[1] = {0x30};  // Standard full mode
        send_subcmd(s, state, SWITCH_SUBCMD_SET_REPORT_MODE, data, 1);
    }

    // Enable/disable IMU (subcommand 0x40)
    void fsm_enable_imu(int s, switch_state* state)
    {
        state->init_state = SWITCH_STATE_ENABLE_IMU;

        uint8_t data[1] = {0x00};  // Disable IMU for now (can be 0x01 to enable)
        send_subcmd(s, state, SWITCH_SUBCMD_ENABLE_IMU, data, 1);
    }

    // Update player LED (subcommand 0x30)
    void fsm_update_led(int s, switch_state* state, uint8_t led_mask)
    {
        state->init_state = SWITCH_STATE_UPDATE_LED;

        uint8_t data[1] = {led_mask};
        send_subcmd(s, state, SWITCH_SUBCMD_SET_PLAYER_LEDS, data, 1);
    }

    // Mark controller as ready
    void fsm_ready(switch_state* state)
    {
        state->init_state = SWITCH_STATE_READY;
    }

    // Process FSM - advance to next state
    void process_fsm(InputDevice* d)
    {
        int slot = d->_wii_index;
        if (slot == -1)
            return;

        switch_state* state = switch_states + slot;

        switch (state->init_state) {
            case SWITCH_STATE_SETUP:
                fsm_request_device_info(d->_interrupt, state);
                break;
            case SWITCH_STATE_REQ_DEV_INFO:
                fsm_read_factory_stick_calibration(d->_interrupt, state);
                break;
            case SWITCH_STATE_READ_FACTORY_STICK_CAL:
                fsm_read_user_stick_calibration(d->_interrupt, state);
                break;
            case SWITCH_STATE_READ_USER_STICK_CAL:
                fsm_read_factory_imu_calibration(d->_interrupt, state);
                break;
            case SWITCH_STATE_READ_FACTORY_IMU_CAL:
                fsm_set_full_report(d->_interrupt, state);
                break;
            case SWITCH_STATE_SET_FULL_REPORT:
                fsm_enable_imu(d->_interrupt, state);
                break;
            case SWITCH_STATE_ENABLE_IMU:
                fsm_update_led(d->_interrupt, state, 0x01 << slot);
                break;
            case SWITCH_STATE_UPDATE_LED:
                fsm_ready(state);
                break;
            case SWITCH_STATE_READY:
                // Nothing to do
                break;
            default:
                printf("Switch FSM: Unexpected state %d\n", state->init_state);
                break;
        }
    }

    bool is_switch(InputDevice* d)
    {
        if (strncmp(d->_name.c_str(), "Pro Controller", 14) == 0)
            return true;
        if (strncmp(d->_name.c_str(), "Joy-Con (L)", 11) == 0)
            return true;
        if (strncmp(d->_name.c_str(), "Joy-Con (R)", 11) == 0)
            return true;
        return d->_dev_class == SWITCH_PRO_DEVICE_CLASS;
    }

    // Determine controller type from name
    uint8_t get_controller_type(InputDevice* d)
    {
        if (strncmp(d->_name.c_str(), "Pro Controller", 14) == 0)
            return SWITCH_CONTROLLER_TYPE_PRO;
        if (strncmp(d->_name.c_str(), "Joy-Con (L)", 11) == 0)
            return SWITCH_CONTROLLER_TYPE_JCL;
        if (strncmp(d->_name.c_str(), "Joy-Con (R)", 11) == 0)
            return SWITCH_CONTROLLER_TYPE_JCR;
        return SWITCH_CONTROLLER_TYPE_PRO; // Default to Pro Controller
    }

    // Find or assign a slot for this device
    int find_slot(InputDevice* d)
    {
        if (d->_wii_index == -1)
        {
            for (int i = 0; i < 4; i++) {
                if (switch_states[i].flags == 0) {
                    uint8_t ctrl_type = get_controller_type(d);
                    switch_states[i].flags = switch_controller;
                    if (ctrl_type == SWITCH_CONTROLLER_TYPE_PRO)
                        switch_states[i].flags |= switch_pro_controller;
                    else if (ctrl_type == SWITCH_CONTROLLER_TYPE_JCL)
                        switch_states[i].flags |= switch_joycon_left;
                    else if (ctrl_type == SWITCH_CONTROLLER_TYPE_JCR)
                        switch_states[i].flags |= switch_joycon_right;

                    switch_states[i].init_state = SWITCH_STATE_SETUP;
                    switch_states[i].controller_type = ctrl_type;
                    switch_states[i].packet_num = 0;
                    switch_states[i].last_buttons = 0;
                    switch_states[i].last_lstick_x = 128;
                    switch_states[i].last_lstick_y = 128;

                    // Initialize default calibration values
                    switch_states[i].cal_x.min = switch_states[i].cal_y.min = 512;
                    switch_states[i].cal_x.center = switch_states[i].cal_y.center = 2048;
                    switch_states[i].cal_x.max = switch_states[i].cal_y.max = 3583;
                    switch_states[i].cal_rx.min = switch_states[i].cal_ry.min = 512;
                    switch_states[i].cal_rx.center = switch_states[i].cal_ry.center = 2048;
                    switch_states[i].cal_rx.max = switch_states[i].cal_ry.max = 3583;

                    d->_wii_index = i;
                    process_fsm(d);
                    break;
                }
            }
        }
        return d->_wii_index;
    }

    // Parse stick calibration data from SPI flash
    void parse_stick_calibration(switch_cal_stick* x, switch_cal_stick* y, const uint8_t* data, bool is_left)
    {
        int32_t cal_x_max, cal_y_max, cal_x_min, cal_y_min;

        if (is_left) {
            // Left stick: max, center, min
            cal_x_max = data[0] | ((data[1] & 0x0F) << 8);
            cal_y_max = (data[1] >> 4) | (data[2] << 4);
            x->center = data[3] | ((data[4] & 0x0F) << 8);
            y->center = (data[4] >> 4) | (data[5] << 4);
            cal_x_min = data[6] | ((data[7] & 0x0F) << 8);
            cal_y_min = (data[7] >> 4) | (data[8] << 4);
        } else {
            // Right stick: center, min, max
            x->center = data[0] | ((data[1] & 0x0F) << 8);
            y->center = (data[1] >> 4) | (data[2] << 4);
            cal_x_min = data[3] | ((data[4] & 0x0F) << 8);
            cal_y_min = (data[4] >> 4) | (data[5] << 4);
            cal_x_max = data[6] | ((data[7] & 0x0F) << 8);
            cal_y_max = (data[7] >> 4) | (data[8] << 4);
        }

        x->min = x->center - cal_x_min;
        x->max = x->center + cal_x_max;
        y->min = y->center - cal_y_min;
        y->max = y->center + cal_y_max;
    }

    // Calibrate analog stick value using calibration data
    int32_t calibrate_axis(int32_t v, switch_cal_stick cal)
    {
        int32_t ret;
        if (v > cal.center) {
            ret = (v - cal.center) * 128;
            ret /= (cal.max - cal.center);
        } else {
            ret = (cal.center - v) * -128;
            ret /= (cal.center - cal.min);
        }
        // Clamp to 0-255 range
        ret += 128;
        if (ret < 0) ret = 0;
        if (ret > 255) ret = 255;
        return ret;
    }

    // Parse button data from standard input report
    void parse_buttons(switch_state* state, const uint8_t* data)
    {
        // Button data is in bytes 3, 4, 5 for standard input report 0x30
        uint8_t btn_right = data[3];  // Right buttons (Y, X, B, A, SR, SL, R, ZR)
        uint8_t btn_shared = data[4]; // Shared buttons (-, +, R-stick, L-stick, Home, Capture)
        uint8_t btn_left = data[5];   // Left buttons (Down, Up, Right, Left, SR, SL, L, ZL)

        // Use lookup tables for faster button parsing
        static const struct { uint8_t mask; uint32_t flag; } right_map[] = {
            { 0x01, switch_y },   { 0x02, switch_x },
            { 0x04, switch_b },   { 0x08, switch_a },
            { 0x40, switch_r },   { 0x80, switch_zr },
        };
        static const struct { uint8_t mask; uint32_t flag; } shared_map[] = {
            { 0x01, switch_minus },   { 0x02, switch_plus },
            { 0x04, switch_rstick },  { 0x08, switch_lstick },
            { 0x10, switch_home },    { 0x20, switch_capture },
        };
        static const struct { uint8_t mask; uint32_t flag; } left_map[] = {
            { 0x01, switch_down },    { 0x02, switch_up },
            { 0x04, switch_right },   { 0x08, switch_left },
            { 0x40, switch_l },       { 0x80, switch_zl },
        };

        state->buttons = 0;
        for (const auto& m : right_map)  if (btn_right & m.mask) state->buttons |= m.flag;
        for (const auto& m : shared_map) if (btn_shared & m.mask) state->buttons |= m.flag;
        for (const auto& m : left_map)   if (btn_left & m.mask) state->buttons |= m.flag;
    }

    // Parse analog stick data from standard input report
    void parse_sticks(switch_state* state, const uint8_t* data)
    {
        // Left stick: bytes 6, 7, 8
        uint16_t lstick_x_raw = data[6] | ((data[7] & 0x0F) << 8);
        uint16_t lstick_y_raw = (data[7] >> 4) | (data[8] << 4);

        // Right stick: bytes 9, 10, 11
        uint16_t rstick_x_raw = data[9] | ((data[10] & 0x0F) << 8);
        uint16_t rstick_y_raw = (data[10] >> 4) | (data[11] << 4);

        // Apply calibration
        state->lstick_x = calibrate_axis(lstick_x_raw, state->cal_x);
        state->lstick_y = calibrate_axis(lstick_y_raw, state->cal_y);
        state->rstick_x = calibrate_axis(rstick_x_raw, state->cal_rx);
        state->rstick_y = calibrate_axis(rstick_y_raw, state->cal_ry);
    }

    // Parse data from Simple HID mode (0x3F)
    void parse_simple_hid(switch_state* state, const uint8_t* data, int len)
    {
        if (len < 13)
            return;

        // Format: A1 3F [2 bytes buttons] [1 byte hat] [8 bytes sticks]
        uint16_t buttons_raw = data[2] | (data[3] << 8);
        uint8_t hat = data[4];

        // Parse buttons using lookup table (faster than 14 individual if statements)
        static const struct { uint16_t mask; uint32_t flag; } button_map[] = {
            { 0x0001, switch_y },      { 0x0002, switch_b },
            { 0x0004, switch_a },      { 0x0008, switch_x },
            { 0x0010, switch_l },      { 0x0020, switch_r },
            { 0x0040, switch_zl },     { 0x0080, switch_zr },
            { 0x0100, switch_minus },  { 0x0200, switch_plus },
            { 0x0400, switch_lstick }, { 0x0800, switch_rstick },
            { 0x1000, switch_home },   { 0x2000, switch_capture },
        };

        state->buttons = 0;
        for (const auto& m : button_map) {
            if (buttons_raw & m.mask)
                state->buttons |= m.flag;
        }

        // Parse D-pad/hat (0x08 = centered, 0x00-0x07 = directions)
        if (hat != 0x08) {
            if (hat == 0x00 || hat == 0x01 || hat == 0x07) state->buttons |= switch_up;
            if (hat == 0x01 || hat == 0x02 || hat == 0x03) state->buttons |= switch_right;
            if (hat == 0x03 || hat == 0x04 || hat == 0x05) state->buttons |= switch_down;
            if (hat == 0x05 || hat == 0x06 || hat == 0x07) state->buttons |= switch_left;
        }

        // Parse analog sticks
        // Format appears to be: [low byte] [high byte] for each axis
        // Centered position is 0x8000 (32768), scaled to 0-255 range
        // For simplicity, just use the high byte which gives us 0-255 range centered at 128
        state->lstick_x = data[6];  // High byte of left stick X
        state->lstick_y = data[8];  // High byte of left stick Y
        state->rstick_x = data[10]; // High byte of right stick X
        state->rstick_y = data[12]; // High byte of right stick Y
    }

    // Handle subcommand reply (report 0x21)
    void handle_subcmd_reply(InputDevice* d, switch_state* state, const uint8_t* data, int len)
    {
        if (len < 16) {
            return;
        }

        // Report structure:
        // [0] = 0xA1 (input report)
        // [1] = 0x21 (subcmd reply)
        // [2] = timer
        // [3] = battery / connection info
        // [4-15] = button/stick status
        // [16] = ACK info
        // [17] = subcmd ID
        // [18+] = reply data

        uint8_t ack = data[16];
        uint8_t subcmd_id = data[17];

        if ((ack & 0x80) == 0) {
            return;
        }

        // Handle specific subcommand replies
        switch (subcmd_id) {
            case SWITCH_SUBCMD_REQ_DEV_INFO:
                if (len >= 21) {
                    state->controller_type = data[20];
                }
                break;

            case SWITCH_SUBCMD_SPI_FLASH_READ:
                if (len >= 23) {
                    uint32_t addr = data[18] | (data[19] << 8) | (data[20] << 16) | (data[21] << 24);
                    int data_len = data[22];
                    const uint8_t* spi_data = data + 23;

                    // Process based on current FSM state
                    switch (state->init_state) {
                        case SWITCH_STATE_READ_FACTORY_STICK_CAL:
                            if (state->controller_type == SWITCH_CONTROLLER_TYPE_PRO && data_len >= 18) {
                                parse_stick_calibration(&state->cal_x, &state->cal_y, spi_data, true);
                                parse_stick_calibration(&state->cal_rx, &state->cal_ry, spi_data + 9, false);
                            } else if (data_len >= 9) {
                                bool is_left = (state->controller_type == SWITCH_CONTROLLER_TYPE_JCL);
                                if (is_left)
                                    parse_stick_calibration(&state->cal_x, &state->cal_y, spi_data, true);
                                else
                                    parse_stick_calibration(&state->cal_rx, &state->cal_ry, spi_data, false);
                            }
                            break;
                        case SWITCH_STATE_READ_USER_STICK_CAL:
                            // User calibration is optional - just use factory values
                            break;
                        case SWITCH_STATE_READ_FACTORY_IMU_CAL:
                            // IMU calibration - can be skipped for basic functionality
                            break;
                    }
                }
                break;
        }

        // Advance FSM after processing reply
        process_fsm(d);
    }

    void hid(InputDevice* d, const uint8_t* data, int len)
    {
        if (!is_switch(d))
            return;

        int slot = find_slot(d);
        if (slot == -1)
            return;

        switch_state* state = switch_states + slot;

        // Check report type
        if (len < 2)
            return;

        switch (data[0]) {
            case 0xA1:  // Input report
                switch (data[1]) {
                    case 0x21:  // Subcommand reply
                        handle_subcmd_reply(d, state, data, len);
                        // Also parse button/stick data embedded in the reply
                        if (state->init_state == SWITCH_STATE_READY) {
                            parse_buttons(state, data);
                            parse_sticks(state, data);
                            state->battery = (data[2] >> 4) & 0x0F;
                        }
                        break;

                    case 0x30:  // Standard full mode (IMU data enabled)
                    case 0x31:  // NFC/IR MCU mode
                        if (state->init_state == SWITCH_STATE_READY) {
                            memcpy(state->report, data, min((int)sizeof(state->report), len));
                            parse_buttons(state, data);
                            parse_sticks(state, data);
                            state->battery = (data[2] >> 4) & 0x0F;
                        }
                        break;

                    case 0x3F:  // Simple HID mode (used when connected to non-Switch devices)
                        // Controller is in Simple HID mode - skip FSM and use it directly
                        if (state->init_state != SWITCH_STATE_READY) {
                            state->init_state = SWITCH_STATE_READY;
                        }
                        memcpy(state->report, data, min((int)sizeof(state->report), len));
                        parse_simple_hid(state, data, len);
                        break;

                    default:
                        // Silently ignore unknown report types to avoid console spam
                        break;
                }
                break;

            default:
                // Silently ignore unknown report types
                break;
        }
    }
};

InputDevice::InputDevice() : _sdp_handler(0),_reconnect(0),_wii_index(-1) {
    _name[0] = 0;
}

InputDevice::~InputDevice() {
    disconnection_complete();
    delete _sdp_handler;
}

void InputDevice::start_sdp()
{
    delete _sdp_handler;
    _sdp_handler = new SDPParse(_sdp);
}

void InputDevice::update_sdp()
{
    if (_sdp_handler && (_sdp_handler->update() == 0)) {
        _hid_descriptor = _sdp_handler->parse();
        delete _sdp_handler;
        _sdp_handler = 0;
        l2_close(_sdp);
        _sdp = 0;

        // got the sdp from the new device
        // do we want to authenticate?
        hci_authentication_requested(&_bdaddr);
        _state = AUTHENTICATING;
    }
}

// 1. First connection
//      [connect]
//      connection_complete
//      read remote name
//      if NEW
//          read sdp
//      authenticate
//      if NEW
//          get pin
//          store link key
//      open sockets

void InputDevice::connect()
{
    _reconnect = 0;
    _state = CONNECT;
    hci_connect(&_bdaddr);
}

void InputDevice::connection_request()
{
    _reconnect = 1;
    hci_accept_connection_request(&_bdaddr);
}

void InputDevice::connection_complete(int status)
{
    if (_reconnect) {   // start listening to reconnections
        _control = l2_open(&_bdaddr, HID_CONTROL_PSM, true);
        _interrupt = l2_open(&_bdaddr, HID_INTERRUPT_PSM, true);
    }
    hci_remote_name_request(&_bdaddr);
    _state = READING_NAME;
}

void InputDevice::remote_name_response(const char* n)
{
    _name = n;
    printf("%s connected\n", name());
    gui_msg(name());

    // Check if this is a Switch controller (Pro, Joy-Con L, Joy-Con R)
    bool is_switch_controller = (strncmp(_name.c_str(), "Pro Controller", 14) == 0) ||
                                 (strncmp(_name.c_str(), "Joy-Con (L)", 11) == 0) ||
                                 (strncmp(_name.c_str(), "Joy-Con (R)", 11) == 0) ||
                                 (_dev_class == SWITCH_PRO_DEVICE_CLASS);

    // Switch controllers: ALWAYS skip authentication and SDP, even on reconnect
    if (is_switch_controller) {
        _control = l2_open(&_bdaddr, HID_CONTROL_PSM);
        _interrupt = l2_open(&_bdaddr, HID_INTERRUPT_PSM);
        _state = OPENING;
        return;
    }

    // Non-Switch controllers: normal flow
    uint8_t key[16];
    if (_reconnect || (read_link_key(&_bdaddr,key) == 0))       // do we know this device?
    {
        if (_reconnect)
            _state = OPENING;                   // we already have a link key
        else {
            hci_authentication_requested(&_bdaddr);
            _state = AUTHENTICATING;
        }
    } else {
        _sdp = l2_open(&_bdaddr,HID_SDP_PSM);
        _state = READING_SDP;
    }
}

void InputDevice::authentication_complete(int status)
{
    if (status)
        printf("Auth failed: %d\n",status);

    if (!_reconnect) {
        _control = l2_open(&_bdaddr, HID_CONTROL_PSM);
        _interrupt = l2_open(&_bdaddr, HID_INTERRUPT_PSM);
    }
}

void InputDevice::disconnection_complete()
{
    l2_close(_sdp);     // these are already dead
    l2_close(_control);
    l2_close(_interrupt);

    _reconnect = 0;
    _interrupt = _control = _interrupt = 0;
    _state = CLOSED;
    if (_wii_index != -1)
        wii_states[_wii_index].flags = 0;
    _wii_index = -1;
}

const char* _nams[] = {
    "L2CAP_NONE",
    "L2CAP_LISTENING",
    "L2CAP_OPENING",
    "L2CAP_OPEN",
    "L2CAP_CLOSED",
    "L2CAP_DELETED",
};

void InputDevice::socket_changed(int socket, int state)
{
    if ((socket == _sdp) && (state == L2CAP_OPEN))
        start_sdp();
    else if ((socket == _interrupt) && (state == L2CAP_OPEN || state == L2CAP_LISTENING)) {
        // Switch controllers will auto-initialize when data is received
    }
}

const char* InputDevice::name()
{
    return _name.empty() ? batostr(_bdaddr) : _name.c_str();
}

class HIDSource {
    string _local_name;
    vector<InputDevice*> _devices;
    WII _wii;
    SWITCH _switch;

    InputDevice* get_device(const bdaddr_t* bdaddr)
    {
        for (auto d : _devices)
            if (memcmp(&d->_bdaddr,bdaddr,6) == 0)
                return d;
        return NULL;
    }

    static int hci_cb(HCI_CALLBACK_EVENT evt, const void* data, int len, void *ref)
    {
        return ((HIDSource*)ref)->cb(evt,data,len);
    }

    bool is_input_device(const uint8_t* dev_class)
    {
        int dc = (dev_class[0] << 16) | (dev_class[1] << 8) | dev_class[2];
        int major = (dc >> 8) & 0x1f;
        int minor = (dc >> 16) >> 2;

        if (major == 5) {
            switch (minor & 15) {
                case 1: return true;  // joystick
                case 2: return true;  // gamepad
                case 3: return true;  // remote
            }
            switch (minor & 0x30) {
                case 0x10: return true; // Keyboard
                case 0x20: return true; // Pointing device
                case 0x30: return true; // Pointing device/keyboard
            }
        }
        if (dc == 0)
            return true;    // TODO. some wiimotes report 0 on reconnect
        return false;
    }

    // add a device if we have not seen it before
    InputDevice* add_device(const bdaddr_t* addr, const uint8_t* dev_class)
    {
        if (dev_class && !is_input_device(dev_class))
            return 0;

        InputDevice* d = get_device(addr);
        if (!d) {
            d = new InputDevice();
            d->_bdaddr = *addr;
            d->_dev_class = (dev_class[0] << 16) | (dev_class[1] << 8) | dev_class[2];
            d->_control = d->_interrupt = d->_sdp = 0;
            d->_state = InputDevice::CLOSED;
            _devices.push_back(d);
            printf("%s:%06X input device added\n",batostr(d->_bdaddr),d->_dev_class);
        }
        return d;
    }

    // inbound (re)connection, is 1 if we want connection
    int connection_request(const connection_request_info& ci)
    {
        auto* d = add_device(&ci.bdaddr,ci.dev_class);
        if (d)
            d->connection_request();
        return 0;
    }

    int cb(HCI_CALLBACK_EVENT evt, const void* data, int len)
    {
        InputDevice* d;
        const auto& cb = *((const cbdata*)data);
        switch (evt) {
            case CALLBACK_READY:
                printf("HCI Ready\n");
                hci_start_inquiry(5);       // inquire for 5 seconds
                break;

            case CALLBACK_INQUIRY_RESULT:
                add_device(&cb.ii.bdaddr,cb.ii.dev_class);
                break;

            case CALLBACK_INQUIRY_DONE:
                printf("Inquiry done, found %d devices\n", (int)_devices.size());
                for (auto* id : _devices)
                    id->connect();
                break;

            case CALLBACK_REMOTE_NAME:
                d = get_device(&cb.ri.bdaddr);
                if (d)
                    d->remote_name_response(cb.ri.name);
                break;

            case CALLBACK_CONNECTION_REQUEST:
                d = add_device(&cb.cri.bdaddr,cb.cri.dev_class);
                if (d)
                    d->connection_request();
                break;

            case CALLBACK_DISCONNECTION_COMP:
                d = get_device(&cb.bdaddr);
                if (d)
                    d->disconnection_complete();
                break;

            case CALLBACK_CONNECTION_COMPLETE:
                d = get_device(&cb.ci.bdaddr);
                if (d)
                    d->connection_complete(cb.ci.status);
                break;

            case CALLBACK_AUTHENTICATION_COMPLETE:
                d = get_device(&cb.aci.bdaddr);
                if (d)
                    d->authentication_complete(cb.aci.status);
                break;

            case CALLBACK_L2CAP_SOCKET_STATE_CHANGED:
                d = get_device(&cb.ss.bdaddr);
                if (d)
                    d->socket_changed(cb.ss.socket,cb.ss.state);
                break;

            default:
                break;
        }
        return 0;
    }

    public:
    HIDSource(const char* localname) : _local_name(localname)
    {
        hci_init(hci_cb,this);
    }

    void hid_control(InputDevice* d, const uint8_t* data, int len)
    {
        printf("HID CONTROL %02X %d\n",data[0],len);
    }

    // get hid (events). max one per call
    // called from emu thread
    int get(uint8_t* dst, int dst_len)
    {
        for (auto d : _devices) {
            if (d->_interrupt) {
                int tmp_len = 0;
                // Try up to 2 times to get fresher data without blocking
                for (int attempts = 0; attempts < 2; attempts++) {
                    int len = l2_recv(d->_interrupt, dst, dst_len);
                    if (len > 0) {
                        tmp_len = len;
                    } else {
                        break;  // No more data, exit early
                    }
                }
                if (tmp_len > 0) {
                    _wii.hid(d,dst,tmp_len);
                    _switch.hid(d,dst,tmp_len);
                    return tmp_len;
                }
                if (tmp_len < 0) {
                    printf("disconnect\n");
                    d->disconnection_complete();
                }
            }
            if (d->_sdp)
                d->update_sdp();
        }
        return 0;
    }
};


HIDSource* _hid_source = 0;
int hid_init(const char* local_name)
{
    hid_close();
    _hid_source = new HIDSource(local_name);
    return 0;
}

int hid_close()
{
    delete _hid_source;
    _hid_source = 0;
    return 0;
}

int hid_update()
{
    return hci_update();
}

int hid_get(uint8_t* dst, int dst_len)
{
    if (!_hid_source)
        return -1;
    return _hid_source->get(dst,dst_len);
}

// map wii controller keys
uint32_t wii_map(int index, const uint32_t* common, const uint32_t* classic)
{
    uint32_t f = wii_states[index].flags;
    if (!(f & wiimote))
        return 0;

    uint32_t r = 0;
    int pad = wii_states[index].common();
    for (int i = 0; i < 16; i++) {
        if (pad & (0x8000 >> i))
            r |= common[i];
    }
    if (classic && (f & classic_controller)) {
        pad = wii_states[index].classic();
        for (int i = 0; i < 16; i++) {
            if (pad & (0x8000 >> i))
                r |= classic[i];
        }
    }
    return r;
}

// map switch controller buttons
uint32_t switch_map(int index, const uint32_t* buttons)
{
    uint32_t f = switch_states[index].flags;
    if (!(f & switch_controller)) {
        return 0;
    }

    uint32_t r = 0;
    uint32_t pad = switch_states[index].get_buttons();

    // Map button flags to emulator-specific values
    // buttons array should contain 20 entries corresponding to switch button flags
    for (int i = 0; i < 20; i++) {
        if (pad & (1 << i)) {
            r |= buttons[i];
        }
    }

    return r;
}
