/**
 * @file mega/megaclient.h
 * @brief Client access engine core logic
 *
 * (c) 2013-2014 by Mega Limited, Wellsford, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#ifndef MEGACLIENT_H
#define MEGACLIENT_H 1

#include "json.h"
#include "db.h"
#include "gfx.h"
#include "filefingerprint.h"
#include "request.h"
#include "treeproc.h"
#include "sharenodekeys.h"
#include "account.h"
#include "backofftimer.h"
#include "http.h"
#include "pubkeyaction.h"

namespace mega {
extern bool debug;

class MEGA_API MegaClient
{
public:
    // own identity
    handle me;

    // root nodes (files, incoming, rubbish, mail)
    handle rootnodes[4];

    // all nodes
    node_map nodes;

    // all users
    user_map users;

    // process API requests and HTTP I/O
    void exec();

    // wait for I/O or other events
    int wait();

    // abort exponential backoff
    bool abortbackoff();

    // ID tag of the next request
    int nextreqtag();

    // corresponding ID tag of the currently executing callback
    int restag;

    // ephemeral session support
    void createephemeral();
    void resumeephemeral(handle, const byte*, int = 0);

    // full account confirmation/creation support
    void sendsignuplink(const char*, const char*, const byte*);
    void querysignuplink(const byte*, unsigned);
    void confirmsignuplink(const byte*, unsigned, uint64_t);
    void setkeypair();

    // user login: e-mail, pwkey
    void login(const char*, const byte*, bool = false);

    // check if logged in
    sessiontype_t loggedin();

    // set folder link: node, key
    error folderaccess(const char*, const char*);

    // open exported file link
    error openfilelink(const char*, int);

    // change login password
    error changepw(const byte*, const byte*);

    // load all trees: nodes, shares, contacts
    void fetchnodes();

    // retrieve user details
    void getaccountdetails(AccountDetails*, bool, bool, bool, bool, bool, bool);

    // update node attributes
    error setattr(Node*, const char** = NULL);

    // prefix and encrypt attribute json
    void makeattr(SymmCipher*, string*, const char*, int = -1) const;

    // check node access level
    int checkaccess(Node*, accesslevel_t);

    // check if a move operation would succeed
    error checkmove(Node*, Node*);

    // delete node
    error unlink(Node*);

    // move node to new parent folder
    error rename(Node*, Node*, syncdel_t = SYNCDEL_NONE);

    // start/stop/pause file transfer
    bool startxfer(direction_t, File*);
    void stopxfer(File* f);
    void pausexfers(direction_t, bool, bool = false);

    // pause flags
    bool xferpaused[2];

    // active syncs
    sync_list syncs;
    bool syncadded;

    // indicates whether all startup syncs have been fully scanned
    bool syncsup;

    // number of parallel connections per transfer (PUT/GET)
    unsigned char connections[2];

    // generate & return next upload handle
    handle uploadhandle(int);

    // add nodes to specified parent node (complete upload, copy files, make
    // folders)
    void putnodes(handle, NewNode*, int);

    // send files/folders to user
    void putnodes(const char*, NewNode*, int);

    // attach file attribute to upload or node handle
    void putfa(handle, fatype, SymmCipher*, string*);

    // queue file attribute retrieval
    error getfa(Node*, fatype, int = 0);

    // attach/update/delete user attribute
    void putua(const char*, const byte* = NULL, unsigned = 0, int = 0);

    // queue user attribute retrieval
    void getua(User*, const char* = NULL, int = 0);

    // add new contact (by e-mail address)
    error invite(const char*, visibility_t = VISIBLE);

    // add/remove/update outgoing share
    void setshare(Node*, const char*, accesslevel_t);

    // export node link or remove existing exported link for this node
    error exportnode(Node*, int);

    // add/delete sync
    error addsync(string*, const char*, string*, Node*, int);
    void delsync(Sync*);

    // close all open HTTP connections
    void disconnect();

    // abort session and free all state information
    void logout();

    // maximum outbound throughput (per target server)
    int putmbpscap;

    // shopping basket
    handle_vector purchase_basket;

    // enumerate Pro account purchase options
    void purchase_enumeratequotaitems();

    // clear shopping basket
    void purchase_begin();

    // add item to basket
    void purchase_additem(int, handle, unsigned, char*, unsigned, char*, char*);

    // submit purchased products for payment
    void purchase_checkout(int);

    // toggle global debug flag
    bool toggledebug();

private:
    // API request queue double buffering:
    // reqs[r] is open for adding commands
    // reqs[r^1] is being processed on the API server
    HttpReq* pendingcs;
    BackoffTimer btcs;

    // server-client command trigger connection
    HttpReq* pendingsc;
    BackoffTimer btsc;

    // badhost report
    HttpReq* badhostcs;

    // root URL for API requestrs
    static const char* const APIURL;

    // notify URL for new server-client commands
    string scnotifyurl;

    // unique request ID
    char reqid[10];

    // auth URI component for API requests
    string auth;

    // API response JSON object
    JSON response;

    // response record processing issue
    bool warned;

    // next local user record identifier to use
    int userid;

    // pending file attribute writes
    putfa_list newfa;

    // current attribute being sent
    putfa_list::iterator curfa;
    BackoffTimer btpfa;

    // pending file attribute reads, grouped by queued file attribute
    // retrievals
    fareq_set fareqs;

    // next internal upload handle
    handle nextuh;

    // User-Agent header for HTTP requests
    string useragent;

    // maximum number of concurrent transfers
    static const unsigned MAXTRANSFERS = 8;

    // determine if more transfers fit in the pipeline
    bool moretransfers(direction_t);

    // update time at which next deferred transfer retry kicks in
    void nexttransferretry(direction_t d, dstime*);

    // a TransferSlot chunk failed
    bool chunkfailed;
    
    // fetch state serialize from local cache
    bool fetchsc(DbTable*);

    // server-client command processing
    void sc_updatenode();
    void sc_deltree();
    void sc_newnodes();
    void sc_contacts();
    void sc_keys();
    void sc_fileattr();
    void sc_userattr();
    bool sc_shares();

    void init();

    // add node to vector and return index
    unsigned addnode(node_vector*, Node*) const;

    // add child for consideration in syncup()/syncdown()
    void addchild(remotenode_map*, string*, Node*, list<string>*) const;

    // crypto request response
    void cr_response(node_vector*, node_vector*, JSON*);

    // read node tree from JSON object
    void readtree(JSON*);

    // used by wait() to handle event timing
    void checkevent(dstime, dstime*, dstime*);

    // converts UTF-8 to 32-bit word array
    static char* str_to_a32(const char*, int*);

    // last successful interaction with the Internet
    dstime noinetds;

    // was the app notified of a retrying CS request?
    bool csretrying;

public:
    // application callbacks
    struct MegaApp* app;

    // event waiter
    Waiter* waiter;

    // HTTP access
    HttpIO* httpio;

    // directory change notification
    struct FileSystemAccess* fsaccess;

    // bitmap graphics handling
    GfxProc* gfx;
    
    // DB access
    DbAccess* dbaccess;

    // state cache table for logged in user
    DbTable* sctable;

    // scsn as read from sctable
    handle cachedscsn;

    // have we just completed fetching new nodes?
    bool statecurrent;

    // record type indicator for sctable
    enum { CACHEDSCSN, CACHEDNODE, CACHEDUSER, CACHEDLOCALNODE } sctablerectype;

    // initialize/update state cache referenced sctable
    void initsc();
    void updatesc();
    void finalizesc(bool);

    // MegaClient-Server response JSON
    JSON json;

    // Server-MegaClient request JSON
    JSON jsonsc;

    // no two interrelated client instances should ever have the same sessionid
    char sessionid[10];

    // application key
    char appkey[16];

    // incoming shares to be attached to a corresponding node
    newshare_list newshares;

    // current request tag
    int reqtag;

    // user maps: by handle and by case-normalized e-mail address
    uh_map uhindex;
    um_map umindex;

    // pending file attributes
    fa_map pendingfa;

    // file attribute fetch channels
    fafc_map fafcs;

    // file attribute fetches
    faf_map fafs;

    // generate attribute string based on the pending attributes for this upload
    void pendingattrstring(handle, string*);

    // merge newly received share into nodes
    void mergenewshares(bool);

    // transfer queues (PUT/GET)
    transfer_map transfers[2];

    // transfer tslots
    transferslot_list tslots;

    // next TransferSlot to doio() on
    transferslot_list::iterator slotit;

    // FileFingerprint to node mapping
    fingerprint_set fingerprints;

    // asymmetric to symmetric key rewriting
    handle_vector nodekeyrewrite;
    handle_vector sharekeyrewrite;

    static const char* const EXPORTEDLINK;

    // minimum number of bytes in transit for upload/download pipelining
    static const int MINPIPELINE = 65536;

    // initial state load in progress?
    bool fetchingnodes;

    // server-client request sequence number
    char scsn[12];

    bool setscsn(JSON*);

    void purgenodes(node_vector* = NULL);
    void purgeusers(user_vector* = NULL);
    bool readusers(JSON*);

    user_vector usernotify;
    void notifyuser(User*);

    node_vector nodenotify;
    void notifynode(Node*);

    // write changed/added/deleted users to the DB cache and notify the
    // application
    void notifypurge();

    // remove node subtree
    void deltree(handle);

    Node* nodebyhandle(handle);
    Node* nodebyfingerprint(FileFingerprint*);

    // generate & return upload handle
    handle getuploadhandle();

    // sync debris folder name in //bin
    static const char* const SYNCDEBRISFOLDERNAME;

    // we are adding the //bin/SyncDebris/yyyy-mm-dd subfolder(s)
    bool syncdebrisadding;

    // activity flag
    bool syncactivity;

    // app scanstate flag
    bool syncscanstate;

    // block local fs updates processing while locked ops are in progress
    bool syncfsopsfailed;

    // retry accessing temporarily locked filesystem items
    bool syncfslockretry;
    BackoffTimer syncfslockretrybt;

    // retry of transiently failed local filesystem ops
    bool syncdownretry;
    BackoffTimer syncdownbt;

    // sync PUT Nagle timer
    bool syncnagleretry;
    BackoffTimer syncnaglebt;

    // rescan timer if fs notification unavailable or broken
    bool syncscanfailed;
    BackoffTimer syncscanbt;

    // vanished from a local synced folder
    localnode_set localsyncnotseen;

    // maps local fsid to corresponding LocalNode*
    handlelocalnode_map fsidnode;

    // local nodes that need to be added remotely
    localnode_vector synccreate;

    // number of sync-initiated putnodes() in progress
    int syncadding;

    // sync id dispatch
    handle nextsyncid();
    handle currsyncid;

    // SyncDebris folder addition result
    void putnodes_syncdebris_result(error, NewNode*);

    // if no sync putnodes operation is in progress, apply the updates stored
    // in syncadded/syncdeleted/syncoverwritten to the remote tree
    void syncupdate();

    // create missing folders, copy/start uploading missing files
    void syncup(LocalNode*, dstime*);

    // sync putnodes() completion
    void putnodes_sync_result(error, NewNode*);

    // start downloading/copy missing files, create missing directories
    bool syncdown(LocalNode*, string*, bool);

    // move nodes to //bin/SyncDebris/yyyy-mm-dd/
    void movetosyncdebris(Node*);
    void execmovetosyncdebris();
    node_set todebris;

    // recursively cancel transfers in a subtree
    void stopxfers(LocalNode*);

    // update paths of all PUT transfers
    void updateputs();

    // determine if all transfer slots are full
    bool slotavail() const;

    // dispatch as many queued transfers as possible
    void dispatchmore(direction_t);

    // transfer queue dispatch/retry handling
    bool dispatch(direction_t);

    void defer(direction_t, int td, int = 0);
    void freeq(direction_t);

    dstime transferretrydelay();

    // active request buffer
    int r;

    // client-server request double-buffering
    Request reqs[2];

    // upload handle -> node handle map (filled by upload completion)
    handlepair_set uhnh;

    // file attribute fetch failed
    void faf_failed(int);

    // transfer chunk failed
    void setchunkfailed(string*);
    string badhosts;
    
    // process object arrays by the API server
    int readnodes(JSON*, int, putsource_t = PUTNODES_APP, NewNode* = NULL, int = 0);

    void readok(JSON*);
    void readokelement(JSON*);
    void readoutshares(JSON*);
    void readoutshareelement(JSON*);

    void readcr();
    void readsr();

    void procsnk(JSON*);
    void procsuk(JSON*);

    void setkey(SymmCipher*, const char*);
    bool decryptkey(const char*, byte*, int, SymmCipher*, int, handle);

    void handleauth(handle, byte*);

    void procsc();

    // API warnings
    void warn(const char*);
    bool warnlevel();

    Node* childnodebyname(Node*, const char*);

    // purge account state and abort server-client connection
    void purgenodesusersabortsc();

    static const int USERHANDLE = 8;
    static const int NODEHANDLE = 6;

    // session ID length (binary)
    static const unsigned SIDLEN = 2 * SymmCipher::KEYLENGTH + USERHANDLE * 4 / 3 + 1;

    void proccr(JSON*);
    void procsr(JSON*);

    // account access: master key
    // folder link access: folder key
    SymmCipher key;

    // account access (full account): RSA key
    AsymmCipher asymkey;

    // apply keys
    int applykeys();

    // symmetric password challenge
    int checktsid(byte* sidbuf, unsigned len);

    // locate user by e-mail address or by handle
    User* finduser(const char*, int = 0);
    User* finduser(handle, int = 0);
    void mapuser(handle, const char*);

    // queue public key request for user
    void queuepubkeyreq(User*, PubKeyAction*);

    // simple string hash
    static void stringhash(const char*, byte*, SymmCipher*);
    static uint64_t stringhash64(string*, SymmCipher*);

    // set authentication context, either a session ID or a exported folder
    // node handle
    void setsid(const byte*, unsigned);
    void setrootnode(handle);

    // process node subtree
    void proctree(Node*, TreeProc*);

    // hash password
    error pw_key(const char*, byte*) const;

    // convert hex digit to number
    static int hexval(char);

    MegaClient(MegaApp*, Waiter*, HttpIO*, FileSystemAccess*, DbAccess*, GfxProc*, const char*, const char*);
    ~MegaClient();
};
} // namespace

#endif
