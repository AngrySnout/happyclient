#ifndef _CDEMO_H_
#define _CDEMO_H_

namespace cdemo
{

bool recording();

void setup(const char* filename=NULL);
void ctfinit(ucharbuf& p);
void captureinit(ucharbuf& p);
void collectinit(ucharbuf& p);

ENetPacket* packet(int chan, const ENetPacket* p); //N_POS, N_JUMPPAD, N_TELEPORT, server->client catchall
void packet(int chan, const ucharbuf& p);
void addmsghook(int type, int cn, const ucharbuf& p); //save packets that server doesn't send back
//packets that change format between client->server and server->client
void sayteam(const char* msg);
void clipboard(int plainlen, int packlen, const uchar* data);
//packets that are sent only by the server, but not acknowledged to ourself
void explodefx(int cn, int gun, int id);
void shotfx(int cn, int gun, int id, const vec& from, const vec& to);

void stop();

extern int cdemoauto;   
void keepdemo(int *keep);

}

extern string homedir;
#endif
