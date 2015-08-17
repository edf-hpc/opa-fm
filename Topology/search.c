/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

#include "topology.h"
#include "topology_internal.h"
#include <stl_helper.h>

#ifndef __VXWORKS__
#include <fnmatch.h>
#endif

// Functions to search the topology

// search for the PortData corresponding to the given node and port number
PortData * FindNodePort(NodeData *nodep, uint8 port)
{
	cl_map_item_t *mi;

	mi = cl_qmap_get(&nodep->Ports, port);
	if (mi == cl_qmap_end(&nodep->Ports))
		return NULL;
	return PARENT_STRUCT(mi, PortData, NodePortsEntry);
}

// a cl_pfm_qmap_item_compare_t function
// compare a LID (in key) against the range of Lids assigned to the
// given PortData item
// each PortData has LIDs from LID to LID|((1<<LMC)-1)
// Hence when LMC=0 (eg. 1<<0 -> 1), PortData has exactly 1 LID
// When LMC=1, PortData has 2 LIDs [LID to LID+1]
// When LMC=2, PortData has 4 LIDs [LID to LID+3]
// etc.
// Since non-zero LMC requires low "LMC" bits of LID to be 0 we use |
// however so we could also use an + instead of a |
static int CompareLid(const cl_map_item_t *mi, const uint64 key)
{
	PortData *portp = PARENT_STRUCT(mi, PortData, AllLidsEntry);

	if ((portp->PortInfo.LID |
					((1<<portp->PortInfo.s1.LMC)-1)) < key)
		return -1;
	else if (portp->PortInfo.LID > key)
		return 1;
	else
		return 0;
}

// search for the PortData corresponding to the given lid
// accounts for LMC giving a port a range of lids
PortData * FindLid(FabricData_t *fabricp, uint16 lid)
{
	if (fabricp->flags & FF_LIDARRAY) {
		if (lid > LID_UCAST_END)
			return NULL;
		return fabricp->u.LidMap[lid];
	} else {
		cl_map_item_t *mi;

		mi = cl_qmap_get_item_compare(&fabricp->u.AllLids, lid, CompareLid);
		if (mi == cl_qmap_end(&fabricp->u.AllLids))
			return NULL;
		return PARENT_STRUCT(mi, PortData, AllLidsEntry);
	}
}

// search for the PortData corresponding to the given port Guid
PortData * FindPortGuid(FabricData_t *fabricp, EUI64 guid)
{
	LIST_ITEM *p;

	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		if (portp->PortGUID == guid)
			return portp;
	}
	return NULL;
}

// search for the NodeData corresponding to the given node Guid
NodeData * FindNodeGuid(const FabricData_t *fabricp, EUI64 guid)
{
	cl_map_item_t *mi;

	mi = cl_qmap_get(&fabricp->AllNodes, guid);
	if (mi == cl_qmap_end(&fabricp->AllNodes))
		return NULL;
	return PARENT_STRUCT(mi, NodeData, AllNodesEntry);
}

// search for the NodeData corresponding to the given node name
FSTATUS FindNodeName(FabricData_t *fabricp, char *name, Point *pPoint, int silent)
{
	cl_map_item_t *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=cl_qmap_head(&fabricp->AllNodes); p != cl_qmap_end(&fabricp->AllNodes); p = cl_qmap_next(p)) {
		NodeData *nodep = PARENT_STRUCT(p, NodeData, AllNodesEntry);
		if (strncmp((char*)nodep->NodeDesc.NodeString,
					name, STL_NODE_DESCRIPTION_ARRAY_SIZE) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_NODE_LIST, nodep);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		if(!silent)
			fprintf(stderr, "%s: Node name Not Found: %s\n", g_Top_cmdname, name);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

#ifndef __VXWORKS__
// search for the NodeData corresponding to the given node name pattern
FSTATUS FindNodeNamePat(FabricData_t *fabricp, char *pattern, Point *pPoint)
{
	cl_map_item_t *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=cl_qmap_head(&fabricp->AllNodes); p != cl_qmap_end(&fabricp->AllNodes); p = cl_qmap_next(p)) {
		NodeData *nodep = PARENT_STRUCT(p, NodeData, AllNodesEntry);
		char Name[STL_NODE_DESCRIPTION_ARRAY_SIZE+1];

		strncpy(Name, (char*)nodep->NodeDesc.NodeString, STL_NODE_DESCRIPTION_ARRAY_SIZE);
		Name[STL_NODE_DESCRIPTION_ARRAY_SIZE] = '\0';
		if (fnmatch(pattern, Name, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_NODE_LIST, nodep);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Node name pattern Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

// search for nodes whose ExpectedNode has the given node details
FSTATUS FindNodeDetailsPat(FabricData_t *fabricp, const char* pattern, Point *pPoint)
{
	cl_map_item_t *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=cl_qmap_head(&fabricp->AllNodes); p != cl_qmap_end(&fabricp->AllNodes); p = cl_qmap_next(p)) {
		NodeData *nodep = PARENT_STRUCT(p, NodeData, AllNodesEntry);
		if (! nodep->enodep || ! nodep->enodep->details)
			continue;	// no node details information
		if (fnmatch(pattern, nodep->enodep->details, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_NODE_LIST, nodep);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Node Details Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}
#endif

// search for the NodeData corresponding to the given node type
FSTATUS FindNodeType(FabricData_t *fabricp, NODE_TYPE type, Point *pPoint)
{
	cl_map_item_t *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=cl_qmap_head(&fabricp->AllNodes); p != cl_qmap_end(&fabricp->AllNodes); p = cl_qmap_next(p)) {
		NodeData *nodep = PARENT_STRUCT(p, NodeData, AllNodesEntry);
		if (nodep->NodeInfo.NodeType == type)
		{
			status = PointListAppend(pPoint, POINT_TYPE_NODE_LIST, nodep);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: No nodes of type %s found\n",
					   	g_Top_cmdname, StlNodeTypeToText(type));
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

#if !defined(VXWORKS) || defined(BUILD_DMC)
// search for the Ioc corresponding to the given ioc name
FSTATUS FindIocName(FabricData_t *fabricp, char *name, Point *pPoint)
{
	cl_map_item_t *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=cl_qmap_head(&fabricp->AllIOCs); p != cl_qmap_end(&fabricp->AllIOCs); p = cl_qmap_next(p)) {
		IocData *iocp = PARENT_STRUCT(p, IocData, AllIOCsEntry);

		if (strncmp((char*)iocp->IocProfile.IDString, name, IOC_IDSTRING_SIZE) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_IOC_LIST, iocp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: IOC name Not Found: %s\n", g_Top_cmdname, name);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

#ifndef __VXWORKS__
// search for the Ioc corresponding to the given ioc name pattern
FSTATUS FindIocNamePat(FabricData_t *fabricp, char *pattern, Point *pPoint)
{
	cl_map_item_t *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=cl_qmap_head(&fabricp->AllIOCs); p != cl_qmap_end(&fabricp->AllIOCs); p = cl_qmap_next(p)) {
		IocData *iocp = PARENT_STRUCT(p, IocData, AllIOCsEntry);
		char Name[IOC_IDSTRING_SIZE+1];

		strncpy(Name, (char*)iocp->IocProfile.IDString, IOC_IDSTRING_SIZE);
		Name[IOC_IDSTRING_SIZE] = '\0';
		if (fnmatch(pattern, Name, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_IOC_LIST, iocp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: IOC name pattern Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}
#endif

// search for the Ioc corresponding to the given ioc type
FSTATUS FindIocType(FabricData_t *fabricp, IocType type, Point *pPoint)
{
	cl_map_item_t *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=cl_qmap_head(&fabricp->AllIOCs); p != cl_qmap_end(&fabricp->AllIOCs); p = cl_qmap_next(p)) {
		IocData *iocp = PARENT_STRUCT(p, IocData, AllIOCsEntry);
		if ((type == IOC_TYPE_VNIC &&
						(iocp->IocProfile.IOClass == 0x2000
						&& iocp->IocProfile.IOSubClass == 0x66a
						&& iocp->IocProfile.Protocol == 0
							// EVIC is 1, 12400 is 3
						&& (iocp->IocProfile.ProtocolVer == 1
							|| iocp->IocProfile.ProtocolVer == 3)))
			|| (type == IOC_TYPE_SRP &&
						((iocp->IocProfile.IOClass == 0xff00
							|| iocp->IocProfile.IOClass == 0x0100)
						&& iocp->IocProfile.IOSubClass == 0x609e
						&& iocp->IocProfile.Protocol == 0x108
						&& iocp->IocProfile.ProtocolVer == 1)))
		{
			status = PointListAppend(pPoint, POINT_TYPE_IOC_LIST, iocp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: IOC type Not Found: %s\n",
					   	g_Top_cmdname, (type == IOC_TYPE_VNIC)?"VNIC":"SRP");
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

// search for the Ioc corresponding to the given Ioc Guid
IocData * FindIocGuid(FabricData_t *fabricp, EUI64 guid)
{
	cl_map_item_t *mi;

	mi = cl_qmap_get(&fabricp->AllIOCs, guid);
	if (mi == cl_qmap_end(&fabricp->AllIOCs))
		return NULL;
	return PARENT_STRUCT(mi, IocData, AllIOCsEntry);
}
#endif

// search for the SystemData corresponding to the given system image Guid
SystemData * FindSystemGuid(FabricData_t *fabricp, EUI64 guid)
{
	cl_map_item_t *mi;

	mi = cl_qmap_get(&fabricp->AllSystems, guid);
	if (mi == cl_qmap_end(&fabricp->AllSystems))
		return NULL;
	return PARENT_STRUCT(mi, SystemData, AllSystemsEntry);
}

// search for the PortData corresponding to the given port rate
FSTATUS FindRate(FabricData_t *fabricp, uint32 rate, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		/* omit switch port 0, rate is often odd */
		if (portp->PortNum == 0)
			continue;
		if (portp->rate == rate) {
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Rate Not Found: %s\n",
					   	g_Top_cmdname, StlStaticRateToText(rate));
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

// search for the PortData corresponding to the given port state
FSTATUS FindPortState(FabricData_t *fabricp, IB_PORT_STATE state, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		///* omit switch port 0, state can be odd */
		//if (portp->PortNum == 0)
		//	continue;
		if (portp->PortInfo.PortStates.s.PortState == state) {
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Port State Not Found: %s\n",
					   	g_Top_cmdname, IbPortStateToText(state));
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

#ifndef __VXWORKS__
// search for ports whose ExpectedLink has the given cable label
FSTATUS FindCableLabelPat(FabricData_t *fabricp, const char* pattern, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		if (! portp->elinkp || ! portp->elinkp->CableData.label)
			continue;	// no cable information
		if (fnmatch(pattern, portp->elinkp->CableData.label, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Cable Label Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

// search for ports whose ExpectedLink has the given cable length
FSTATUS FindCableLenPat(FabricData_t *fabricp, const char* pattern, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		if (! portp->elinkp || ! portp->elinkp->CableData.length)
			continue;	// no cable information
		if (fnmatch(pattern, portp->elinkp->CableData.length, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Cable Length Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

// search for ports whose ExpectedLink has the given cable details
FSTATUS FindCableDetailsPat(FabricData_t *fabricp, const char* pattern, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		if (! portp->elinkp || ! portp->elinkp->CableData.details)
			continue;	// no cable information
		if (fnmatch(pattern, portp->elinkp->CableData.details, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Cable Details Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

// search for ports whose ExpectedLink has the given link details
FSTATUS FindLinkDetailsPat(FabricData_t *fabricp, const char* pattern, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		if (! portp->elinkp || ! portp->elinkp->details)
			continue;	// no link information
		if (fnmatch(pattern, portp->elinkp->details, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Link Details Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

// search for ports whose ExpectedLink has the given port details
FSTATUS FindPortDetailsPat(FabricData_t *fabricp, const char* pattern, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);
		PortSelector *portselp = GetPortSelector(portp);

		if (! portselp || ! portselp->details)
			continue;	// no port details information
		if (fnmatch(pattern, portselp->details, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Port Details Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}
#endif

// search for the PortData corresponding to the given port MTU
FSTATUS FindMtu(FabricData_t *fabricp, IB_MTU mtu, uint8 vl_num, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		/* omit switch port 0, mtu is often odd */
		if (portp->PortNum == 0)
			continue;
		if(MIN(portp->PortInfo.MTU.Cap, portp->neighbor->PortInfo.MTU.Cap) == mtu){
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
  			if (FSUCCESS != status)
  				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: MTU Not Found: %s\n",
					   	g_Top_cmdname, IbMTUToText(mtu));
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}

// search for the master SM (should be only 1, so we return 1st master found)
PortData * FindMasterSm(FabricData_t *fabricp)
{
	cl_map_item_t *p;

	for (p=cl_qmap_head(&fabricp->AllSMs); p != cl_qmap_end(&fabricp->AllSMs); p = cl_qmap_next(p)) {
		SMData *smp = PARENT_STRUCT(p, SMData, AllSMsEntry);
		if (smp->SMInfoRecord.SMInfo.u.s.SMStateCurrent == SM_MASTER)
			return smp->portp;
	}
	return NULL;
}

#ifndef __VXWORKS__
// search for SMData whose ExpectedSM has the given SM details
FSTATUS FindSmDetailsPat(FabricData_t *fabricp, const char* pattern, Point *pPoint)
{
	cl_map_item_t *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=cl_qmap_head(&fabricp->AllSMs); p != cl_qmap_end(&fabricp->AllSMs); p = cl_qmap_next(p)) {
		SMData *smp = PARENT_STRUCT(p, SMData, AllSMsEntry);
		if (! smp->esmp || ! smp->esmp->details)
			continue;	// no node details information
		if (fnmatch(pattern, smp->esmp->details, 0) == 0)
		{
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, smp->portp);
			if (FSUCCESS != status)
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: SM Details Not Found: %s\n",
					   	g_Top_cmdname, pattern);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}
#endif

// search for the SMData corresponding to the given PortGuid
SMData * FindSMPort(FabricData_t *fabricp, EUI64 PortGUID)
{
	cl_map_item_t *mi;

	mi = cl_qmap_get(&fabricp->AllSMs, PortGUID);
	if (mi == cl_qmap_end(&fabricp->AllSMs))
		return NULL;
	return PARENT_STRUCT(mi, SMData, AllSMsEntry);
}

// search for the PortData corresponding to the given lid and port number
// For FIs lid completely defines the port
// For Switches, lid will identify the switch and port is used to select port
PortData * FindLidPort(FabricData_t *fabricp, uint16 lid, uint8 port)
{
	PortData *portp;

	portp = FindLid(fabricp, lid);
	if (! portp)
		return NULL;
	/* lid is only valid for port 0 on switches
	 * ignore port for non-switches, in which case LID is all we need
	 */
	if (portp->PortNum == port
		|| portp->nodep->NodeInfo.NodeType != STL_NODE_SW)
		return portp;
	/* must be a port w/o a LID on a switch */
	return FindNodePort(portp->nodep, port);
}

// search for the PortData corresponding to the given nodeguid and port number
PortData *FindNodeGuidPort(FabricData_t *fabricp, EUI64 nodeguid, uint8 port)
{
	NodeData *nodep = FindNodeGuid(fabricp, nodeguid);
	if (! nodep)
		return NULL;
	return FindNodePort(nodep, port);
}

// Search through the ExpectedFIs and ExpectedSWs for an ExpectedNode with the given nodeGuid
ExpectedNode* FindExpectedNodeByNodeGuid(FabricData_t *fabricp, EUI64 nodeGuid) {
	LIST_ITEM *it;

	if(fabricp == NULL)
		return NULL;

	// First check through the FIs
	if(QListHead(&fabricp->ExpectedFIs) != NULL) {
		for(it = QListHead(&fabricp->ExpectedFIs); it != NULL; it = QListNext(&fabricp->ExpectedFIs, it)) {
			ExpectedNode* eNode = PARENT_STRUCT(it, ExpectedNode, ExpectedNodesEntry);

			if(eNode->NodeGUID == nodeGuid)
				return eNode;
		}
		
	}

	// Check through switches if it wasn't a CA
	if(QListHead(&fabricp->ExpectedSWs) != NULL) {
		for(it = QListHead(&fabricp->ExpectedSWs); it != NULL; it = QListNext(&fabricp->ExpectedSWs, it)) {
			ExpectedNode* eNode = PARENT_STRUCT(it, ExpectedNode, ExpectedNodesEntry);

			if(eNode->NodeGUID == nodeGuid)
				return eNode;
		}
		
	}

	return NULL;
}

// Search for the ExpectedLink by one side of the link with nodeGuid & portNum. 
// (OPTIONAL) Side is which portsel in the ExpectedLink that was the one given.
ExpectedLink* FindExpectedLinkByOneSide(FabricData_t *fabricp, EUI64 nodeGuid, uint8 portNum, uint8* side) {
	LIST_ITEM *it;

	if(fabricp == NULL || QListHead(&fabricp->ExpectedLinks) == NULL)
		return NULL;

	for(it = QListHead(&fabricp->ExpectedLinks); it != NULL; it = QListNext(&fabricp->ExpectedLinks, it)) {
		ExpectedLink* foundLink = PARENT_STRUCT(it, ExpectedLink, ExpectedLinksEntry);
		if(foundLink->portselp1 == NULL || foundLink->portselp2 == NULL)
			continue;

		if(foundLink->portselp1->NodeGUID == nodeGuid && foundLink->portselp1->PortNum == portNum) {
			if(side)
				*side = 1;

			return foundLink;
		} else if (foundLink->portselp2->NodeGUID == nodeGuid && foundLink->portselp2->PortNum == portNum) {
			if(side)
				*side = 2;
			
			return foundLink;
		}
	}
	
	return NULL;
}

// Search for the ExpectedLink by matching the two nodeGuids on either end of the link
ExpectedLink* FindExpectedLinkByNodeGuid(FabricData_t *fabricp, EUI64 nodeGuid1, EUI64 nodeGuid2) {
	LIST_ITEM *it;

	if(fabricp == NULL || QListHead(&fabricp->ExpectedLinks) == NULL)
		return NULL;

	for(it = QListHead(&fabricp->ExpectedLinks); it != NULL; it = QListNext(&fabricp->ExpectedLinks, it)) {
		ExpectedLink* foundLink = PARENT_STRUCT(it, ExpectedLink, ExpectedLinksEntry);
		if(foundLink->portselp1 == NULL || foundLink->portselp2 == NULL)
			continue;

		if((foundLink->portselp1->NodeGUID == nodeGuid1 && foundLink->portselp2->NodeGUID == nodeGuid2) ||
			(foundLink->portselp1->NodeGUID == nodeGuid2 && foundLink->portselp2->NodeGUID == nodeGuid1))
			return foundLink;
	}

	return NULL;
}

// Search for the ExpectedLink by matching the two portGuids on either end of the link
ExpectedLink* FindExpectedLinkByPortGuid(FabricData_t *fabricp, EUI64 portGuid1, EUI64 portGuid2) {
	LIST_ITEM *it;

	if(fabricp == NULL || QListHead(&fabricp->ExpectedLinks) == NULL)
		return NULL;

	for(it = QListHead(&fabricp->ExpectedLinks); it != NULL; it = QListNext(&fabricp->ExpectedLinks, it)) {
		ExpectedLink* foundLink = PARENT_STRUCT(it, ExpectedLink, ExpectedLinksEntry);
		if(foundLink->portselp1 == NULL || foundLink->portselp2 == NULL)
			continue;

		if((foundLink->portselp1->PortGUID == portGuid1 && foundLink->portselp2->PortGUID == portGuid2) ||
			(foundLink->portselp1->PortGUID == portGuid2 && foundLink->portselp2->PortGUID == portGuid1))
			return foundLink;
	}

	return NULL;
}

// Search for the ExpectedLink by matching the two nodeDescs on either end of the link
ExpectedLink* FindExpectedLinkByNodeDesc(FabricData_t *fabricp, char* nodeDesc1, char* nodeDesc2) {
	LIST_ITEM *it;

	if(fabricp == NULL || QListHead(&fabricp->ExpectedLinks) == NULL || nodeDesc1 == NULL || nodeDesc2 == NULL)
		return NULL;

	for(it = QListHead(&fabricp->ExpectedLinks); it != NULL; it = QListNext(&fabricp->ExpectedLinks, it)) {
		ExpectedLink* foundLink = PARENT_STRUCT(it, ExpectedLink, ExpectedLinksEntry);
		if(foundLink->portselp1 == NULL || foundLink->portselp2 == NULL ||
			foundLink->portselp1->NodeDesc == NULL || foundLink->portselp2->NodeDesc == NULL)
			continue;

		if((strncmp(foundLink->portselp1->NodeDesc, nodeDesc1, STL_NODE_DESCRIPTION_ARRAY_SIZE) &&
			strncmp(foundLink->portselp2->NodeDesc, nodeDesc2, STL_NODE_DESCRIPTION_ARRAY_SIZE)) ||
			(strncmp(foundLink->portselp1->NodeDesc, nodeDesc2, STL_NODE_DESCRIPTION_ARRAY_SIZE) &&
			strncmp(foundLink->portselp2->NodeDesc, nodeDesc1, STL_NODE_DESCRIPTION_ARRAY_SIZE)))
			return foundLink;
	}

	return NULL;
}

FSTATUS FindLinkQuality(FabricData_t *fabricp, uint16 quality, LinkQualityCompare comp, Point *pPoint)
{
	LIST_ITEM *p;
	FSTATUS status;

	ASSERT(pPoint->Type == POINT_TYPE_NONE);
	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);

		boolean match = FALSE;
		if (!portp->pPortStatus)
			continue;
		switch (comp) {
		case QUAL_EQ:
			match = (uint16)(portp->pPortStatus->lq.s.LinkQualityIndicator) == quality;
			break;
		case QUAL_GE:
			match = (uint16)(portp->pPortStatus->lq.s.LinkQualityIndicator) >= quality;
			break;
		case QUAL_LE:
			match = (uint16)(portp->pPortStatus->lq.s.LinkQualityIndicator) <= quality;
			break;
		default:
			break;
		}
		if (match) {
			status = PointListAppend(pPoint, POINT_TYPE_PORT_LIST, portp);
			if (FSUCCESS != status) 
				return status;
		}
	}
	if (pPoint->Type == POINT_TYPE_NONE) {
		fprintf(stderr, "%s: Link Quality Not Found: %d\n",
						g_Top_cmdname, quality);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
}