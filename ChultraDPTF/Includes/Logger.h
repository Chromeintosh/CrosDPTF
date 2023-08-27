//
//  Logger.h
//  ChultraSOF
//
//  Created by Gwydien on 8/22/23.
//

#ifndef Logger_h
#define Logger_h

#define IOLogInfo(format, ...) do { IOLog("DPTF - Info: " format "\n", ## __VA_ARGS__); } while (0)
#define IOLogError(format, ...) do { IOLog("DPTF - Error: " format "\n", ## __VA_ARGS__); } while (0)

#ifdef DEBUG
#define IOLogDebug(format, ...) do { IOLog("DPTF - Debug: " format "\n", ## __VA_ARGS__); } while (0)
#else
#define IOLogDebug(format, ...)
#endif // DEBUG

#endif /* Logger_h */
