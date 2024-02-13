/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that apps can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_Fingerprint,
    0xce00073c,0xcea4,0x41e7,0x87,0xef,0x8d,0xb0,0xc7,0x89,0xf0,0xba);
// {ce00073c-cea4-41e7-87ef-8db0c789f0ba}
