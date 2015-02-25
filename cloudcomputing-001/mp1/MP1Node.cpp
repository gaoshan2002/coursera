/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

#include <set>

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

static const int CFG_GOSSIP_B = 8;
static const int CFG_GOSSIP_ENTRY_EXPIRATION = 10;

int addr2id(Address& addr)
{
	return *(int*)(&addr.addr);
}
int addr2port(Address& addr)
{
	return *(short*)(&addr.addr[4]);
}
Address getAddr(int id, int port)
{
	Address addr;
	memcpy(&addr.addr[0], &id, sizeof(int));
	memcpy(&addr.addr[4], &port, sizeof(short));
	return addr;
}

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
//	int id = getId();
//	int port = getPort();

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	JOINREQMsg* msg = new JOINREQMsg();
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
    	// hdr + this->addr + this->hb
//        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
//        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        msg->addr = memberNode->addr;
        msg->heartbeat = memberNode->heartbeat;
//        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
//        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, sizeof(JOINREQMsg));

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
	return 1; // 1 as OK?
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    memberNode->heartbeat++;

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size )
{
	MessageHdr* msg = (MessageHdr*)data;
	switch (msg->msgType) {
	case JOINREQ:
		onRcvJOINREQ((JOINREQMsg*)msg);
		break;
	case JOINRSP:
		onRcvJOINRSP((JOINRSPMsg*)msg);
		break;
	case HEARTBEAT:
		onRcvHEARTBEAT((HEARTBEATMsg*)msg);
//		updateMemberList(((HEARTBEATMsg*)msg)->memberList);
		break;
	default:
		printf("unknown msg type: %d\n", msg->msgType);
		break;
	}
	return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */

	removeExpiredMembers();
	doGossip();
    return;
}

void MP1Node::removeExpiredMembers()
{
	for (auto i = 0u; i < memberNode->memberList.size(); i++) {
		auto& e = memberNode->memberList[i];
		if (par->getcurrtime() - e.timestamp > CFG_GOSSIP_ENTRY_EXPIRATION) {
			auto addr = getAddr(e.id, e.port);
			log->LOG(&memberNode->addr, "removing %s w/ hb %d ts:%d", addr.getAddress().c_str(), e.heartbeat, e.port);
			auto a = getAddr(e.id, e.port);
			log->logNodeRemove(&memberNode->addr, &a);
			memberNode->memberList.erase(memberNode->memberList.begin() + i);
			i--;
		}
	}
}

void MP1Node::doGossip()
{
	std::set<int> gossiped_members;

	for (int i = 0; gossiped_members.size() < memberNode->memberList.size() && gossiped_members.size() < CFG_GOSSIP_B; i++) {
//		auto& e = memberNode->memberList[i%memberNode->memberList.size()];
		if (gossiped_members.find(i%memberNode->memberList.size()) != gossiped_members.end()) {
			continue;
		}
		if (rand()*1.0/RAND_MAX < CFG_GOSSIP_B / (par->MAX_NNB*1.0)) {
			gossiped_members.insert(i%memberNode->memberList.size());
		}
	}

	for (auto i = gossiped_members.begin(); i != gossiped_members.end(); i++) {
		auto& e = memberNode->memberList[*i];
		Address target = getAddr(e.id, e.port);
		HEARTBEATMsg* hb = new HEARTBEATMsg();
		hb->addr = memberNode->addr;
		hb->heartbeat = memberNode->heartbeat;
		hb->memberList = memberNode->memberList;
		log->LOG(&memberNode->addr, "sending hb [%d] to %s", hb->heartbeat, target.getAddress().c_str());
		emulNet->ENsend(&memberNode->addr, &target, (char*)hb, sizeof(HEARTBEATMsg));
	}
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    

}

int MP1Node::getId()
{
	return addr2id(memberNode->addr);
//	return *(int*)(&memberNode->addr.addr);
}

int MP1Node::getPort()
{
	return addr2port(memberNode->addr);
//	return *(short*)(&memberNode->addr.addr[4]);
}

MemberListEntry* MP1Node::findMemberEntry(std::vector<MemberListEntry>& lst, int id)
{
	for (auto i = lst.begin(); i < lst.end(); i++) {
		if ((*i).id == id) {
			return &(*i);
		}
	}
	return NULL;
}
void MP1Node::onRcvJOINREQ(JOINREQMsg* msg)
{
	log->logNodeAdd(&memberNode->addr, &msg->addr);
//	log->LOG(&memberNode->addr, "rcvd JOINREQ from : %s @ %d", msg->addr.getAddress().c_str(), par->getcurrtime());

	auto e = findMemberEntry(memberNode->memberList, addr2id(msg->addr));
	if (!e) {
		MemberListEntry newentry(addr2id(msg->addr), addr2port(msg->addr), msg->heartbeat, par->getcurrtime());
		memberNode->memberList.push_back(newentry);
	} else {
		log->LOG(&memberNode->addr, " duplicated!");
	}

	JOINRSPMsg* rsp = new JOINRSPMsg();
	rsp->addr = memberNode->addr;
	rsp->heartbeat = memberNode->heartbeat;
	rsp->memberList = memberNode->memberList;

    emulNet->ENsend(&memberNode->addr, &msg->addr, (char*)rsp, sizeof(JOINRSPMsg));
}

void MP1Node::updateMemberList(vector<MemberListEntry>& memberList)
{
	for (auto i = memberList.begin(); i != memberList.end(); i++) {
		auto& newe = *i;
		if (newe.timestamp < par->getcurrtime() - CFG_GOSSIP_ENTRY_EXPIRATION) {
			continue;
		}
		if (newe.id == addr2id(memberNode->addr)) {
			continue;
		}
		auto e = findMemberEntry(memberNode->memberList, newe.id);

		if (!e) {
			memberNode->memberList.push_back(newe);
			auto newe_addr = getAddr(newe.id, newe.port);
			log->logNodeAdd(&memberNode->addr, &newe_addr);
		} else if (newe.heartbeat > e->heartbeat){
			e->heartbeat = newe.heartbeat;
			e->timestamp = par->getcurrtime();
		}
	}
}

void MP1Node::onRcvJOINRSP(JOINRSPMsg* msg)
{
	memberNode->inGroup = true;
	onRcvHEARTBEAT(msg);
}

void MP1Node::onRcvHEARTBEAT(HEARTBEATMsg* msg)
{
	auto e = findMemberEntry(memberNode->memberList, addr2id(msg->addr));
	log->LOG(&memberNode->addr, "got hb from %s w/ hb: %d ts: %d, old hb: %d"
							, msg->addr.getAddress().c_str(), msg->heartbeat, par->getcurrtime(), e?e->heartbeat:-1);

	if (!e) {
		MemberListEntry newentry(addr2id(msg->addr), addr2port(msg->addr), msg->heartbeat, par->getcurrtime());
		msg->memberList.push_back(newentry);
		auto newe_addr = getAddr(newentry.id, newentry.port);
		log->logNodeAdd(&memberNode->addr, &newe_addr);
	} else if (msg->heartbeat >= e->heartbeat){
		e->heartbeat = msg->heartbeat;
		e->timestamp = par->getcurrtime();
	}
	updateMemberList(msg->memberList);
}

