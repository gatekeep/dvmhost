/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__P25_LC__TSBK_H__)
#define  __P25_LC__TSBK_H__

#include "Defines.h"
#include "p25/edac/Trellis.h"
#include "p25/lc/LC.h"
#include "p25/lc/TDULC.h"
#include "p25/SiteData.h"
#include "edac/RS634717.h"
#include "lookups/IdenTableLookup.h"

#include <string>

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------
        class HOST_SW_API LC;
        class HOST_SW_API TDULC;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents link control data for TSDU packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TSBK {
        public:
            /// <summary>Initializes a new instance of the TSBK class.</summary>
            TSBK(SiteData siteData, lookups::IdenTable entry);
            /// <summary>Initializes a new instance of the TSBK class.</summary>
            TSBK(SiteData siteData, lookups::IdenTable entry, bool verbose);
            /// <summary>Initializes a new instance of the TSBK class.</summary>
            TSBK(LC* lc);
            /// <summary>Finalizes a instance of the TSBK class.</summary>
            ~TSBK();

            /// <summary>Equals operator.</summary>
            TSBK& operator=(const TSBK& data);

            /// <summary>Decode a trunking signalling block.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a trunking signalling block.</summary>
            void encode(uint8_t* data, bool singleBlock);

            /// <summary>Sets the flag to skip vendor opcode processing.</summary>
            void setVendorSkip(bool skip);

            /// <summary>Sets the callsign.</summary>
            void setCallsign(std::string callsign);

        public:
            /// <summary>Flag indicating verbose log output.</summary>
            __PROPERTY(bool, verbose, Verbose);

            /** Common Data */
            /// <summary>Flag indicating the link control data is protected.</summary>
            __PROPERTY(bool, protect, Protect);
            /// <summary>Link control opcode.</summary>
            __PROPERTY(uint8_t, lco, LCO);
            /// <summary>Manufacturer ID.</summary>
            __PROPERTY(uint8_t, mfId, MFId);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

            /// <summary>Flag indicating this is the last TSBK in a sequence of TSBKs.</summary>
            __PROPERTY(bool, lastBlock, LastBlock);
            /// <summary>Flag indicating this TSBK contains additional information.</summary>
            __PROPERTY(bool, aivFlag, AIV);
            /// <summary>Flag indicating this TSBK contains extended addressing.</summary>
            __PROPERTY(bool, extendedAddrFlag, EX);

            /// <summary>Service type.</summary>
            __PROPERTY(uint8_t, service, Service);
            /// <summary>Response type.</summary>
            __PROPERTY(uint8_t, response, Response);

            /// <summary>Configured network ID.</summary>
            __READONLY_PROPERTY(uint32_t, netId, NetId);
            /// <summary>Configured system ID.</summary>
            __READONLY_PROPERTY(uint32_t, sysId, SysId);

            /// <summary>Voice channel number.</summary>
            __PROPERTY(uint32_t, grpVchNo, GrpVchNo);

            /// <summary>Message value.</summary>
            __PROPERTY(uint32_t, messageValue, Message);
            /// <summary>Status value.</summary>
            __PROPERTY(uint8_t, statusValue, Status);

            /// <summary>Extended function opcode.</summary>
            __PROPERTY(uint32_t, extendedFunction, ExtendedFunction);

            /** Adjacent Site Data */
            /// <summary>Adjacent site CFVA flags.</summary>
            __PROPERTY(uint8_t, adjCFVA, AdjSiteCFVA);
            /// <summary>Adjacent site system ID.</summary>
            __PROPERTY(uint32_t, adjSysId, AdjSiteSysId);
            /// <summary>Adjacent site RFSS ID.</summary>
            __PROPERTY(uint8_t, adjRfssId, AdjSiteRFSSId);
            /// <summary>Adjacent site ID.</summary>
            __PROPERTY(uint8_t, adjSiteId, AdjSiteId);
            /// <summary>Adjacent site channel ID.</summary>
            __PROPERTY(uint8_t, adjChannelId, AdjSiteChnId);
            /// <summary>Adjacent site channel number.</summary>
            __PROPERTY(uint32_t, adjChannelNo, AdjSiteChnNo);
            /// <summary>Adjacent site service class.</summary>
            __PROPERTY(uint8_t, adjServiceClass, AdjSiteSvcClass);

            /** SCCB Data */
            /// <summary>SCCB channel ID 1.</summary>
            __PROPERTY(uint8_t, sccbChannelId1, SCCBChnId1);
            /// <summary>SCCB channel ID 2.</summary>
            __PROPERTY(uint8_t, sccbChannelId2, SCCBChnId2);
            /// <summary>Explicit SCCB channel number.</summary>
            __PROPERTY(uint32_t, sccbChannelNo, SCCBChnNo);

            /** Location Data */
            /// <summary>Location registration area.</summary>
            __PROPERTY(uint8_t, lra, LRA);

            /** Patch Group data */
            /// <summary>Patch super group ID.</summary>
            __PROPERTY(uint32_t, patchSuperGroupId, PatchSuperGroupId);
            /// <summary>1st patch group ID.</summary>
            __PROPERTY(uint32_t, patchGroup1Id, PatchGroup1Id);
            /// <summary>2nd patch group ID.</summary>
            __PROPERTY(uint32_t, patchGroup2Id, PatchGroup2Id);
            /// <summary>3rd patch group ID.</summary>
            __PROPERTY(uint32_t, patchGroup3Id, PatchGroup3Id);

            /** Service Options */
            /// <summary>Flag indicating the emergency bits are set.</summary>
            __PROPERTY(bool, emergency, Emergency);
            /// <summary>Flag indicating that encryption is enabled.</summary>
            __PROPERTY(bool, encrypted, Encrypted);
            /// <summary>Priority level for the traffic.</summary>
            __PROPERTY(uint8_t, priority, Priority);
            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROPERTY(bool, group, Group);

            /** Local Site data */
            /// <summary>Local Site Data.</summary>
            __PROPERTY_PLAIN(SiteData, siteData, siteData);
            /// <summary>Local Site Identity Entry.</summary>
            __PROPERTY_PLAIN(lookups::IdenTable, siteIdenEntry, siteIdenEntry);

        private:
            /// <summary>Initializes a new instance of the TSBK class.</summary>
            TSBK(SiteData siteData);

            friend class LC;
            friend class TDULC;
            edac::RS634717 m_rs;
            edac::Trellis m_trellis;
            bool m_vendorSkip;

            bool m_sndcpAutoAccess;
            bool m_sndcpReqAccess;
            uint16_t m_sndcpDAC;

            /** Local Site data */
            uint8_t* m_siteCallsign;
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TSBK_H__
