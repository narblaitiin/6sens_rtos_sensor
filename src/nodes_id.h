#ifndef NODES_ID_H
#define NODES_ID_H

#ifndef NODE_ID
#error You must define a node number NODE_ID : `export NODE_ID=X`
#endif

#if NODE_ID==1
// node-1-otaa
#define LORAWAN_DEV_EUI	{ 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x06, 0x21, 0xA5 }
#elif NODE_ID==2
// node-2-otaa
#define LORAWAN_DEV_EUI { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x06, 0xB4, 0x66 }
#elif NODE_ID==3
// node-3-0taa
#define LORAWAN_DEV_EUI { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x06, 0xB4, 0x67 }
#endif

#ifndef LORAWAN_DEV_EUI
#error The NODE_ID defined do not correspond to any LORAWAN_DEV_EUI !
#endif

#endif