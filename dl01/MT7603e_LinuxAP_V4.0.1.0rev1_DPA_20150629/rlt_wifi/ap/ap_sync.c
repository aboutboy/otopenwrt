/****************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ****************************************************************************
     
     Module Name:
     sync.c
     
     Abstract:
     Synchronization state machine related services
     
     Revision History:
     Who         When          What
     --------    ----------    ----------------------------------------------
     John Chang  08-04-2003    created for 11g soft-AP
     
 */

#include "rt_config.h"

#define OBSS_BEACON_RSSI_THRESHOLD		(-85)


/*
	==========================================================================
	Description:
		Process the received ProbeRequest from clients
	Parameters:
		Elem - msg containing the ProbeReq frame
	==========================================================================
 */
VOID APPeerProbeReqAction(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	PEER_PROBE_REQ_PARAM ProbeReqParam;
	HEADER_802_11 ProbeRspHdr;
	NDIS_STATUS NStatus;
	PUCHAR pOutBuffer = NULL;
	ULONG FrameLen = 0, TmpLen;
	LARGE_INTEGER FakeTimestamp;
	UCHAR DsLen = 1;
	UCHAR ErpIeLen = 1;
	UCHAR apidx = 0, PhyMode, SupRateLen;
	UCHAR RSNIe=IE_WPA, RSNIe2=IE_WPA2;
	BSS_STRUCT *mbss;
	struct wifi_dev *wdev;

#ifdef WSC_AP_SUPPORT
	UCHAR Addr3[MAC_ADDR_LEN];
	PFRAME_802_11 pFrame = (PFRAME_802_11)Elem->Msg;

	COPY_MAC_ADDR(Addr3, pFrame->Hdr.Addr3);
#endif /* WSC_AP_SUPPORT */

#ifdef WDS_SUPPORT
	/* if in bridge mode, no need to reply probe req. */
	if (pAd->WdsTab.Mode == WDS_BRIDGE_MODE)
		return;
#endif /* WDS_SUPPORT */


	if (PeerProbeReqSanity(pAd, Elem->Msg, Elem->MsgLen, &ProbeReqParam) == FALSE) {
DBGPRINT(RT_DEBUG_OFF, ("%s():shiang! PeerProbeReqSanity failed!\n", __FUNCTION__));
		return;
	}


	for(apidx=0; apidx<pAd->ApCfg.BssidNum; apidx++)
	{
		mbss = &pAd->ApCfg.MBSSID[apidx];
		wdev = &mbss->wdev;
		RSNIe = IE_WPA;
	
		if ((wdev->if_dev == NULL) || ((wdev->if_dev != NULL) &&
			!(RTMP_OS_NETDEV_STATE_RUNNING(wdev->if_dev))))
		{
			/* the interface is down, so we can not send probe response */
			continue;
		}
#ifdef AIRPLAY_SUPPORT
		if (pAd->ApCfg.MBSSID[apidx].bcn_buf.bBcnSntReq == FALSE)
			continue;
#endif /* AIRPLAY_SUPPORT */

		PhyMode = wdev->PhyMode;

		if ( ((((ProbeReqParam.SsidLen == 0) && (!mbss->bHideSsid)) ||
			   ((ProbeReqParam.SsidLen == mbss->SsidLen) && NdisEqualMemory(ProbeReqParam.Ssid, mbss->Ssid, (ULONG) ProbeReqParam.SsidLen)))
#ifdef CONFIG_HOTSPOT
			   && ProbeReqforHSAP(pAd, apidx, &ProbeReqParam)
#endif
			 )
#ifdef WSC_AP_SUPPORT
            /* buffalo WPS testbed STA send ProbrRequest ssid length = 32 and ssid are not AP , but DA are AP. for WPS test send ProbeResponse */
			|| ((ProbeReqParam.SsidLen == 32) && MAC_ADDR_EQUAL(Addr3, wdev->bssid) && (mbss->bHideSsid == 0))
#endif /* WSC_AP_SUPPORT */
		)
		{
			;
		}
		else {
			continue; /* check next BSS */
		}


		/* allocate and send out ProbeRsp frame */
		NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);
		if (NStatus != NDIS_STATUS_SUCCESS)
			return;

		MgtMacHeaderInit(pAd, &ProbeRspHdr, SUBTYPE_PROBE_RSP, 0, ProbeReqParam.Addr2, 
							wdev->if_addr, wdev->bssid);

		 if ((wdev->AuthMode == Ndis802_11AuthModeWPA) || (wdev->AuthMode == Ndis802_11AuthModeWPAPSK))
			RSNIe = IE_WPA;
		else if ((wdev->AuthMode == Ndis802_11AuthModeWPA2) ||(wdev->AuthMode == Ndis802_11AuthModeWPA2PSK))
			RSNIe = IE_WPA2;
#ifdef WAPI_SUPPORT
		else if ((wdev->AuthMode == Ndis802_11AuthModeWAICERT) || (wdev->AuthMode == Ndis802_11AuthModeWAIPSK))
			RSNIe = IE_WAPI;
#endif /* WAPI_SUPPORT */

		{
		SupRateLen = pAd->CommonCfg.SupRateLen;
		if (PhyMode == WMODE_B)
			SupRateLen = 4;

		MakeOutgoingFrame(pOutBuffer,                 &FrameLen,
						  sizeof(HEADER_802_11),      &ProbeRspHdr,
						  TIMESTAMP_LEN,              &FakeTimestamp,
						  2,                          &pAd->CommonCfg.BeaconPeriod,
						  2,                          &mbss->CapabilityInfo,
						  1,                          &SsidIe,
						  1,                          &mbss->SsidLen,
						  mbss->SsidLen,     mbss->Ssid,
						  1,                          &SupRateIe,
						  1,                          &SupRateLen,
						  SupRateLen,                 pAd->CommonCfg.SupRate,
						  1,                          &DsIe,
						  1,                          &DsLen,
						  1,                          &pAd->CommonCfg.Channel,
						  END_OF_ARGS);
		}

		if ((pAd->CommonCfg.ExtRateLen) && (PhyMode != WMODE_B))
		{
			MakeOutgoingFrame(pOutBuffer+FrameLen,      &TmpLen,
							  1,                        &ErpIe,
							  1,                        &ErpIeLen,
							  1,                        &pAd->ApCfg.ErpIeContent,
							  1,                        &ExtRateIe,
							  1,                        &pAd->CommonCfg.ExtRateLen,
							  pAd->CommonCfg.ExtRateLen,    pAd->CommonCfg.ExtRate,
							  END_OF_ARGS);
			FrameLen += TmpLen;
		}

#ifdef CUSTOMER_DCC_FEATURE
		if(pAd->CommonCfg.channelSwitch.CHSWMode == CHANNEL_SWITCHING_MODE)
		{
			UCHAR CSAIe=IE_CHANNEL_SWITCH_ANNOUNCEMENT;
			UCHAR CSALen=3;
			UCHAR CSAMode=1;
			UCHAR Period = (pAd->CommonCfg.channelSwitch.CHSWPeriod - pAd->CommonCfg.channelSwitch.CHSWCount);
			MakeOutgoingFrame(pOutBuffer+FrameLen,      &TmpLen,
							  1,                        &CSAIe,
							  1,                        &CSALen,
							  1,                        &CSAMode,
							  1,                        &pAd->CommonCfg.Channel,
							  1,                        &Period,
							  END_OF_ARGS);
			FrameLen += TmpLen;
#ifdef DOT11_N_SUPPORT
   			if (pAd->CommonCfg.bExtChannelSwitchAnnouncement)
			{
				HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE	HtExtChannelSwitchIe;

				build_ext_channel_switch_ie_New(pAd, &HtExtChannelSwitchIe);
				MakeOutgoingFrame(pOutBuffer + FrameLen,             &TmpLen,
							sizeof(HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE),	&HtExtChannelSwitchIe,
						  END_OF_ARGS);
				FrameLen += TmpLen;
			}

#endif /* DOT11_N_SUPPORT */
		}
#endif		
#ifdef A_BAND_SUPPORT
		/* add Channel switch announcement IE */
		if ((pAd->CommonCfg.Channel > 14)
			&& (pAd->CommonCfg.bIEEE80211H == 1)
			&& (pAd->Dot11_H.RDMode == RD_SWITCHING_MODE))
		{
			UCHAR CSAIe=IE_CHANNEL_SWITCH_ANNOUNCEMENT;
			UCHAR CSALen=3;
			UCHAR CSAMode=1;

			MakeOutgoingFrame(pOutBuffer+FrameLen,      &TmpLen,
							  1,                        &CSAIe,
							  1,                        &CSALen,
							  1,                        &CSAMode,
							  1,                        &pAd->CommonCfg.Channel,
							  1,                        &pAd->Dot11_H.CSCount,
							  END_OF_ARGS);
			FrameLen += TmpLen;
		}
#endif /* A_BAND_SUPPORT */

#ifdef DOT11_N_SUPPORT
		if (WMODE_CAP_N(PhyMode) &&
			(wdev->DesiredHtPhyInfo.bHtEnable))
		{
			ULONG TmpLen;
			UCHAR	HtLen, AddHtLen/*, NewExtLen*/;
			HT_CAPABILITY_IE HtCapabilityTmp;
#ifdef RT_BIG_ENDIAN
			ADD_HT_INFO_IE	addHTInfoTmp;
#endif

/* YF@20120419: Fix IOT Issue with Atheros STA on Windows 7 When IEEE80211H flag turn on. */

			HtLen = sizeof(pAd->CommonCfg.HtCapability);
			AddHtLen = sizeof(pAd->CommonCfg.AddHTInfo);
			//NewExtLen = 1;
			/*New extension channel offset IE is included in Beacon, Probe Rsp or channel Switch Announcement Frame */
#ifndef RT_BIG_ENDIAN
			NdisMoveMemory(&HtCapabilityTmp, &pAd->CommonCfg.HtCapability, HtLen);
			HtCapabilityTmp.HtCapInfo.ChannelWidth = pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth;

			MakeOutgoingFrame(pOutBuffer + FrameLen,            &TmpLen,
							  1,                                &HtCapIe,
							  1,                                &HtLen,
							 sizeof(HT_CAPABILITY_IE),          &HtCapabilityTmp,
							  1,                                &AddHtInfoIe,
							  1,                                &AddHtLen,
							 sizeof(ADD_HT_INFO_IE),          &pAd->CommonCfg.AddHTInfo,
							  END_OF_ARGS);
#else
			NdisMoveMemory(&HtCapabilityTmp, &pAd->CommonCfg.HtCapability, HtLen);
			HtCapabilityTmp.HtCapInfo.ChannelWidth = pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth;
			*(USHORT *)(&HtCapabilityTmp.HtCapInfo) = SWAP16(*(USHORT *)(&HtCapabilityTmp.HtCapInfo));
#ifdef UNALIGNMENT_SUPPORT
			{
				EXT_HT_CAP_INFO extHtCapInfo;

				NdisMoveMemory((PUCHAR)(&extHtCapInfo), (PUCHAR)(&HtCapabilityTmp.ExtHtCapInfo), sizeof(EXT_HT_CAP_INFO));
				*(USHORT *)(&extHtCapInfo) = cpu2le16(*(USHORT *)(&extHtCapInfo));
				NdisMoveMemory((PUCHAR)(&HtCapabilityTmp.ExtHtCapInfo), (PUCHAR)(&extHtCapInfo), sizeof(EXT_HT_CAP_INFO));		
			}
#else				
			*(USHORT *)(&HtCapabilityTmp.ExtHtCapInfo) = cpu2le16(*(USHORT *)(&HtCapabilityTmp.ExtHtCapInfo));
#endif /* UNALIGNMENT_SUPPORT */

			NdisMoveMemory(&addHTInfoTmp, &pAd->CommonCfg.AddHTInfo, AddHtLen);
			*(USHORT *)(&addHTInfoTmp.AddHtInfo2) = SWAP16(*(USHORT *)(&addHTInfoTmp.AddHtInfo2));
			*(USHORT *)(&addHTInfoTmp.AddHtInfo3) = SWAP16(*(USHORT *)(&addHTInfoTmp.AddHtInfo3));

			MakeOutgoingFrame(pOutBuffer + FrameLen,         &TmpLen,
								1,                           &HtCapIe,
								1,                           &HtLen,
								HtLen,                       &HtCapabilityTmp,
								1,                           &AddHtInfoIe,
								1,                           &AddHtLen,
								AddHtLen,                    &addHTInfoTmp,
								END_OF_ARGS);

#endif
			FrameLen += TmpLen;
		}
#endif /* DOT11_N_SUPPORT */

		/* Append RSN_IE when  WPA OR WPAPSK, */
		if (wdev->AuthMode < Ndis802_11AuthModeWPA)
			; /* enough information */
		else if ((wdev->AuthMode == Ndis802_11AuthModeWPA1WPA2) ||
			(wdev->AuthMode == Ndis802_11AuthModeWPA1PSKWPA2PSK))
		{
			MakeOutgoingFrame(pOutBuffer+FrameLen,      &TmpLen,
							  1,                        &RSNIe,
							  1,                        &mbss->RSNIE_Len[0],
							  mbss->RSNIE_Len[0],  mbss->RSN_IE[0],
							  1,                        &RSNIe2,
							  1,                        &mbss->RSNIE_Len[1],
							  mbss->RSNIE_Len[1],  mbss->RSN_IE[1],
							  END_OF_ARGS);
			FrameLen += TmpLen;
		}
		else
		{
#ifdef CONFIG_HOTSPOT_R2
			PHOTSPOT_CTRL pHSCtrl =  &mbss->HotSpotCtrl;
			extern UCHAR		OSEN_IE[];
			extern UCHAR		OSEN_IELEN;

			if ((pHSCtrl->HotSpotEnable == 0) && (pHSCtrl->bASANEnable == 1) && (wdev->AuthMode == Ndis802_11AuthModeWPA2))
			{
				RSNIe = IE_WPA;
				MakeOutgoingFrame(pOutBuffer+FrameLen,        &TmpLen,
						  1,                            &RSNIe,
						  1,                            &OSEN_IELEN,
						  OSEN_IELEN,					OSEN_IE,
						  END_OF_ARGS);
			}
			else
#endif
			{						
				MakeOutgoingFrame(pOutBuffer+FrameLen,      &TmpLen,
							  1,                        &RSNIe,
							  1,                        &mbss->RSNIE_Len[0],
							  mbss->RSNIE_Len[0],  mbss->RSN_IE[0],
							  END_OF_ARGS);
			}
			FrameLen += TmpLen;
		}

#ifdef CONFIG_HOTSPOT
		if (pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.HotSpotEnable)
		{
			ULONG TmpLen;
				
			/* Hotspot 2.0 Indication */
			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.HSIndicationIELen, 
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.HSIndicationIE, END_OF_ARGS);

			FrameLen += TmpLen;

			/* Interworking element */
			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.InterWorkingIELen, 
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.InterWorkingIE, END_OF_ARGS); 

			FrameLen += TmpLen;

			/* Advertisement Protocol element */
			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.AdvertisementProtoIELen, 
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.AdvertisementProtoIE, END_OF_ARGS); 

			FrameLen += TmpLen;

			/* Roaming Consortium element */ 
			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.RoamingConsortiumIELen, 
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.RoamingConsortiumIE, END_OF_ARGS); 

			FrameLen += TmpLen;

			/* P2P element */
			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.P2PIELen, 
							  pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.P2PIE, END_OF_ARGS); 

			FrameLen += TmpLen;
		}
#endif

		/* Extended Capabilities IE */
		{
			ULONG TmpLen;
			EXT_CAP_INFO_ELEMENT extCapInfo;
			UCHAR extInfoLen = sizeof(EXT_CAP_INFO_ELEMENT);

			NdisZeroMemory(&extCapInfo, extInfoLen);

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
			/* P802.11n_D1.10, HT Information Exchange Support */
			if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED) && (pAd->CommonCfg.Channel <= 14) &&
				(pAd->ApCfg.MBSSID[apidx].wdev.DesiredHtPhyInfo.bHtEnable) && 
				(pAd->CommonCfg.bBssCoexEnable == TRUE))
			{
				extCapInfo.BssCoexistMgmtSupport = 1;
			}
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */

#ifdef CONFIG_DOT11V_WNM
			if (pAd->ApCfg.MBSSID[apidx].WNMCtrl.ProxyARPEnable)
				extCapInfo.proxy_arp = 1;
#ifdef CONFIG_HOTSPOT_R2		
			if (pAd->ApCfg.MBSSID[apidx].WNMCtrl.WNMNotifyEnable)
				extCapInfo.wnm_notification= 1;
			if (pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.QosMapEnable)
				extCapInfo.qosmap= 1;
#endif		
#endif

#ifdef CONFIG_HOTSPOT
			if (pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.HotSpotEnable)
				extCapInfo.interworking = 1;
#endif

			MakeOutgoingFrame(pOutBuffer+FrameLen, &TmpLen,
								1, 			&ExtCapIe,
								1, 			&extInfoLen,
								extInfoLen, 	&extCapInfo,
								END_OF_ARGS);
				
			FrameLen += TmpLen;
		}

#ifdef AP_QLOAD_SUPPORT
		if (pAd->phy_ctrl.FlgQloadEnable != 0)
		{
#ifdef CONFIG_HOTSPOT_R2		
			if (pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.QLoadTestEnable == 1)
				FrameLen += QBSS_LoadElementAppend_HSTEST(pAd, pOutBuffer+FrameLen, apidx);
			else if (pAd->ApCfg.MBSSID[apidx].HotSpotCtrl.QLoadTestEnable == 0)	
#endif		
			FrameLen += QBSS_LoadElementAppend(pAd, pOutBuffer+FrameLen);
		}
#endif /* AP_QLOAD_SUPPORT */

		/* add WMM IE here */
		if (mbss->wdev.bWmmCapable)
		{
			UCHAR i;
			UCHAR WmeParmIe[26] = {IE_VENDOR_SPECIFIC, 24, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01, 0, 0};
			WmeParmIe[8] = pAd->ApCfg.BssEdcaParm.EdcaUpdateCount & 0x0f;
#ifdef UAPSD_SUPPORT
			UAPSD_MR_IE_FILL(WmeParmIe[8], &mbss->wdev.UapsdInfo);
#endif /* UAPSD_SUPPORT */
			for (i=QID_AC_BE; i<=QID_AC_VO; i++)
			{
				WmeParmIe[10+ (i*4)] = (i << 5) + /* b5-6 is ACI */
									   ((UCHAR)pAd->ApCfg.BssEdcaParm.bACM[i] << 4) +     /* b4 is ACM */
									   (pAd->ApCfg.BssEdcaParm.Aifsn[i] & 0x0f);		/* b0-3 is AIFSN */
				WmeParmIe[11+ (i*4)] = (pAd->ApCfg.BssEdcaParm.Cwmax[i] << 4) +	/* b5-8 is CWMAX */
									   (pAd->ApCfg.BssEdcaParm.Cwmin[i] & 0x0f);	/* b0-3 is CWMIN */
				WmeParmIe[12+ (i*4)] = (UCHAR)(pAd->ApCfg.BssEdcaParm.Txop[i] & 0xff);        /* low byte of TXOP */
				WmeParmIe[13+ (i*4)] = (UCHAR)(pAd->ApCfg.BssEdcaParm.Txop[i] >> 8);          /* high byte of TXOP */
			}

			MakeOutgoingFrame(pOutBuffer+FrameLen, &TmpLen,
							  26,                       WmeParmIe,
							  END_OF_ARGS);
			FrameLen += TmpLen;
		}

	    /* add country IE, power constraint IE */
		if (pAd->CommonCfg.bCountryFlag)
		{
#ifndef EXT_BUILD_CHANNEL_LIST
			ULONG TmpLen; 
#endif  /* !EXT_BUILD_CHANNEL_LIST */
			ULONG TmpLen2=0;
			UCHAR *TmpFrame = NULL;

			os_alloc_mem(NULL, (UCHAR **)&TmpFrame, 256);
			if (TmpFrame != NULL)
			{
				NdisZeroMemory(TmpFrame, 256);

				/* prepare channel information */
#ifdef EXT_BUILD_CHANNEL_LIST
				BuildBeaconChList(pAd, TmpFrame, &TmpLen2);
#else
				{
					UCHAR MaxTxPower = GetCuntryMaxTxPwr(pAd, pAd->CommonCfg.Channel);
					MakeOutgoingFrame(TmpFrame+TmpLen2,     &TmpLen,
										1,                 	&pAd->ChannelList[0].Channel,
										1,                 	&pAd->ChannelListNum,
										1,                 	&MaxTxPower,
										END_OF_ARGS);
					TmpLen2 += TmpLen;
				}
#endif /* EXT_BUILD_CHANNEL_LIST */

#ifdef DOT11K_RRM_SUPPORT
				if (IS_RRM_ENABLE(pAd, apidx)
					&& (pAd->CommonCfg.RegulatoryClass[0] != 0))
				{
					TmpLen2 = 0;
					NdisZeroMemory(TmpFrame, sizeof(TmpFrame));
					RguClass_BuildBcnChList(pAd, TmpFrame, &TmpLen2);
				}		
#endif /* DOT11K_RRM_SUPPORT */

				os_free_mem(NULL, TmpFrame);
			}
			else
				DBGPRINT(RT_DEBUG_ERROR, ("%s: Allocate memory fail!!!\n", __FUNCTION__));
		}
			
#ifdef DOT11K_RRM_SUPPORT
		if (IS_RRM_ENABLE(pAd, apidx))
		{
			InsertTpcReportIE(pAd, pOutBuffer+FrameLen, &FrameLen,
			RTMP_GetTxPwr(pAd, pAd->CommonCfg.MlmeTransmit), 0);
			RRM_InsertRRMEnCapIE(pAd, pOutBuffer+FrameLen, &FrameLen, apidx);
		}


		{
			INT loop;
			for (loop=0; loop<MAX_NUM_OF_REGULATORY_CLASS; loop++)
			{
				if (pAd->CommonCfg.RegulatoryClass[loop] == 0)
					break;
				InsertChannelRepIE(pAd, pOutBuffer+FrameLen, &FrameLen,
									(RTMP_STRING *)pAd->CommonCfg.CountryCode,
									pAd->CommonCfg.RegulatoryClass[loop]);
			}
		}

#ifndef APPLE_11K_IOT
		/* Insert BSS AC Access Delay IE. */
		RRM_InsertBssACDelayIE(pAd, pOutBuffer+FrameLen, &FrameLen);

		/* Insert BSS Available Access Capacity IE. */
		RRM_InsertBssAvailableACIE(pAd, pOutBuffer+FrameLen, &FrameLen);
#endif /* !APPLE_11K_IOT */

#endif /* DOT11K_RRM_SUPPORT */

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
	 	/* P802.11n_D3.03, 7.3.2.60 Overlapping BSS Scan Parameters IE */
	 	if (WMODE_CAP_N(PhyMode) &&
			(pAd->CommonCfg.Channel <= 14) &&
			(wdev->DesiredHtPhyInfo.bHtEnable) &&
			(pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth == 1))
	 	{
			OVERLAP_BSS_SCAN_IE  OverlapScanParam;
			ULONG	TmpLen;
			UCHAR	OverlapScanIE, ScanIELen;

			OverlapScanIE = IE_OVERLAPBSS_SCAN_PARM;
			ScanIELen = 14;
			OverlapScanParam.ScanPassiveDwell = cpu2le16(pAd->CommonCfg.Dot11OBssScanPassiveDwell);
			OverlapScanParam.ScanActiveDwell = cpu2le16(pAd->CommonCfg.Dot11OBssScanActiveDwell);
			OverlapScanParam.TriggerScanInt = cpu2le16(pAd->CommonCfg.Dot11BssWidthTriggerScanInt);
			OverlapScanParam.PassiveTalPerChannel = cpu2le16(pAd->CommonCfg.Dot11OBssScanPassiveTotalPerChannel);
			OverlapScanParam.ActiveTalPerChannel = cpu2le16(pAd->CommonCfg.Dot11OBssScanActiveTotalPerChannel);
			OverlapScanParam.DelayFactor = cpu2le16(pAd->CommonCfg.Dot11BssWidthChanTranDelayFactor);
			OverlapScanParam.ScanActThre = cpu2le16(pAd->CommonCfg.Dot11OBssScanActivityThre);
			
			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
								1,			&OverlapScanIE,
								1,			&ScanIELen,
								ScanIELen,	&OverlapScanParam,
								END_OF_ARGS);
			
			FrameLen += TmpLen;
	 	}

		/* 7.3.2.27 Extended Capabilities IE */
		{
			ULONG TmpLen;
			EXT_CAP_INFO_ELEMENT extCapInfo;
			UCHAR extInfoLen;


			extInfoLen = sizeof(EXT_CAP_INFO_ELEMENT);
			NdisZeroMemory(&extCapInfo, extInfoLen);

			/* P802.11n_D1.10, HT Information Exchange Support */
			if (WMODE_CAP_N(PhyMode) && (pAd->CommonCfg.Channel <= 14) &&
				(pAd->ApCfg.MBSSID[apidx].wdev.DesiredHtPhyInfo.bHtEnable) && 
				(pAd->CommonCfg.bBssCoexEnable == TRUE))
			{
				extCapInfo.BssCoexistMgmtSupport = 1;

				MakeOutgoingFrame(pOutBuffer+FrameLen, &TmpLen,
								1, 			&ExtCapIe,
								1, 			&extInfoLen,
								extInfoLen, 	&extCapInfo,
								END_OF_ARGS);
				
				FrameLen += TmpLen;
			}
		}
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */

	    /* add country IE, power constraint IE */
		if (pAd->CommonCfg.bCountryFlag)
		{
			ULONG TmpLen2=0;
			UCHAR TmpFrame[256];
			UCHAR CountryIe = IE_COUNTRY;
			UCHAR MaxTxPower=16;

#ifdef A_BAND_SUPPORT
			/* 
			Only 802.11a APs that comply with 802.11h are required to include
			a Power Constrint Element(IE=32) in beacons and probe response frames
			*/
			if (pAd->CommonCfg.Channel > 14 && pAd->CommonCfg.bIEEE80211H == TRUE)
			{
				/* prepare power constraint IE */
				MakeOutgoingFrame(pOutBuffer+FrameLen,    &TmpLen,
						3,                 	PowerConstraintIE,
						END_OF_ARGS);
						FrameLen += TmpLen;

#ifdef DOT11_VHT_AC
				if (WMODE_CAP_AC(PhyMode)) {
					ULONG TmpLen;
					UINT8 vht_txpwr_env_ie = IE_VHT_TXPWR_ENV;
					UINT8 ie_len;
					VHT_TXPWR_ENV_IE txpwr_env;

					ie_len = build_vht_txpwr_envelope(pAd, (UCHAR *)&txpwr_env);
					MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
								1,							&vht_txpwr_env_ie,
								1,							&ie_len,
								ie_len,						&txpwr_env,
								END_OF_ARGS);
					FrameLen += TmpLen;
				}
#endif /* DOT11_VHT_AC */
			}
#endif /* A_BAND_SUPPORT */

			NdisZeroMemory(TmpFrame, sizeof(TmpFrame));

			/* prepare channel information */
			MakeOutgoingFrame(TmpFrame+TmpLen2,     &TmpLen,
					1,                 	&pAd->ChannelList[0].Channel,
					1,                 	&pAd->ChannelListNum,
					1,                 	&MaxTxPower,
					END_OF_ARGS);
			TmpLen2 += TmpLen;

			/* need to do the padding bit check, and concatenate it */
			if ((TmpLen2%2) == 0)
			{
				UCHAR	TmpLen3 = TmpLen2+4;
				MakeOutgoingFrame(pOutBuffer+FrameLen,  &TmpLen,
					1,                 	&CountryIe,
					1,                 	&TmpLen3,
					3,                 	pAd->CommonCfg.CountryCode,
					TmpLen2+1,				TmpFrame,
					END_OF_ARGS);
			}
			else
			{
				UCHAR	TmpLen3 = TmpLen2+3;
				MakeOutgoingFrame(pOutBuffer+FrameLen,  &TmpLen,
						1,                 	&CountryIe,
						1,                 	&TmpLen3,
						3,                 	pAd->CommonCfg.CountryCode,
						TmpLen2,				TmpFrame,
						END_OF_ARGS);
			}
			FrameLen += TmpLen;
		}/* Country IE - */

#ifdef A_BAND_SUPPORT
		/* add Channel switch announcement IE */
		if ((pAd->CommonCfg.Channel > 14)
			&& (pAd->CommonCfg.bIEEE80211H == 1)
			&& (pAd->Dot11_H.RDMode == RD_SWITCHING_MODE))
		{
			UCHAR CSAIe=IE_CHANNEL_SWITCH_ANNOUNCEMENT;
			UCHAR CSALen=3;
			UCHAR CSAMode=1;

			MakeOutgoingFrame(pOutBuffer+FrameLen,      &TmpLen,
							  1,                        &CSAIe,
							  1,                        &CSALen,
							  1,                        &CSAMode,
							  1,                        &pAd->CommonCfg.Channel,
							  1,                        &pAd->Dot11_H.CSCount,
							  END_OF_ARGS);
			FrameLen += TmpLen;
#ifdef DOT11_N_SUPPORT
   			if (pAd->CommonCfg.bExtChannelSwitchAnnouncement)
			{
				HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE HtExtChannelSwitchIe;

				build_ext_channel_switch_ie(pAd, &HtExtChannelSwitchIe);
				MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
								  sizeof(HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE),	&HtExtChannelSwitchIe,
								  END_OF_ARGS);
			}
#endif /* DOT11_N_SUPPORT */
			FrameLen += TmpLen;
		}
#endif /* A_BAND_SUPPORT */

#ifdef DOT11_N_SUPPORT
		if (WMODE_CAP_N(PhyMode) &&
			(wdev->DesiredHtPhyInfo.bHtEnable))
		{
			ULONG TmpLen;
			UCHAR	HtLen, AddHtLen;/*, NewExtLen; */
#ifdef RT_BIG_ENDIAN
			HT_CAPABILITY_IE HtCapabilityTmp;
			ADD_HT_INFO_IE	addHTInfoTmp;
#endif
			HtLen = sizeof(pAd->CommonCfg.HtCapability);
			AddHtLen = sizeof(pAd->CommonCfg.AddHTInfo);

		if (pAd->bBroadComHT == TRUE)
		{
			UCHAR epigram_ie_len;
			UCHAR BROADCOM_HTC[4] = {0x0, 0x90, 0x4c, 0x33};
			UCHAR BROADCOM_AHTINFO[4] = {0x0, 0x90, 0x4c, 0x34};


			epigram_ie_len = HtLen + 4;
#ifndef RT_BIG_ENDIAN
			MakeOutgoingFrame(pOutBuffer + FrameLen,        &TmpLen,
						  1,                                &WpaIe,
							  1,                                &epigram_ie_len,
							  4,                                &BROADCOM_HTC[0],
							  HtLen,          					&pAd->CommonCfg.HtCapability,
							  END_OF_ARGS);
#else
				NdisMoveMemory(&HtCapabilityTmp, &pAd->CommonCfg.HtCapability, HtLen);
				*(USHORT *)(&HtCapabilityTmp.HtCapInfo) = SWAP16(*(USHORT *)(&HtCapabilityTmp.HtCapInfo));
#ifdef UNALIGNMENT_SUPPORT
			{
				EXT_HT_CAP_INFO extHtCapInfo;

				NdisMoveMemory((PUCHAR)(&extHtCapInfo), (PUCHAR)(&HtCapabilityTmp.ExtHtCapInfo), sizeof(EXT_HT_CAP_INFO));
				*(USHORT *)(&extHtCapInfo) = cpu2le16(*(USHORT *)(&extHtCapInfo));
				NdisMoveMemory((PUCHAR)(&HtCapabilityTmp.ExtHtCapInfo), (PUCHAR)(&extHtCapInfo), sizeof(EXT_HT_CAP_INFO));		
			}
#else				
			*(USHORT *)(&HtCapabilityTmp.ExtHtCapInfo) = cpu2le16(*(USHORT *)(&HtCapabilityTmp.ExtHtCapInfo));
#endif /* UNALIGNMENT_SUPPORT */

				MakeOutgoingFrame(pOutBuffer + FrameLen,         &TmpLen,
								1,                               &WpaIe,
								1,                               &epigram_ie_len,
								4,                               &BROADCOM_HTC[0],
								HtLen,                           &HtCapabilityTmp,
								END_OF_ARGS);
#endif

				FrameLen += TmpLen;

				epigram_ie_len = AddHtLen + 4;
#ifndef RT_BIG_ENDIAN
				MakeOutgoingFrame(pOutBuffer + FrameLen,          &TmpLen,
								  1,                              &WpaIe,
								  1,                              &epigram_ie_len,
								  4,                              &BROADCOM_AHTINFO[0],
								  AddHtLen, 					  &pAd->CommonCfg.AddHTInfo,
								  END_OF_ARGS);
#else
				NdisMoveMemory(&addHTInfoTmp, &pAd->CommonCfg.AddHTInfo, AddHtLen);
				*(USHORT *)(&addHTInfoTmp.AddHtInfo2) = SWAP16(*(USHORT *)(&addHTInfoTmp.AddHtInfo2));
				*(USHORT *)(&addHTInfoTmp.AddHtInfo3) = SWAP16(*(USHORT *)(&addHTInfoTmp.AddHtInfo3));

				MakeOutgoingFrame(pOutBuffer + FrameLen,         &TmpLen,
								1,                               &WpaIe,
								1,                               &epigram_ie_len,
								4,                               &BROADCOM_AHTINFO[0],
								AddHtLen,                        &addHTInfoTmp,
							  END_OF_ARGS);
#endif

				FrameLen += TmpLen;
			}

#ifdef DOT11_VHT_AC
			if (WMODE_CAP_AC(PhyMode) &&
				(pAd->CommonCfg.Channel > 14)) {
				FrameLen += build_vht_ies(pAd, (UCHAR *)(pOutBuffer+FrameLen), SUBTYPE_PROBE_RSP);
			}
#endif /* DOT11_VHT_AC */

		}
#endif /* DOT11_N_SUPPORT */


#ifdef WSC_AP_SUPPORT
		/* for windows 7 logo test */
		if ((mbss->WscControl.WscConfMode != WSC_DISABLE) &&
#ifdef DOT1X_SUPPORT
				(wdev->IEEE8021X == FALSE) && 
#endif /* DOT1X_SUPPORT */
				(wdev->WepStatus == Ndis802_11WEPEnabled))
		{
			/*
				Non-WPS Windows XP and Vista PCs are unable to determine if a WEP enalbed network is static key based 
				or 802.1X based. If the legacy station gets an EAP-Rquest/Identity from the AP, it assume the WEP
				network is 802.1X enabled & will prompt the user for 802.1X credentials. If the legacy station doesn't
				receive anything after sending an EAPOL-Start, it will assume the WEP network is static key based and
				prompt user for the WEP key. <<from "WPS and Static Key WEP Networks">>
				A WPS enabled AP should include this IE in the beacon when the AP is hosting a static WEP key network.  
				The IE would be 7 bytes long with the Extended Capability field set to 0 (all bits zero)
				http://msdn.microsoft.com/library/default.asp?url=/library/en-us/randz/protocol/securing_public_wi-fi_hotspots.asp 
			*/
			ULONG TempLen1 = 0;
			UCHAR PROVISION_SERVICE_IE[7] = {0xDD, 0x05, 0x00, 0x50, 0xF2, 0x05, 0x00};
			MakeOutgoingFrame(pOutBuffer+FrameLen,        &TempLen1,
								7,                            PROVISION_SERVICE_IE,
								END_OF_ARGS);
			FrameLen += TempLen1;
	    }

        /* add Simple Config Information Element */
        if ((mbss->WscControl.WscConfMode > WSC_DISABLE) && (mbss->WscIEProbeResp.ValueLen))
        {
    		ULONG WscTmpLen = 0;
    		MakeOutgoingFrame(pOutBuffer+FrameLen,                                  &WscTmpLen,
    						  mbss->WscIEProbeResp.ValueLen,   mbss->WscIEProbeResp.Value,
                              END_OF_ARGS);
    		FrameLen += WscTmpLen;
        }
#endif /* WSC_AP_SUPPORT */


#ifdef DOT11R_FT_SUPPORT
		/* The Mobility Domain information element (MDIE) is present in Probe-
		** Request frame when dot11FastBssTransitionEnable is set to true. */
		if (pAd->ApCfg.MBSSID[apidx].FtCfg.FtCapFlag.Dot11rFtEnable)
		{
			PFT_CFG pFtCfg = &pAd->ApCfg.MBSSID[apidx].FtCfg;
			FT_CAP_AND_POLICY FtCap;
			FtCap.field.FtOverDs = pFtCfg->FtCapFlag.FtOverDs;
			FtCap.field.RsrReqCap = pFtCfg->FtCapFlag.RsrReqCap;
			FT_InsertMdIE(pAd, pOutBuffer + FrameLen, &FrameLen,
							pFtCfg->FtMdId, FtCap);
		}
#endif /* DOT11R_FT_SUPPORT */


	/* 
		add Ralink-specific IE here - Byte0.b0=1 for aggregation, Byte0.b1=1 for piggy-back
		                                 Byte0.b3=1 for rssi-feedback 
	*/
	{
		ULONG TmpLen;
		UCHAR RalinkSpecificIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x00, 0x00, 0x00, 0x00};

		if (pAd->CommonCfg.bAggregationCapable)
			RalinkSpecificIe[5] |= 0x1;
		if (pAd->CommonCfg.bPiggyBackCapable)
			RalinkSpecificIe[5] |= 0x2;
#ifdef DOT11_N_SUPPORT
		if (pAd->CommonCfg.bRdg)
			RalinkSpecificIe[5] |= 0x4;
#endif /* DOT11_N_SUPPORT */
#ifdef RSSI_FEEDBACK
		if (ProbeReqParam.bRequestRssi == TRUE)
		{
		    MAC_TABLE_ENTRY *pEntry=NULL;

			DBGPRINT(RT_DEBUG_ERROR, ("SYNC - Send PROBE_RSP to %02x:%02x:%02x:%02x:%02x:%02x...\n",
										PRINT_MAC(Addr2)));
	    
			RalinkSpecificIe[5] |= 0x8;
			pEntry = MacTableLookup(pAd, Addr2);

			if (pEntry != NULL)
			{
				RalinkSpecificIe[6] = (UCHAR)pEntry->RssiSample.AvgRssi[0];
				RalinkSpecificIe[7] = (UCHAR)pEntry->RssiSample.AvgRssi[1];
				RalinkSpecificIe[8] = (UCHAR)pEntry->RssiSample.AvgRssi[2];
			}
		}
#endif /* RSSI_FEEDBACK */
		MakeOutgoingFrame(pOutBuffer+FrameLen, &TmpLen,
							9, RalinkSpecificIe,
							END_OF_ARGS);
		FrameLen += TmpLen;

	}
#ifdef AIRPLAY_SUPPORT
		if (AIRPLAY_ON(pAd))
		{ 
			ULONG	AirplayTmpLen = 0;		

			/*User user setting IE*/
			if (pAd->pAirplayIe && (pAd->AirplayIeLen != 0))
			{	
				//printk("AIRPLAY IE setting : MakeOutgoingFrame IeLen=%d\n",pAd->AirplayIeLen);
				//hex_dump("APPLE IE:", pAd->pAirplayIe , pAd->AirplayIeLen);
				MakeOutgoingFrame(pOutBuffer+FrameLen, &AirplayTmpLen,
									 pAd->AirplayIeLen, pAd->pAirplayIe,	 
										END_OF_ARGS);
				FrameLen += AirplayTmpLen;
			}
		}
#endif /* AIRPLAY_SUPPORT*/

	/* 802.11n 11.1.3.2.2 active scanning. sending probe response with MCS rate is */
	MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);
	MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);
	MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);
	MlmeFreeMemory(pAd, pOutBuffer);
	}
}


/*
	==========================================================================
	Description:
		parse the received BEACON

	NOTE:
		The only thing AP cares about received BEACON frames is to decide
		if there's any overlapped legacy BSS condition (OLBC).
		If OLBC happened, this AP should set the ERP->Use_Protection bit in its
		outgoing BEACON. The result is to tell all its clients to use RTS/CTS
		or CTS-to-self protection to protect B/G mixed traffic
	==========================================================================
 */


typedef struct
{
	ULONG	count;
	UCHAR	bssid[MAC_ADDR_LEN];
} BSSIDENTRY;

#ifdef CUSTOMER_DCC_FEATURE
VOID APChannelSwitch(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem) 
{
			
#ifdef DOT11_N_SUPPORT
	UCHAR RFChannel;
	if (WMODE_CAP_N(pAd->CommonCfg.PhyMode) &&
					pAd->CommonCfg.RegTransmitSetting.field.BW == BW_40)
		RFChannel = N_SetCenCh(pAd, pAd->CommonCfg.Channel);
	else
#endif /* DOT11_N_SUPPORT */
	RFChannel = pAd->CommonCfg.Channel;
	AsicSwitchChannel(pAd, RFChannel, FALSE);
	APStop(pAd);
	APStartUp(pAd);	
}
#endif

VOID APPeerBeaconAction(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	UCHAR Rates[MAX_LEN_OF_SUPPORTED_RATES], *pRates = NULL, RatesLen;
	BOOLEAN LegacyBssExist;
	CHAR RealRssi;
	UCHAR *VarIE = NULL;
	USHORT LenVIE;
	NDIS_802_11_VARIABLE_IEs *pVIE = NULL;
	UCHAR MaxSupportedRate = 0;
#ifdef APCLI_SUPPORT
	UINT ApCliIndex = 0;
#ifdef MT_MAC
        BOOLEAN ApCliWcid = FALSE;
#endif /* MT_MAC */
#endif /* APCLI_SUPPORT */
	BCN_IE_LIST *ie_list = NULL;


	/* allocate memory */
	os_alloc_mem(NULL, (UCHAR **)&ie_list, sizeof(BCN_IE_LIST));
	if (ie_list == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("%s: Allocate ie_list fail!!!\n", __FUNCTION__));
		goto LabelErr;
	}
	NdisZeroMemory(ie_list, sizeof(BCN_IE_LIST));

	/* Init Variable IE structure */
	os_alloc_mem(NULL, (UCHAR **)&VarIE, MAX_VIE_LEN);
	if (VarIE == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("%s: Allocate VarIE fail!!!\n", __FUNCTION__));
		goto LabelErr;
	}
	pVIE = (PNDIS_802_11_VARIABLE_IEs) VarIE;
	pVIE->Length = 0;



	pRates = (PUCHAR)Rates;

	ie_list->Channel = Elem->Channel;
	RealRssi = RTMPMaxRssi(pAd, ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_0),
							ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_1),
							ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_2));
	
	if (PeerBeaconAndProbeRspSanity(pAd,
								Elem->Msg,
								Elem->MsgLen,
								Elem->Channel,
								ie_list,
								&LenVIE,
								pVIE,
								FALSE))
	{

		/* ignore BEACON not in this channel */
		if (ie_list->Channel != pAd->CommonCfg.Channel
#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
			&& (pAd->CommonCfg.bOverlapScanning == FALSE)
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
			&& (!RTMP_CFG80211_VIF_P2P_CLI_ON(pAd))
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
#ifdef CFG80211_MULTI_STA
			&& (!RTMP_CFG80211_MULTI_STA_ON(pAd, pAd->cfg80211_ctrl.multi_sta_net_dev))
#endif /* CFG80211_MULTI_STA */
			)
		{
			goto __End_Of_APPeerBeaconAction;
		}

#ifdef IDS_SUPPORT
		/* Conflict SSID detection */
		RTMPConflictSsidDetection(pAd, (PUCHAR)ie_list->Ssid, ie_list->SsidLen,
								(CHAR)Elem->rssi_info.raw_rssi[0],
								(CHAR)Elem->rssi_info.raw_rssi[1],
								(CHAR)Elem->rssi_info.raw_rssi[2]);
#endif /* IDS_SUPPORT */
#ifdef CUSTOMER_DCC_FEATURE
		if(ie_list->Channel == pAd->CommonCfg.Channel && pAd->ApEnableBeaconTable == TRUE)
		{
			ULONG	Idx;
			CHAR 	Rssi = -127;
			UCHAR	Snr0 = 0;
			UCHAR	Snr1 = 0;
			UCHAR   SNR0 = 0;
			UCHAR	SNR1 = 0;
			
			SNR0 = ConvertToSnr(pAd,Elem->Snr0);
			SNR1 = ConvertToSnr(pAd,Elem->Snr1);
			Idx = BssTableSearch(&pAd->AvailableBSS, ie_list->Bssid, ie_list->Channel);
			if (Idx != BSS_NOT_FOUND)
			{
          	Rssi = pAd->AvailableBSS.BssEntry[Idx].Rssi;
			Snr0 = pAd->AvailableBSS.BssEntry[Idx].Snr0;
			Snr1 = pAd->AvailableBSS.BssEntry[Idx].Snr1;
			}
      		
			RealRssi = RTMPMaxRssi(pAd, ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_0),
								ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_1),
								ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_2));
	   	
       		RealRssi = RealRssi + pAd->BbpRssiToDbmDelta;
			if(Rssi == -127)
				Rssi = RealRssi;
			else
	   		Rssi = (((Rssi * (MOV_AVG_CONST - 1)) + RealRssi) >> MOV_AVG_CONST_SHIFT);

				
			if(Snr0 == 0)
				Snr0 = SNR0;
			else
				Snr0 = (((Snr0 * (MOV_AVG_CONST - 1)) + SNR0) >> MOV_AVG_CONST_SHIFT);

			if(Snr1 == 0)
				Snr1 = SNR1;
			else
				Snr1 = (((Snr1 * (MOV_AVG_CONST - 1)) + SNR1) >> MOV_AVG_CONST_SHIFT);
			

			Idx = BssTableSetEntry(pAd, &pAd->AvailableBSS,ie_list,Rssi, LenVIE, pVIE
#ifdef CUSTOMER_DCC_FEATURE
					,
					Snr0,Snr1
#endif
					);
			
			if (Idx != BSS_NOT_FOUND)
			{
				NdisMoveMemory(pAd->AvailableBSS.BssEntry[Idx].PTSF, &Elem->Msg[24], 4);
				NdisMoveMemory(&pAd->AvailableBSS.BssEntry[Idx].TTSF[0], &Elem->TimeStamp.u.LowPart, 4);
				NdisMoveMemory(&pAd->AvailableBSS.BssEntry[Idx].TTSF[4], &Elem->TimeStamp.u.LowPart, 4);
			}
		}
#endif
			
#ifdef DOT11_N_SUPPORT
		/* 40Mhz BSS Width Trigger events Intolerant devices */
		if ((RealRssi > OBSS_BEACON_RSSI_THRESHOLD) && (ie_list->HtCapability.HtCapInfo.Forty_Mhz_Intolerant)) /* || (HtCapabilityLen == 0))) */
		{
			Handle_BSS_Width_Trigger_Events(pAd);
		}
#endif /* DOT11_N_SUPPORT */

#ifdef DOT11_N_SUPPORT
		if ((pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth == BW_40)
#ifdef DOT11N_DRAFT3
			&& (pAd->CommonCfg.bOverlapScanning == FALSE)
#endif /* DOT11N_DRAFT3 */
		   )
		{
			if (pAd->CommonCfg.Channel<=14)
			{
#if defined(P2P_SUPPORT) || defined(RT_CFG80211_P2P_CONCURRENT_DEVICE) || defined(CFG80211_MULTI_STA)
				if(OPSTATUS_TEST_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED) && 
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
					RTMP_CFG80211_VIF_P2P_CLI_ON(pAd)
#else
					RTMP_CFG80211_MULTI_STA_ON(pAd, pAd->cfg80211_ctrl.multi_sta_net_dev)
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
				  )
				{
					if (ie_list->Channel != pAd->CommonCfg.Channel)
					{
						DBGPRINT(RT_DEBUG_INFO, ("Channel=%d is not equal as CommonCfg.Channel = %d.\n", ie_list->Channel, pAd->CommonCfg.Channel));
//						goto __End_Of_APPeerBeaconAction;
					}
				}
				else
#endif /* P2P_SUPPORT || RT_CFG80211_P2P_CONCURRENT_DEVICE */
				if (((pAd->CommonCfg.CentralChannel+2) != ie_list->Channel) &&
					((pAd->CommonCfg.CentralChannel-2) != ie_list->Channel))
				{
/*
					DBGPRINT(RT_DEBUG_TRACE, ("%02x:%02x:%02x:%02x:%02x:%02x is a legacy BSS (%d) \n",
								Bssid[0], Bssid[1], Bssid[2], Bssid[3], Bssid[4], Bssid[5], Channel));
*/
					goto __End_Of_APPeerBeaconAction;
				}
			}
			else
			{
				if (ie_list->Channel != pAd->CommonCfg.Channel)
				{
					//goto __End_Of_APPeerBeaconAction;
				}
			}
		}
#endif /* DOT11_N_SUPPORT */

                SupportRate(ie_list->SupRate, ie_list->SupRateLen, ie_list->ExtRate, ie_list->ExtRateLen, &pRates, &RatesLen, &MaxSupportedRate);
		
                if ((ie_list->Erp & 0x01) || (RatesLen <= 4))
			LegacyBssExist = TRUE;
		else
			LegacyBssExist = FALSE;

		if (LegacyBssExist && pAd->CommonCfg.DisableOLBCDetect == 0)
		{
			pAd->ApCfg.LastOLBCDetectTime = pAd->Mlme.Now32;
			
		}

#ifdef DOT11_N_SUPPORT
		if ((pAd->CommonCfg.bHTProtect)
			&& (ie_list->HtCapabilityLen == 0) && (RealRssi > OBSS_BEACON_RSSI_THRESHOLD))
		{
			
			pAd->ApCfg.LastNoneHTOLBCDetectTime = pAd->Mlme.Now32;
		}
#endif /* DOT11_N_SUPPORT */

#ifdef APCLI_SUPPORT
#ifdef MT_MAC
#ifdef MULTI_APCLI_SUPPORT
        if (Elem->Wcid == APCLI_MCAST_WCID(0) || Elem->Wcid == APCLI_MCAST_WCID(1))
        {
        	ApCliWcid = TRUE;
        }
#else /* MULTI_APCLI_SUPPORT */
        if (Elem->Wcid == APCLI_MCAST_WCID)
		    ApCliWcid = TRUE;
#endif /* !MULTI_APCLI_SUPPORT */
#endif
		if (Elem->Wcid < MAX_LEN_OF_MAC_TABLE
#ifdef MT_MAC
			|| ApCliWcid
#endif
		)
		{
			PMAC_TABLE_ENTRY pEntry = NULL;
#ifdef MT_MAC
            if (ApCliWcid == FALSE)
#endif
			pEntry = &pAd->MacTab.Content[Elem->Wcid];
#ifdef MT_MAC
            else
                pEntry = MacTableLookup(pAd, ie_list->Addr2);//Found the pEntry from Peer Bcn Content
#endif /* MT_MAC */
			
			
			if (pEntry && IS_ENTRY_APCLI(pEntry) && (pEntry->func_tb_idx < MAX_APCLI_NUM))
			{
				//printk("%s: ===> beacon FINAL WCID[%d]\n", __FUNCTION__, Elem->Wcid);
				ApCliIndex = pEntry->func_tb_idx;	
				pAd->ApCfg.ApCliTab[ApCliIndex].ApCliRcvBeaconTime = pAd->Mlme.Now32;

				if (pAd->CommonCfg.BBPCurrentBW == BW_40)
				{
					/* Check if root-ap change BW to 20 */
					if ((ie_list->AddHtInfo.AddHtInfo.ExtChanOffset == EXTCHA_NONE) &&
						(ie_list->AddHtInfo.AddHtInfo.RecomWidth == 0))
					{
#ifdef MAC_REPEATER_SUPPORT
						UINT ifIndex;
						UCHAR CliIdx;
						REPEATER_CLIENT_ENTRY *pReptEntry = NULL;
#endif /* MAC_REPEATER_SUPPORT */
						pEntry->HTPhyMode.field.BW = 0;
#ifdef MAC_REPEATER_SUPPORT
						ifIndex = pEntry->func_tb_idx;

						if (pAd->ApCfg.bMACRepeaterEn)
						{
							for(CliIdx = 0; CliIdx < MAX_EXT_MAC_ADDR_SIZE; CliIdx++)
							{
								pReptEntry = &pAd->ApCfg.ApCliTab[ifIndex].RepeaterCli[CliIdx];

								if ((pReptEntry->CliEnable) && (pReptEntry->CliValid))
								{
									pEntry = &pAd->MacTab.Content[pReptEntry->MacTabWCID];
									if (pEntry)
										pEntry->HTPhyMode.field.BW = 0;
								}
							}
						}
#endif /* MAC_REPEATER_SUPPORT */
						DBGPRINT(RT_DEBUG_INFO, ("FallBack APClient BW to 20MHz\n"));
					}

					/* Check if root-ap change BW to 40 */
					if ((ie_list->AddHtInfo.AddHtInfo.ExtChanOffset != EXTCHA_NONE) &&
						(ie_list->HtCapabilityLen > 0) &&
						(ie_list->HtCapability.HtCapInfo.ChannelWidth == 1))
					{
#ifdef MAC_REPEATER_SUPPORT
						UINT ifIndex;
						UCHAR CliIdx;
						REPEATER_CLIENT_ENTRY *pReptEntry = NULL;
#endif /* MAC_REPEATER_SUPPORT */
						pEntry->HTPhyMode.field.BW = 1;
#ifdef MAC_REPEATER_SUPPORT
						ifIndex = pEntry->func_tb_idx;

						if (pAd->ApCfg.bMACRepeaterEn)
						{
							for(CliIdx = 0; CliIdx < MAX_EXT_MAC_ADDR_SIZE; CliIdx++)
							{
								pReptEntry = &pAd->ApCfg.ApCliTab[ifIndex].RepeaterCli[CliIdx];

								if ((pReptEntry->CliEnable) && (pReptEntry->CliValid))
								{
									pEntry = &pAd->MacTab.Content[pReptEntry->MacTabWCID];
									if (pEntry)
										pEntry->HTPhyMode.field.BW = 1;
								}
							}
						}
#endif /* MAC_REPEATER_SUPPORT */
						DBGPRINT(RT_DEBUG_INFO, ("FallBack APClient BW to 40MHz\n"));
					}
				}
			}

			//ApCliWaitProbRsp(pAd, ApCliIndex);
			if ( /*ApCliWaitProbRsp(pAd, ApCliIndex) && */
		   	    (NdisEqualMemory(pAd->ApCfg.ApCliTab[ApCliIndex].CfgApCliBssid, ie_list->Bssid, MAC_ADDR_LEN)))
			{
				MlmeEnqueue(pAd, APCLI_SYNC_STATE_MACHINE, APCLI_MT2_PEER_BEACON, Elem->MsgLen, Elem->Msg, ApCliIndex);
			}
		}
#endif /* APCLI_SUPPORT */

#ifdef WDS_SUPPORT
		do
		{
			MAC_TABLE_ENTRY *pEntry;
			BOOLEAN bWmmCapable;

			/* check BEACON does in WDS TABLE. */
			pEntry = WdsTableLookup(pAd, ie_list->Addr2, FALSE);
			bWmmCapable = ie_list->EdcaParm.bValid ? TRUE : FALSE;

			if (pEntry)
			{
				WdsPeerBeaconProc(pAd, pEntry, ie_list->CapabilityInfo,
								MaxSupportedRate, RatesLen, bWmmCapable,
								ie_list->RalinkIe, &ie_list->HtCapability,
								ie_list->HtCapabilityLen);
			}
		} while(FALSE);
#endif /* WDS_SUPPORT */

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
		if (pAd->CommonCfg.bOverlapScanning == TRUE)
		{
			INT		index,secChIdx;
			//BOOLEAN		found = FALSE;
			ADD_HTINFO *pAdd_HtInfo;
			
			for (index = 0; index < pAd->ChannelListNum; index++)
			{
				/* found the effected channel, mark that. */
				if(pAd->ChannelList[index].Channel == ie_list->Channel)
				{
					secChIdx = -1;
					if (ie_list->HtCapabilityLen > 0 && ie_list->AddHtInfoLen > 0)
					{	/* This is a 11n AP. */
						pAd->ChannelList[index].bEffectedChannel |= EFFECTED_CH_PRIMARY; /* 2; 	// 2 for 11N 20/40MHz AP with primary channel set as this channel. */
						pAdd_HtInfo = &ie_list->AddHtInfo.AddHtInfo;
						if (pAdd_HtInfo->ExtChanOffset == EXTCHA_BELOW)
						{
#ifdef A_BAND_SUPPORT						
							if (ie_list->Channel > 14)
								secChIdx = ((index > 0) ? (index - 1) : -1);
							else
#endif /* A_BAND_SUPPORT */								
								secChIdx = ((index >= 4) ? (index - 4) : -1);
						}
						else if (pAdd_HtInfo->ExtChanOffset == EXTCHA_ABOVE)
						{
#ifdef A_BAND_SUPPORT						
							if (ie_list->Channel > 14)
								secChIdx = (((index+1) < pAd->ChannelListNum) ? (index + 1) : -1);
							else
#endif /* A_BAND_SUPPORT */								
								secChIdx = (((index+4) < pAd->ChannelListNum) ? (index + 4) : -1);
						}

						if (secChIdx >=0)
							pAd->ChannelList[secChIdx].bEffectedChannel |= EFFECTED_CH_SECONDARY; /* 1; */

						if ((pAd->CommonCfg.Channel != ie_list->Channel) || 
							(pAdd_HtInfo->ExtChanOffset  != pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset)
						)
							pAd->CommonCfg.BssCoexApCnt++;
					}
					else
					{
						/* This is a legacy AP. */
						pAd->ChannelList[index].bEffectedChannel |=  EFFECTED_CH_LEGACY; /* 4; 1 for legacy AP. */
						pAd->CommonCfg.BssCoexApCnt++;
					}

					//found = TRUE;
				}
			}
		}
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */
	}
	/* sanity check fail, ignore this frame */

__End_Of_APPeerBeaconAction:
/*#ifdef AUTO_CH_SELECT_ENHANCE */
#ifdef CONFIG_AP_SUPPORT
IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
{
	if (ie_list->Channel == pAd->ApCfg.AutoChannel_Channel)
	{
		if (AutoChBssSearchWithSSID(pAd, ie_list->Bssid, (PUCHAR)ie_list->Ssid, ie_list->SsidLen, ie_list->Channel) == BSS_NOT_FOUND)
			pAd->pChannelInfo->ApCnt[pAd->ApCfg.current_channel_index]++;
		AutoChBssInsertEntry(pAd, ie_list->Bssid, ie_list->Ssid, ie_list->SsidLen, ie_list->Channel, ie_list->NewExtChannelOffset, RealRssi);
	}
}
#endif /* CONFIG_AP_SUPPORT */
/*#endif // AUTO_CH_SELECT_ENHANCE */

LabelErr:
	if (VarIE != NULL)
		os_free_mem(NULL, VarIE);
	if (ie_list != NULL)
		os_free_mem(NULL, ie_list);

	return;
}

#ifdef AP_SCAN_SUPPORT
/*
    ==========================================================================
    Description:
    ==========================================================================
 */
VOID APInvalidStateWhenScan(RTMP_ADAPTER *pAd, MLME_QUEUE_ELEM *Elem)
{
	DBGPRINT(RT_DEBUG_TRACE, ("AYNC - InvalidStateWhenScan(state=%ld). Reset SYNC machine\n", pAd->Mlme.ApSyncMachine.CurrState));
}

/*
    ==========================================================================
    Description:
        Scan timeout handler, executed in timer thread
    ==========================================================================
 */
VOID APScanTimeout(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)FunctionContext;

	DBGPRINT(RT_DEBUG_TRACE, ("AP SYNC - Scan Timeout \n"));
	MlmeEnqueue(pAd, AP_SYNC_STATE_MACHINE, APMT2_SCAN_TIMEOUT, 0, NULL, 0);
	RTMP_MLME_HANDLER(pAd);
}

/*
    ==========================================================================
    Description:
        Scan timeout procedure. basically add channel index by 1 and rescan
    ==========================================================================
 */
VOID APScanTimeoutAction(RTMP_ADAPTER *pAd, MLME_QUEUE_ELEM *Elem)
{
#ifdef CONFIG_AP_SUPPORT
#ifdef CUSTOMER_DCC_FEATURE
	if(pAd->ScanCtrl.ScanGivenChannel == TRUE)
	{
		pAd->ScanCtrl.Channel = 0;
		pAd->ScanCtrl.ScanGivenChannel = FALSE;
	}
	else
#endif
#endif
	pAd->ScanCtrl.Channel = NextChannel(pAd, pAd->ScanCtrl.Channel);
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{		
		/*
			iwpriv set auto channel selection
			update the current index of the channel
		*/
#ifndef CUSTOMER_DCC_FEATURE
		if (pAd->ApCfg.bAutoChannelAtBootup == TRUE)
#endif
		{
			/* update current channel info */
			UpdateChannelInfo(pAd, pAd->ApCfg.current_channel_index, pAd->ApCfg.AutoChannelAlg);

			/* move to next channel */
			pAd->ApCfg.current_channel_index++;
			if (pAd->ApCfg.current_channel_index < pAd->ChannelListNum)
			{
				pAd->ApCfg.AutoChannel_Channel = pAd->ChannelList[pAd->ApCfg.current_channel_index].Channel;
			}
		}
	}
#endif /* CONFIG_AP_SUPPORT */
	ScanNextChannel(pAd, OPMODE_AP);
}

/*
    ==========================================================================
    Description:
        MLME SCAN req state machine procedure
    ==========================================================================
 */
VOID APMlmeScanReqAction(RTMP_ADAPTER *pAd, MLME_QUEUE_ELEM *Elem)
{
	BOOLEAN Cancelled;
	UCHAR Ssid[MAX_LEN_OF_SSID], SsidLen, ScanType, BssType;
#ifdef CONFIG_AP_SUPPORT
#ifdef CUSTOMER_DCC_FEATURE
	UINT Channel=0;
	UINT Timeout=0;
#endif
#endif
	/* Suspend MSDU transmission here */
	RTMPSuspendMsduTransmission(pAd);

	/* first check the parameter sanity */
	if (MlmeScanReqSanity(pAd, Elem->Msg, Elem->MsgLen, &BssType, (PCHAR)Ssid, &SsidLen, &ScanType
#ifdef CUSTOMER_DCC_FEATURE
						 , &Channel, &Timeout
#endif
						))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("AP SYNC - MlmeScanReqAction\n"));
		NdisGetSystemUpTime(&pAd->ApCfg.LastScanTime);

		RTMPCancelTimer(&pAd->ScanCtrl.APScanTimer, &Cancelled);

		/* record desired BSS parameters */
		pAd->ScanCtrl.BssType = BssType;
		pAd->ScanCtrl.ScanType = ScanType;
		pAd->ScanCtrl.SsidLen = SsidLen;
		NdisMoveMemory(pAd->ScanCtrl.Ssid, Ssid, SsidLen);
#ifdef CUSTOMER_DCC_FEATURE
		pAd->ScanCtrl.ScanGivenChannel = FALSE;
		pAd->ScanCtrl.ScanTime = Timeout;
		if( Channel != 0)
		{	
			pAd->ScanCtrl.Channel = Channel;
			pAd->ScanCtrl.ScanGivenChannel = TRUE;
		}
		else
#endif
		{
		/* start from the first channel */
			pAd->ScanCtrl.Channel = FirstChannel(pAd);
		}
		/* Let BBP register at 20MHz to do scan */
		bbp_set_bw(pAd, BW_20);
		DBGPRINT(RT_DEBUG_TRACE, ("SYNC - BBP R4 to 20MHz.l\n"));

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
			if (pAd->ApCfg.bAutoChannelAtBootup == TRUE)/* iwpriv set auto channel selection */
			{
				APAutoChannelInit(pAd);	
				pAd->ApCfg.AutoChannel_Channel = pAd->ChannelList[0].Channel;
			}
		}
#ifdef CUSTOMER_DCC_FEATURE
		{
			if(pAd->EnableChannelStatsCheck)
			{
			 	UINT32	macVal = 0xD7083F0F;  //EDCCA OFF //d7083f0f		
				RTMP_IO_WRITE32(pAd, WF_PHY_BASE + 0x0618, macVal);		
			}
			AsicGetChBusyCnt(pAd, 0);
			AsicGetRxStat(pAd, HQA_RX_RESET_PHY_COUNT);
		}
#endif
#endif /* CONFIG_AP_SUPPORT */
		ScanNextChannel(pAd, OPMODE_AP);
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, ("AP SYNC - MlmeScanReqAction() sanity check fail. BUG!!!\n"));
		pAd->Mlme.ApSyncMachine.CurrState = AP_SYNC_IDLE;
	}
}


/*
    ==========================================================================
    Description:
        peer sends beacon back when scanning
    ==========================================================================
 */
VOID APPeerBeaconAtScanAction(RTMP_ADAPTER *pAd, MLME_QUEUE_ELEM *Elem)
{
	//PFRAME_802_11 pFrame;
	UCHAR *VarIE = NULL;
	USHORT LenVIE;
	NDIS_802_11_VARIABLE_IEs *pVIE = NULL;
	CHAR RealRssi = -127;

	BCN_IE_LIST *ie_list = NULL;
#ifdef CUSTOMER_DCC_FEATURE
	UCHAR		Snr0 = Elem->Snr0;
	UCHAR		Snr1 = Elem->Snr1;
		
	Snr0 = ConvertToSnr(pAd, Snr0);
	Snr1 = ConvertToSnr(pAd, Snr1);
#endif

	os_alloc_mem(pAd, (UCHAR **)&ie_list, sizeof(BCN_IE_LIST));
	if (!ie_list) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: Alloc memory for ie_list fail!!!\n", __FUNCTION__));
		return;
	}
	NdisZeroMemory((UCHAR *)ie_list, sizeof(BCN_IE_LIST));

	/* allocate memory */
	os_alloc_mem(NULL, (UCHAR **)&VarIE, MAX_VIE_LEN);
	if (VarIE == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("%s: Allocate memory fail!!!\n", __FUNCTION__));
		goto LabelErr;
	}

	//pFrame = (PFRAME_802_11) Elem->Msg;
	/* Init Variable IE structure */
	pVIE = (PNDIS_802_11_VARIABLE_IEs) VarIE;
	pVIE->Length = 0;


	if (PeerBeaconAndProbeRspSanity(pAd,
					Elem->Msg, Elem->MsgLen, Elem->Channel,
					ie_list, &LenVIE, pVIE, FALSE))
    {
		ULONG Idx;
		CHAR  Rssi = -127;

		RealRssi = RTMPMaxRssi(pAd, ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_0),
								ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_1),
								ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_2));


		
		/* ignore BEACON not in this channel */
		if (ie_list->Channel != pAd->ScanCtrl.Channel
#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
			&& (pAd->CommonCfg.bOverlapScanning == FALSE)
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */
		   )
		{
			goto __End_Of_APPeerBeaconAtScanAction;
		}

#ifdef DOT11_N_SUPPORT
   		if ((RealRssi > OBSS_BEACON_RSSI_THRESHOLD) && (ie_list->HtCapability.HtCapInfo.Forty_Mhz_Intolerant)) /* || (HtCapabilityLen == 0))) */
		{
			Handle_BSS_Width_Trigger_Events(pAd);
		}
#endif /* DOT11_N_SUPPORT */

#ifdef IDS_SUPPORT
		/* Conflict SSID detection */
		if (ie_list->Channel == pAd->CommonCfg.Channel)
			RTMPConflictSsidDetection(pAd, ie_list->Ssid, ie_list->SsidLen,
							Elem->rssi_info.raw_rssi[0],
							Elem->rssi_info.raw_rssi[1],
							Elem->rssi_info.raw_rssi[2]);
#endif /* IDS_SUPPORT */

		/*
			This correct im-proper RSSI indication during SITE SURVEY issue.
			Always report bigger RSSI during SCANNING when receiving multiple BEACONs from the same AP.
			This case happens because BEACONs come from adjacent channels, so RSSI become weaker as we
			switch to more far away channels.
		*/
        Idx = BssTableSearch(&pAd->ScanTab, ie_list->Bssid, ie_list->Channel);
		if (Idx != BSS_NOT_FOUND)
        {
			UINT32 u4MaxBssEntrySize = sizeof(pAd->ScanTab.BssEntry)/sizeof(pAd->ScanTab.BssEntry[0]);
			if(Idx < u4MaxBssEntrySize)
            	Rssi = pAd->ScanTab.BssEntry[Idx].Rssi;
			else
			{
				DBGPRINT(RT_DEBUG_ERROR, ("%s(): Over buffer size!\n", __FUNCTION__));
			}
		}

		

        /* TODO: 2005-03-04 dirty patch. we should change all RSSI related variables to SIGNED SHORT for easy/efficient reading and calaulation */
		RealRssi = RTMPMaxRssi(pAd, ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_0), 
								ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_1),
								ConvertToRssi(pAd, &Elem->rssi_info, RSSI_IDX_2));
        if ((RealRssi + pAd->BbpRssiToDbmDelta) > Rssi)
            Rssi = RealRssi + pAd->BbpRssiToDbmDelta;

		Idx = BssTableSetEntry(pAd, &pAd->ScanTab, ie_list, Rssi, LenVIE, pVIE
#ifdef CUSTOMER_DCC_FEATURE
							,
							Snr0,
							Snr1
#endif
							);
		if (Idx != BSS_NOT_FOUND)
		{
			NdisMoveMemory(pAd->ScanTab.BssEntry[Idx].PTSF, &Elem->Msg[24], 4);
			NdisMoveMemory(&pAd->ScanTab.BssEntry[Idx].TTSF[0], &Elem->TimeStamp.u.LowPart, 4);
			NdisMoveMemory(&pAd->ScanTab.BssEntry[Idx].TTSF[4], &Elem->TimeStamp.u.LowPart, 4);
		}

#if defined(RT_CFG80211_P2P_CONCURRENT_DEVICE) || defined(CFG80211_MULTI_STA)
		if (RTMPEqualMemory(ie_list->Ssid, "DIRECT-", 7))
			DBGPRINT(RT_DEBUG_OFF, ("%s P2P_SCANNING: %s [%d], channel =%u\n", __FUNCTION__, ie_list->Ssid, Idx,Elem->Channel));

		if (ie_list->Channel != 0)
			Elem->Channel = ie_list->Channel;
		
		//DBGPRINT(RT_DEBUG_TRACE, ("APPeerBeaconAtScanAction : Update the SSID %s in Kernel Table, Elem->Channel=%u\n", ie_list->Ssid,Elem->Channel));
        RT_CFG80211_SCANNING_INFORM(pAd, Idx, /*ie_list->Channel*/Elem->Channel, (UCHAR *)Elem->Msg, Elem->MsgLen, RealRssi);
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE || CFG80211_MULTI_STA */
		
	}

	/* sanity check fail, ignored */
__End_Of_APPeerBeaconAtScanAction:
	/*scan beacon in pastive */
#ifdef CONFIG_AP_SUPPORT
IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
{
	if (ie_list->Channel == pAd->ApCfg.AutoChannel_Channel)
	{
		if (AutoChBssSearchWithSSID(pAd, ie_list->Bssid, (PUCHAR)ie_list->Ssid, ie_list->SsidLen, ie_list->Channel) == BSS_NOT_FOUND)
			pAd->pChannelInfo->ApCnt[pAd->ApCfg.current_channel_index]++;

		AutoChBssInsertEntry(pAd, ie_list->Bssid, (CHAR *)ie_list->Ssid, ie_list->SsidLen, ie_list->Channel, ie_list->NewExtChannelOffset, RealRssi);   
	}
}
#endif /* CONFIG_AP_SUPPORT */

LabelErr:
	if (VarIE != NULL)
		os_free_mem(NULL, VarIE);
	if (ie_list != NULL)
		os_free_mem(NULL, ie_list);

}

/*
    ==========================================================================
    Description:
        MLME Cancel the SCAN req state machine procedure
    ==========================================================================
 */
VOID APScanCnclAction(RTMP_ADAPTER *pAd, MLME_QUEUE_ELEM *Elem)
{
	BOOLEAN Cancelled;

	RTMPCancelTimer(&pAd->ScanCtrl.APScanTimer, &Cancelled);
	pAd->ScanCtrl.Channel = 0;
	ScanNextChannel(pAd, OPMODE_AP);

	return;
}

/*
    ==========================================================================
    Description:
        if ChannelSel is false, 
        	AP scans channels and lists the information of channels.
        if ChannelSel is true,
        	AP scans channels and selects an optimal channel. 

    NOTE:
    ==========================================================================
*/
VOID ApSiteSurvey(
	IN	PRTMP_ADAPTER  		pAd,
	IN	PNDIS_802_11_SSID	pSsid,
	IN	UCHAR				ScanType,
	IN	BOOLEAN				ChannelSel)
{
    MLME_SCAN_REQ_STRUCT    ScanReq;

    AsicDisableSync(pAd);

    BssTableInit(&pAd->ScanTab);
#ifdef CUSTOMER_DCC_FEATURE
	ChannelInfoResetNew(pAd);
#endif
    pAd->Mlme.ApSyncMachine.CurrState = AP_SYNC_IDLE;

	RTMPZeroMemory(ScanReq.Ssid, MAX_LEN_OF_SSID);
	ScanReq.SsidLen = 0;
	if (pSsid)
	{
	    ScanReq.SsidLen = pSsid->SsidLength;
	    NdisMoveMemory(ScanReq.Ssid, pSsid->Ssid, pSsid->SsidLength);
	}
    ScanReq.BssType = BSS_ANY;
    ScanReq.ScanType = ScanType;
    pAd->ApCfg.bAutoChannelAtBootup = ChannelSel;
    
    MlmeEnqueue(pAd, AP_SYNC_STATE_MACHINE, APMT2_MLME_SCAN_REQ, sizeof(MLME_SCAN_REQ_STRUCT), &ScanReq, 0);
    RTMP_MLME_HANDLER(pAd);
}

#ifdef CUSTOMER_DCC_FEATURE
UCHAR Channel2Index(   
	IN PRTMP_ADAPTER pAd,
	IN UCHAR channel)
{    UCHAR i;
	 for (i = 0; i < pAd->ChannelListNum; i++)
	 {        
	 	if (channel == pAd->ChannelList[i].Channel)
			return i;    
	 }    
	 return -1;
}

VOID ApSiteSurveyNew(
	IN	PRTMP_ADAPTER  			pAd,
	IN  	UINT 				channel,
	IN  	UINT 				timeout,
	IN	UCHAR				ScanType,
	IN	BOOLEAN				ChannelSel)
{
    MLME_SCAN_REQ_STRUCT    ScanReq;
	NdisZeroMemory(&ScanReq, sizeof(MLME_SCAN_REQ_STRUCT));
    AsicDisableSync(pAd);
    //printk("%s , %u, %u, %u",__func__,channel,timeout,ScanType); 
    BssTableInit(&pAd->ScanTab);
    ChannelInfoResetNew(pAd);
    pAd->Mlme.ApSyncMachine.CurrState = AP_SYNC_IDLE;
    ScanReq.BssType = BSS_ANY;
    ScanReq.ScanType = ScanType;
    ScanReq.Channel = channel;
    ScanReq.Timeout = timeout;
    pAd->ApCfg.bAutoChannelAtBootup = ChannelSel;
    pAd->ApCfg.current_channel_index = Channel2Index (pAd, channel);
    pAd->ChannelInfo.ChannelNo = channel; 
    MlmeEnqueue(pAd, AP_SYNC_STATE_MACHINE, APMT2_MLME_SCAN_REQ, sizeof(MLME_SCAN_REQ_STRUCT), &ScanReq, 0);
    RTMP_MLME_HANDLER(pAd);
}

VOID RemoveOldBssEntry(
	IN PRTMP_ADAPTER 		pAd)
{
		
	INT32 i;
	
	if(pAd->AvailableBSS.BssNr > 0)
	{
		for(i = 0; i < pAd->AvailableBSS.BssNr; i++)
		{
			if((jiffies_to_msecs(jiffies) - pAd->AvailableBSS.BssEntry[i].LastBeaconRxTime) >= 300000)
			{
				NdisZeroMemory(&pAd->AvailableBSS.BssEntry[i],sizeof(BSS_ENTRY));
				if(i != (pAd->AvailableBSS.BssNr - 1))
				{
					NdisCopyMemory(&pAd->AvailableBSS.BssEntry[i], &pAd->AvailableBSS.BssEntry[pAd->AvailableBSS.BssNr - 1], sizeof(BSS_ENTRY));
					NdisZeroMemory( &pAd->AvailableBSS.BssEntry[pAd->AvailableBSS.BssNr - 1],sizeof(BSS_ENTRY));
				}
				pAd->AvailableBSS.BssNr--;
			}
		}
	}
	
}

VOID APResetStreamingStatus(
	IN PRTMP_ADAPTER   pAd)
{
	UINT64 Time = jiffies_to_msecs(jiffies);

	if((Time - pAd->StreamingTypeStatus.BE_Time) > 1000)
		pAd->StreamingTypeStatus.BE = FALSE;
	
	if((Time - pAd->StreamingTypeStatus.BK_Time) > 1000)
		pAd->StreamingTypeStatus.BK = FALSE;
	
	if((Time - pAd->StreamingTypeStatus.VI_Time) > 1000)
		pAd->StreamingTypeStatus.VI = FALSE;
	
	if((Time - pAd->StreamingTypeStatus.VO_Time) > 1000)
		pAd->StreamingTypeStatus.VO= FALSE;

}

VOID RemoveOldStaList(
	IN PRTMP_ADAPTER   pAd)
{
	
	INT32 i;
	
	if(pAd->AllowedStaList.StaCount > 0)
	{
		for(i = 0; i < pAd->AllowedStaList.StaCount; i++)
		{
			if((jiffies_to_msecs(jiffies) - pAd->AllowedStaList.AllowedSta[i].DissocTime) >= 30000)
			{
				NdisZeroMemory(&pAd->AllowedStaList.AllowedSta[i],sizeof(ALLOWED_STA));
				if(i != (pAd->AllowedStaList.StaCount - 1))
				{
					NdisCopyMemory(&pAd->AllowedStaList.AllowedSta[i], &pAd->AllowedStaList.AllowedSta[pAd->AllowedStaList.StaCount - 1], sizeof(ALLOWED_STA));
					NdisZeroMemory( &pAd->AllowedStaList.AllowedSta[pAd->AllowedStaList.StaCount - 1], sizeof(ALLOWED_STA));
				}
				pAd->AllowedStaList.StaCount--;
			}
		}
	}
	
}

VOID ReadChannelStats(
	IN PRTMP_ADAPTER   pAd)
{
	UINT32 	msdr9, msdr18, OFDM_PD_CNT, CCK_PD_CNT,OFDM_MDRDY_CNT, CCK_MDRDY_CNT;
	UINT64	Time; 
	UINT32	TimeDiff, value, CCAcount = 0;
	UINT64 	temp;
//	RX_STA_CNT1_STRUC RxStaCnt1;
		
	TIMESTAMP_GET(pAd, Time);

	TimeDiff = Time - pAd->ChannelStats.LastReadTime;
	pAd->ChannelStats.TotalDuration += TimeDiff;
	pAd->ChannelStats.LastReadTime = Time; 
	pAd->ChannelStats.msec100counts ++;

	RTMP_IO_READ32(pAd, MIB_MSDR9, &msdr9);
	msdr9 = (msdr9 & 0xffffff);
	pAd->ChannelStats.ChBusytime += msdr9;
	pAd->ChannelStats.ChBusyTime1secValue += msdr9;
	
	RTMP_IO_READ32(pAd, MIB_MSDR18, &msdr18);
	msdr18 = (msdr18 & 0xffffff);
	pAd->ChannelStats.CCABusytime += msdr18;
	pAd->ChannelStats.CCABusyTime1secValue += msdr18;
		
	RTMP_IO_READ32(pAd,RO_PHYCTRL_STS0,&value);
	OFDM_PD_CNT = (value >> 16);
	CCK_PD_CNT = (value & 0xFFFF);

	RTMP_IO_READ32(pAd,RO_PHYCTRL_STS5,&value);
	OFDM_MDRDY_CNT = (value >> 16);
	CCK_MDRDY_CNT = (value & 0xFFFF);
	CCAcount = ((OFDM_PD_CNT - OFDM_MDRDY_CNT) + (CCK_PD_CNT - CCK_MDRDY_CNT));
	
	pAd->ChannelStats.FalseCCACount1secValue += CCAcount;
	pAd->ChannelStats.FalseCCACount += CCAcount;

	AsicGetRxStat(pAd, HQA_RX_RESET_PHY_COUNT);
	
	if((pAd->ChannelStats.msec100counts % 10) == 0)
	{ 
			if(pAd->ChannelStats.ChBusyTimeAvg == 0)
				pAd->ChannelStats.ChBusyTimeAvg = pAd->ChannelStats.ChBusyTime1secValue;
			else
				pAd->ChannelStats.ChBusyTimeAvg = (((pAd->ChannelStats.ChBusyTimeAvg * (MOV_AVG_CONST - 1)) + pAd->ChannelStats.ChBusyTime1secValue) >> MOV_AVG_CONST_SHIFT);

			if(pAd->ChannelStats.CCABusyTimeAvg == 0)
				pAd->ChannelStats.CCABusyTimeAvg = pAd->ChannelStats.CCABusyTime1secValue;
			else
				pAd->ChannelStats.CCABusyTimeAvg = (((pAd->ChannelStats.CCABusyTimeAvg * (MOV_AVG_CONST - 1)) + pAd->ChannelStats.CCABusyTime1secValue) >> MOV_AVG_CONST_SHIFT);

			if(pAd->ChannelStats.FalseCCACountAvg == 0)
				pAd->ChannelStats.FalseCCACountAvg = pAd->ChannelStats.FalseCCACount1secValue;
			else
				pAd->ChannelStats.FalseCCACountAvg = (((pAd->ChannelStats.FalseCCACountAvg * (MOV_AVG_CONST - 1)) + pAd->ChannelStats.FalseCCACount1secValue) >> MOV_AVG_CONST_SHIFT);
			
			if(pAd->ChannelStats.ChannelApActivityAvg == 0)
				pAd->ChannelStats.ChannelApActivityAvg = pAd->ChannelStats.ChannelApActivity1secValue;
			else
			{ 
				temp = (UINT64)((pAd->ChannelStats.ChannelApActivityAvg * (MOV_AVG_CONST - 1)) + pAd->ChannelStats.ChannelApActivity1secValue);
				do_div(temp , MOV_AVG_CONST);
		        pAd->ChannelStats.ChannelApActivityAvg = temp;
			}
			pAd->ChannelStats.ChBusyTime1secValue = 0;
			pAd->ChannelStats.CCABusyTime1secValue = 0;
			pAd->ChannelStats.FalseCCACount1secValue = 0;
			pAd->ChannelStats.ChannelApActivity1secValue = 0;
			pAd->ChannelStats.msec100counts = 10; /* restart  1 sec counter for AVG value */
			
	}

}


VOID ClearChannelStats(
	IN PRTMP_ADAPTER   pAd)
{
	UINT32 	mac_val;
	UINT32	Time;
	
	TIMESTAMP_GET(pAd, Time);
				
	pAd->ChannelStats.LastReadTime = Time; 
	mt7603_set_ed_cca(pAd, 1);
	RTMP_IO_READ32(pAd, MIB_MSDR9, &mac_val); //	p_CCA Time
	RTMP_IO_READ32(pAd, MIB_MSDR18, &mac_val); // 	p_ED Time
	AsicGetRxStat(pAd, HQA_RX_RESET_PHY_COUNT);
}

VOID ResetChannelStatus(
	IN PRTMP_ADAPTER   pAd)
{
	pAd->ChannelStats.LastReadTime = 0;
	pAd->ChannelStats.TotalDuration = 0;
	pAd->ChannelStats.CCABusytime = 0;
	pAd->ChannelStats.ChBusytime = 0;
	pAd->ChannelStats.FalseCCACount = 0;
	pAd->ChannelStats.ChannelApActivity = 0;
}

#endif

BOOLEAN ApScanRunning(RTMP_ADAPTER *pAd)
{
	return (pAd->Mlme.ApSyncMachine.CurrState == AP_SCAN_LISTEN) ? TRUE : FALSE;
}
#endif /* AP_SCAN_SUPPORT */


/*
	==========================================================================
	Description:
		The sync state machine,
	Parameters:
		Sm - pointer to the state machine
	Note:
		the state machine looks like the following

							AP_SYNC_IDLE
	APMT2_PEER_PROBE_REQ	peer_probe_req_action
	==========================================================================
 */
VOID APSyncStateMachineInit(
	IN RTMP_ADAPTER *pAd,
	IN STATE_MACHINE *Sm,
	OUT STATE_MACHINE_FUNC Trans[])
{
	StateMachineInit(Sm, (STATE_MACHINE_FUNC *)Trans, AP_MAX_SYNC_STATE, AP_MAX_SYNC_MSG, (STATE_MACHINE_FUNC)Drop, AP_SYNC_IDLE, AP_SYNC_MACHINE_BASE);

	StateMachineSetAction(Sm, AP_SYNC_IDLE, APMT2_PEER_PROBE_REQ, (STATE_MACHINE_FUNC)APPeerProbeReqAction);
	StateMachineSetAction(Sm, AP_SYNC_IDLE, APMT2_PEER_BEACON, (STATE_MACHINE_FUNC)APPeerBeaconAction);
#ifdef CUSTOMER_DCC_FEATURE	
	StateMachineSetAction(Sm, AP_SYNC_IDLE, APMT2_CHANNEL_SWITCH, (STATE_MACHINE_FUNC)APChannelSwitch);
#endif
#if defined(P2P_SUPPORT) || defined(RT_CFG80211_P2P_SUPPORT) || defined(CFG80211_MULTI_STA)
	StateMachineSetAction(Sm, AP_SYNC_IDLE, APMT2_PEER_PROBE_RSP, (STATE_MACHINE_FUNC)APPeerBeaconAtScanAction);
#endif /* P2P_SUPPORT || RT_CFG80211_P2P_SUPPORT || CFG80211_MULTI_STA */
#ifdef AP_SCAN_SUPPORT
	StateMachineSetAction(Sm, AP_SYNC_IDLE, APMT2_MLME_SCAN_REQ, (STATE_MACHINE_FUNC)APMlmeScanReqAction);

	/* scan_listen state */
	StateMachineSetAction(Sm, AP_SCAN_LISTEN, APMT2_MLME_SCAN_REQ, (STATE_MACHINE_FUNC)APInvalidStateWhenScan);
	StateMachineSetAction(Sm, AP_SCAN_LISTEN, APMT2_PEER_BEACON, (STATE_MACHINE_FUNC)APPeerBeaconAtScanAction);
	StateMachineSetAction(Sm, AP_SCAN_LISTEN, APMT2_PEER_PROBE_RSP, (STATE_MACHINE_FUNC)APPeerBeaconAtScanAction);
	StateMachineSetAction(Sm, AP_SCAN_LISTEN, APMT2_SCAN_TIMEOUT, (STATE_MACHINE_FUNC)APScanTimeoutAction);
	StateMachineSetAction(Sm, AP_SCAN_LISTEN, APMT2_MLME_SCAN_CNCL, (STATE_MACHINE_FUNC)APScanCnclAction);

	RTMPInitTimer(pAd, &pAd->ScanCtrl.APScanTimer, GET_TIMER_FUNCTION(APScanTimeout), pAd, FALSE);
#endif /* AP_SCAN_SUPPORT */
}


VOID SupportRate(
	IN PUCHAR SupRate,
	IN UCHAR SupRateLen,
	IN PUCHAR ExtRate,
	IN UCHAR ExtRateLen,
	OUT PUCHAR *ppRates,
	OUT PUCHAR RatesLen,
	OUT PUCHAR pMaxSupportRate)
{
	INT i;

	*pMaxSupportRate = 0;

	if ((SupRateLen <= MAX_LEN_OF_SUPPORTED_RATES) && (SupRateLen > 0))
	{
		NdisMoveMemory(*ppRates, SupRate, SupRateLen);
		*RatesLen = SupRateLen;
	}
	else
	{
		/* HT rate not ready yet. return true temporarily. rt2860c */
		/*DBGPRINT(RT_DEBUG_TRACE, ("PeerAssocReqSanity - wrong IE_SUPP_RATES\n")); */
		*RatesLen = 8;
		*(*ppRates + 0) = 0x82;
		*(*ppRates + 1) = 0x84;
		*(*ppRates + 2) = 0x8b;
		*(*ppRates + 3) = 0x96;
		*(*ppRates + 4) = 0x12;
		*(*ppRates + 5) = 0x24;
		*(*ppRates + 6) = 0x48;
		*(*ppRates + 7) = 0x6c;
		DBGPRINT(RT_DEBUG_TRACE, ("SUPP_RATES., Len=%d\n", SupRateLen));
	}

	if (ExtRateLen + *RatesLen <= MAX_LEN_OF_SUPPORTED_RATES)
	{
		NdisMoveMemory((*ppRates + (ULONG)*RatesLen), ExtRate, ExtRateLen);
		*RatesLen = (*RatesLen) + ExtRateLen;
	}
	else
	{
		NdisMoveMemory((*ppRates + (ULONG)*RatesLen), ExtRate, MAX_LEN_OF_SUPPORTED_RATES - (*RatesLen));
		*RatesLen = MAX_LEN_OF_SUPPORTED_RATES;
	}

	

	for (i = 0; i < *RatesLen; i++)
	{
		if(*pMaxSupportRate < (*(*ppRates + i) & 0x7f))
			*pMaxSupportRate = (*(*ppRates + i) & 0x7f);
	}

	return;
}

#ifdef DOT11_N_SUPPORT
/* Regulatory classes in the USA */

typedef struct
{
	UCHAR	regclass;		/* regulatory class */
	UCHAR	spacing;		/* 0: 20Mhz, 1: 40Mhz */
	UCHAR	channelset[16];	/* max 15 channels, use 0 as terminator */
} REG_CLASS;

REG_CLASS reg_class[] =
{
	{  1, 0, {36, 40, 44, 48, 0}},
	{  2, 0, {52, 56, 60, 64, 0}},
	{  3, 0, {149, 153, 157, 161, 0}},
	{  4, 0, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}},
	{  5, 0, {165, 0}},
	{ 22, 1, {36, 44, 0}},
	{ 23, 1, {52, 60, 0}},
	{ 24, 1, {100, 108, 116, 124, 132, 0}},
	{ 25, 1, {149, 157, 0}},
	{ 26, 1, {149, 157, 0}},
	{ 27, 1, {40, 48, 0}},
	{ 28, 1, {56, 64, 0}},
	{ 29, 1, {104, 112, 120, 128, 136, 0}},
	{ 30, 1, {153, 161, 0}},
	{ 31, 1, {153, 161, 0}},
	{ 32, 1, {1, 2, 3, 4, 5, 6, 7, 0}},
	{ 33, 1, {5, 6, 7, 8, 9, 10, 11, 0}},
	{ 0,  0, {0}}			/* end */
};


UCHAR get_regulatory_class(RTMP_ADAPTER *pAd)
{
	int i=0;
	UCHAR regclass = 0;

	do
	{
#ifdef CUSTOMER_DCC_FEATURE
		if (reg_class[i].spacing == pAd->CommonCfg.RegTransmitSetting.field.BW)
#else
		if (reg_class[i].spacing == pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth)
#endif
		{
			int j=0;

			do
			{
				if (reg_class[i].channelset[j] == pAd->CommonCfg.Channel)
				{
					regclass = reg_class[i].regclass;
					break;
				}
				j++;
			} while (reg_class[i].channelset[j] != 0);
		}
		i++;
	} while (reg_class[i].regclass != 0);

	ASSERT(regclass);

	return regclass;
}


void build_ext_channel_switch_ie(
	IN PRTMP_ADAPTER pAd,
	IN HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE *pIE)
{

	pIE->ID = IE_EXT_CHANNEL_SWITCH_ANNOUNCEMENT;
	pIE->Length = 4;
	pIE->ChannelSwitchMode = 1;	/*no further frames */
	pIE->NewRegClass = get_regulatory_class(pAd);
	pIE->NewChannelNum = pAd->CommonCfg.Channel;
    pIE->ChannelSwitchCount = (pAd->Dot11_H.CSPeriod - pAd->Dot11_H.CSCount - 1);
}
#ifdef CUSTOMER_DCC_FEATURE
void build_ext_channel_switch_ie_New(
	IN PRTMP_ADAPTER pAd,
	IN HT_EXT_CHANNEL_SWITCH_ANNOUNCEMENT_IE *pIE)
{

	pIE->ID = IE_EXT_CHANNEL_SWITCH_ANNOUNCEMENT;
	pIE->Length = 4;
	pIE->ChannelSwitchMode = 1;	/*no further frames */
	pIE->NewRegClass = get_regulatory_class(pAd);
	pIE->NewChannelNum = pAd->CommonCfg.Channel;
    pIE->ChannelSwitchCount = pAd->CommonCfg.channelSwitch.CHSWPeriod - pAd->CommonCfg.channelSwitch.CHSWCount;
}
#endif
#endif /* DOT11_N_SUPPORT */

